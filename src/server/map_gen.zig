const std = @import("std");
const Random = std.rand.Random;
const Allocator = std.mem.Allocator;
const ArrayList = std.ArrayList;

const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const makeRect = core.geometry.makeRect;
const CardinalDirection = core.geometry.CardinalDirection;

const NewGameSettings = core.protocol.NewGameSettings;
const Species = core.protocol.Species;
const TerrainSpace = core.protocol.TerrainSpace;
const Wall = core.protocol.Wall;

const game_model = @import("./game_model.zig");
const GameState = game_model.GameState;
const Individual = game_model.Individual;
const Item = game_model.Item;
const Terrain = game_model.Terrain;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

/// overwrites terrain. populates individuals.
pub fn generate(game_state: *GameState, new_game_settings: NewGameSettings) !void {
    switch (new_game_settings) {
        .regular => |data| return generateRegular(game_state, data.seed),
        .puzzle_levels => return generatePuzzleLevels(game_state),
    }
}

const MapGenerator = struct {
    // Convenience pointers to game_state fields
    allocator: Allocator,
    terrain: *Terrain,
    individuals: *IdMap(*Individual),
    items: *IdMap(*Item),

    // Our state
    r: std.rand.Random,
    next_id: u32,
    warp_points_list: ArrayList(Coord),
};

pub fn generateRegular(game_state: *GameState, specified_seed: ?u64) !void {
    const random_seed: u64 = specified_seed orelse s: {
        var buf: [8]u8 = undefined;
        std.options.cryptoRandomSeed(&buf);
        break :s @bitCast(buf);
    };
    var _r = std.rand.DefaultPrng.init(random_seed);

    var _mg = MapGenerator{
        .allocator = game_state.allocator,
        .terrain = &game_state.terrain,
        .individuals = &game_state.individuals,
        .items = &game_state.items,

        .r = _r.random(),
        .next_id = 2, // the player character is 1.
        .warp_points_list = ArrayList(Coord).init(game_state.allocator),
    };
    const mg = &_mg;

    const last_mine_room = try generateMine(mg);
    const forest_rect = try generateForest(mg, last_mine_room);
    try generateDesert(mg, forest_rect);

    game_state.warp_points = try mg.warp_points_list.toOwnedSlice();
}

fn generateMine(mg: *MapGenerator) !Rect {
    // orcish mines
    {
        var rooms = ArrayList(Rect).init(mg.allocator);
        // TODO: this design:
        //  start ▓▓▓
        //   orcs ▓▓▓
        // dagger ▓▓▓----▓▓▓ rats
        //               ▓▓▓
        //               ▓▓▓  ▓▓▓ boss
        //            door|   ▓▓▓ orcs
        //                |   ▓▓▓ wolfs
        //              |--    |
        //              |      |    |---- exit
        //             ▓▓▓    ▓▓▓   |
        // rats ▓▓▓▓---▓▓▓----▓▓▓----
        // orcs ▓▓▓▓   ▓▓▓    ▓▓▓ door
        //            wolf    orcs
        //            orcs

        var rooms_remaining: usize = 7;
        while (rooms_remaining > 0) : (rooms_remaining -= 1) {
            const size = makeCoord(
                mg.r.intRangeAtMost(i32, 5, 9),
                mg.r.intRangeAtMost(i32, 5, 9),
            );
            const position = makeCoord(
                mg.r.intRangeAtMost(i32, mg.terrain.metrics.min_x - 5 - size.x, mg.terrain.metrics.max_x + 5),
                mg.r.intRangeAtMost(i32, mg.terrain.metrics.min_y - 5 - size.y, mg.terrain.metrics.max_y + 5),
            );
            const room_rect = makeRect(position, size);
            try rooms.append(room_rect);

            var y = position.y;
            while (y <= room_rect.bottom()) : (y += 1) {
                var x = position.x;
                while (x <= room_rect.right()) : (x += 1) {
                    const cell_ptr = try mg.terrain.getOrPut(x, y);
                    if (cell_ptr.wall == .air) {
                        // already open
                        continue;
                    }
                    if (x == position.x or y == position.y or x == room_rect.right() or y == room_rect.bottom()) {
                        cell_ptr.* = TerrainSpace{
                            .floor = .dirt,
                            .wall = .dirt,
                        };
                    } else {
                        cell_ptr.* = TerrainSpace{
                            .floor = .dirt,
                            .wall = .air,
                        };
                    }
                }
            }
        }

        // fill rooms with individuals
        var rooms_for_spawn = try clone(mg.allocator, rooms);
        var possible_spawn_locations = ArrayList(Coord).init(mg.allocator);
        while (rooms_for_spawn.items.len > 1) {
            const room = popRandom(mg.r, &rooms_for_spawn).?;
            possible_spawn_locations.shrinkRetainingCapacity(0);
            var it = (Rect{
                .x = room.x + 1,
                .y = room.y + 1,
                .width = room.width - 2,
                .height = room.height - 2,
            }).rowMajorIterator();
            while (it.next()) |coord| {
                try possible_spawn_locations.append(coord);
            }

            var orc_count: usize = 1;
            var wolf_count: usize = 0;
            var rat_count: usize = 0;
            var boss_room = false;
            switch (rooms_for_spawn.items.len) {
                0 => unreachable, // human starting room.
                1 => {
                    // boss room
                    orc_count = 4;
                    wolf_count = 2;
                    boss_room = true;
                },
                2...3 => {
                    // wolf room
                    orc_count = mg.r.intRangeAtMost(usize, 2, 3);
                    wolf_count = 1;
                },
                4...999 => {
                    // rat room
                    orc_count = 2;
                    rat_count = 7;
                },
                else => unreachable,
            }
            var individuals_remaining = orc_count;
            while (individuals_remaining > 0) : (individuals_remaining -= 1) {
                const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
                const orc_id = mg.next_id;
                try mg.individuals.putNoClobber(orc_id, try makeIndividual(coord, .orc).clone(mg.allocator));
                mg.next_id += 1;
                if (boss_room and individuals_remaining == 1) {
                    // give the boss a shield
                    try mg.items.putNoClobber(mg.next_id, try (Item{
                        .location = .{ .held = .{ .holder_id = orc_id, .equipped_to_slot = .right_hand } },
                        .kind = .shield,
                    }).clone(mg.allocator));
                    mg.next_id += 1;
                }
            }
            individuals_remaining = wolf_count;
            while (individuals_remaining > 0) : (individuals_remaining -= 1) {
                const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
                try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .wolf).clone(mg.allocator));
                mg.next_id += 1;
            }
            individuals_remaining = rat_count;
            // All the rats in a room use an arbitrary but matching origin coordinate to make caching their ai decision more efficient.
            const rat_home = popRandom(mg.r, &possible_spawn_locations) orelse undefined;
            while (individuals_remaining > 0) : (individuals_remaining -= 1) {
                const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
                const rat = try makeIndividual(coord, .rat).clone(mg.allocator);
                rat.perceived_origin = rat_home;
                try mg.individuals.putNoClobber(mg.next_id, rat);
                mg.next_id += 1;
            }
        }

        const start_point = makeCoord(
            mg.r.intRangeLessThan(i32, rooms_for_spawn.items[0].x + 1, rooms_for_spawn.items[0].right() - 1),
            mg.r.intRangeLessThan(i32, rooms_for_spawn.items[0].y + 1, rooms_for_spawn.items[0].bottom() - 1),
        );
        const human = try makeIndividual(start_point, .human).clone(mg.allocator);
        try mg.individuals.putNoClobber(1, human);
        try mg.warp_points_list.append(start_point);
        // It's dangerous to go alone. Take this.
        try mg.items.putNoClobber(mg.next_id, try (Item{
            .location = .{ .floor_coord = start_point },
            .kind = .dagger,
        }).clone(mg.allocator));
        mg.next_id += 1;

        // join rooms
        var rooms_to_join = try clone(mg.allocator, rooms);
        while (rooms_to_join.items.len > 1) {
            const room_a = popRandom(mg.r, &rooms_to_join).?;
            const room_b = choice(mg.r, rooms_to_join.items);

            var cursor = makeCoord(
                mg.r.intRangeAtMost(i32, room_a.x + 1, room_a.right() - 1),
                mg.r.intRangeAtMost(i32, room_a.y + 1, room_a.bottom() - 1),
            );
            var dest = makeCoord(
                mg.r.intRangeAtMost(i32, room_b.x + 1, room_b.right() - 1),
                mg.r.intRangeAtMost(i32, room_b.y + 1, room_b.bottom() - 1),
            );
            const unit_diagonal = dest.minus(cursor).signumed();
            while (!cursor.equals(dest)) : ({
                // derpizontal, and then derpicle
                if (cursor.x != dest.x) {
                    cursor.x += unit_diagonal.x;
                } else {
                    cursor.y += unit_diagonal.y;
                }
            }) {
                {
                    const cell_ptr = try mg.terrain.getOrPutCoord(cursor);
                    if (cell_ptr.wall == .air) {
                        // already open
                        continue;
                    }
                    cell_ptr.* = TerrainSpace{
                        .floor = .dirt,
                        .wall = .air,
                    };
                }
                // Surround with dirt walls
                for ([_]Coord{
                    cursor.plus(makeCoord(-1, -1)),
                    cursor.plus(makeCoord(0, -1)),
                    cursor.plus(makeCoord(1, -1)),
                    cursor.plus(makeCoord(-1, 0)),
                    cursor.plus(makeCoord(1, 0)),
                    cursor.plus(makeCoord(-1, 1)),
                    cursor.plus(makeCoord(0, 1)),
                    cursor.plus(makeCoord(1, 1)),
                }) |wall_cursor| {
                    const cell_ptr = try mg.terrain.getOrPutCoord(wall_cursor);
                    if (cell_ptr.wall == .air) {
                        continue;
                    }
                    cell_ptr.* = TerrainSpace{
                        .floor = .dirt,
                        .wall = .dirt,
                    };
                }
            }
        }

        return rooms_to_join.items[0];
    }
}

fn generateForest(mg: *MapGenerator, last_mine_room: Rect) !Rect {
    // forest
    var forest_rect: Rect = undefined;
    {
        // path to forest
        var cursor = makeCoord(
            mg.r.intRangeAtMost(i32, last_mine_room.x + 1, last_mine_room.right() - 1),
            mg.r.intRangeAtMost(i32, last_mine_room.y + 1, last_mine_room.bottom() - 1),
        );
        const forest_start_x = mg.terrain.metrics.max_x + 3;
        while (cursor.x < forest_start_x) : (cursor.x += 1) {
            {
                const cell_ptr = try mg.terrain.getOrPutCoord(cursor);
                if (cell_ptr.wall == .air) {
                    // already open
                    continue;
                }
                cell_ptr.* = TerrainSpace{
                    .floor = .dirt,
                    .wall = .air,
                };
            }

            // Surround with dirt walls
            for ([_]Coord{
                cursor.plus(makeCoord(-1, -1)),
                cursor.plus(makeCoord(0, -1)),
                cursor.plus(makeCoord(1, -1)),
                cursor.plus(makeCoord(-1, 0)),
                cursor.plus(makeCoord(1, 0)),
                cursor.plus(makeCoord(-1, 1)),
                cursor.plus(makeCoord(0, 1)),
                cursor.plus(makeCoord(1, 1)),
            }) |wall_cursor| {
                const cell_ptr = try mg.terrain.getOrPutCoord(wall_cursor);
                if (cell_ptr.wall == .air) {
                    continue;
                }
                cell_ptr.* = TerrainSpace{
                    .floor = .dirt,
                    .wall = .dirt,
                };
            }
        }

        // we've arrived at the forest
        try mg.warp_points_list.append(makeCoord(cursor.x - 1, cursor.y));
        forest_rect = Rect{
            .x = cursor.x - 1,
            .y = cursor.y - mg.r.intRangeAtMost(i32, 20, 30),
            .width = mg.r.intRangeAtMost(i32, 40, 60),
            .height = mg.r.intRangeAtMost(i32, 40, 60),
        };
        // dig out the forest
        var y = forest_rect.y;
        while (y <= forest_rect.bottom()) : (y += 1) {
            var x = forest_rect.x;
            while (x <= forest_rect.right()) : (x += 1) {
                const cell_ptr = try mg.terrain.getOrPut(x, y);
                if (cell_ptr.wall == .air) {
                    // the one space of path that enters the forest
                    continue;
                }
                if (x == forest_rect.x or y == forest_rect.y or x == forest_rect.right() or y == forest_rect.bottom()) {
                    cell_ptr.* = TerrainSpace{
                        .floor = .dirt,
                        .wall = .dirt,
                    };
                } else {
                    cell_ptr.* = TerrainSpace{
                        .floor = .grass,
                        .wall = .air,
                    };
                }
            }
        }
    }
    // fill the forest with foliage.
    {
        const pool_rect = Rect{
            .x = forest_rect.x + @divTrunc(forest_rect.width, 2) - 2,
            .y = forest_rect.y + @divTrunc(forest_rect.height, 2) - 2,
            .width = 5,
            .height = 5,
        };
        var it = pool_rect.rowMajorIterator();
        while (it.next()) |coord| {
            if (coord.y == pool_rect.y) {
                if (coord.x == pool_rect.x) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_northwest;
                } else if (coord.x == pool_rect.right() - 1) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_northeast;
                } else {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_north;
                }
            } else if (coord.y == pool_rect.bottom() - 1) {
                if (coord.x == pool_rect.x) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_southwest;
                } else if (coord.x == pool_rect.right() - 1) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_southeast;
                } else {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_south;
                }
            } else {
                if (coord.x == pool_rect.x) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_west;
                } else if (coord.x == pool_rect.right() - 1) {
                    mg.terrain.getExistingCoord(coord).floor = .grass_and_water_edge_east;
                } else {
                    mg.terrain.getExistingCoord(coord).floor = .water;
                }
            }
        }
        // boss starts in the middle of the pool
        {
            const boss_id = mg.next_id;
            const coord = pool_rect.position().plus(pool_rect.size().scaledDivTrunc(2));
            try mg.individuals.putNoClobber(boss_id, try makeIndividual(coord, Species{ .centaur = .warrior }).clone(mg.allocator));
            mg.next_id += 1;
        }

        var possible_spawn_locations = ArrayList(Coord).init(mg.allocator);
        it = (Rect{
            .x = forest_rect.x + 2,
            .y = forest_rect.y + 2,
            .width = forest_rect.width - 4,
            .height = forest_rect.height - 4,
        }).rowMajorIterator();
        while (it.next()) |coord| {
            const cell_ptr = mg.terrain.getExistingCoord(coord);
            if (!(cell_ptr.wall == .air and cell_ptr.floor != .water)) continue;
            switch (mg.r.int(u4)) {
                0...10 => {},
                11...13 => {
                    const ne_ptr = mg.terrain.getExisting(coord.x + 1, coord.y + 0);
                    const sw_ptr = mg.terrain.getExisting(coord.x + 0, coord.y + 1);
                    const se_ptr = mg.terrain.getExisting(coord.x + 1, coord.y + 1);
                    if (ne_ptr.wall == .air and ne_ptr.floor != .water and //
                        sw_ptr.wall == .air and sw_ptr.floor != .water and //
                        se_ptr.wall == .air and se_ptr.floor != .water)
                    {
                        cell_ptr.wall = .tree_northwest;
                        ne_ptr.wall = .tree_northeast;
                        sw_ptr.wall = .tree_southwest;
                        se_ptr.wall = .tree_southeast;
                    }
                },
                14...15 => {
                    cell_ptr.wall = .bush;
                },
            }
            if (cell_ptr.wall == .air) {
                try possible_spawn_locations.append(coord);
            }
        }
        // spawn some woodland creatures.
        var individuals_remaining = mg.r.intRangeAtMost(usize, 4, 10);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, Species{ .centaur = .archer }).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = mg.r.intRangeAtMost(usize, 1, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .wood_golem).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = mg.r.intRangeAtMost(usize, 1, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .kangaroo).clone(mg.allocator));
            mg.next_id += 1;
        }
    }
    return forest_rect;
}

fn generateDesert(mg: *MapGenerator, forest_rect: Rect) !void {
    var desert_rect: Rect = undefined;
    {
        const desert_margin_min = 5;
        const desert_margin_max = 25;
        const room_size_min = 5;
        const room_size_max = 8;
        // x
        desert_rect.x = forest_rect.right() + 1;
        var building_rect: Rect = undefined;
        building_rect.x = desert_rect.x + mg.r.intRangeAtMost(i32, desert_margin_min, desert_margin_max);
        const west_divider_x = building_rect.x + mg.r.intRangeAtMost(i32, room_size_min, room_size_max);
        const main_hall_end = Coord{
            .x = west_divider_x + mg.r.intRangeAtMost(i32, room_size_min, room_size_max),
            .y = mg.r.intRangeAtMost(i32, forest_rect.y + 1, forest_rect.bottom() - 2),
        };
        const east_divider_x = main_hall_end.x + mg.r.intRangeAtMost(i32, room_size_min, room_size_max);
        building_rect.width = east_divider_x + mg.r.intRangeAtMost(i32, room_size_min, room_size_max) - building_rect.x;
        desert_rect.width = building_rect.right() + mg.r.intRangeAtMost(i32, desert_margin_min, desert_margin_max) - desert_rect.x;
        // y
        const north_divider_y = main_hall_end.y - 1 - mg.r.intRangeAtMost(i32, room_size_min, room_size_max);
        building_rect.y = north_divider_y - mg.r.intRangeAtMost(i32, room_size_min, room_size_max);
        desert_rect.y = building_rect.y - mg.r.intRangeAtMost(i32, desert_margin_min, desert_margin_max);
        const south_divider_y = main_hall_end.y + 1 + mg.r.intRangeAtMost(i32, room_size_min, room_size_max);
        building_rect.height = south_divider_y + mg.r.intRangeAtMost(i32, room_size_min, room_size_max) - building_rect.y;
        desert_rect.height = building_rect.bottom() + mg.r.intRangeAtMost(i32, desert_margin_min, desert_margin_max) - desert_rect.y;

        // path to desert
        const opening = makeCoord(
            forest_rect.right() + 1,
            mg.r.intRangeAtMost(i32, @max(forest_rect.y, desert_rect.y) + 1, @min(forest_rect.bottom(), desert_rect.bottom()) - 1),
        );
        try mg.terrain.put(opening.x - 1, opening.y, TerrainSpace{
            .floor = .dirt,
            .wall = .air,
        });
        try mg.terrain.putCoord(opening, TerrainSpace{
            .floor = .dirt,
            .wall = .air,
        });
        try mg.warp_points_list.append(opening);
        // Throw an item on the ground here for debugging.
        try mg.items.putNoClobber(mg.next_id, try (Item{
            .location = .{ .floor_coord = opening },
            .kind = .torch,
        }).clone(mg.allocator));
        mg.next_id += 1;

        // dig out the desert
        {
            var y = desert_rect.y;
            while (y <= desert_rect.bottom()) : (y += 1) {
                var x = desert_rect.x;
                while (x <= desert_rect.right()) : (x += 1) {
                    const cell_ptr = try mg.terrain.getOrPut(x, y);
                    if (cell_ptr.wall == .air) {
                        // the one space of path that enters the forest
                        continue;
                    }
                    if (x == desert_rect.x or y == desert_rect.y or x == desert_rect.right() or y == desert_rect.bottom()) {
                        cell_ptr.* = TerrainSpace{
                            .floor = .dirt,
                            .wall = .dirt,
                        };
                    } else {
                        cell_ptr.* = TerrainSpace{
                            .floor = .sand,
                            .wall = .air,
                        };
                    }
                }
            }
        }

        // build a structure.
        // start filled in.
        {
            var y = building_rect.y;
            while (y <= building_rect.bottom()) : (y += 1) {
                var x = building_rect.x;
                while (x <= building_rect.right()) : (x += 1) {
                    try mg.terrain.put(x, y, TerrainSpace{
                        .floor = .sandstone,
                        .wall = .sandstone,
                    });
                }
            }
        }

        //     / west_divider_x
        //     |   / main_hall_end.x
        //     |   |   / east_divider_x
        // ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
        // ▓6  ▓5  ▓7  ▓9  ▓
        // ▓           ▓   ▓
        // ▓▓▓▓▓▓ ▓▓   ▓▓ ▓▓ north_divider_y
        // ▓0  ▓4  ▓   ▓8  ▓
        // ▓   ▓   ▓   ▓   ▓
        // ▓▓ ▓▓▓ ▓▓   ▓   ▓
        // main h  ▓       ▓
        // ▓▓ ▓▓▓ ▓▓   ▓   ▓
        // ▓1  ▓2  ▓   ▓   ▓
        // ▓   ▓   ▓   ▓   ▓
        // ▓▓▓▓▓   ▓   ▓▓ ▓▓ south_divider_y
        // ▓3      ▓   ▓10 ▓
        // ▓       ▓   ▓   ▓
        // ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓

        // main hallway in the center.
        {
            var x = building_rect.x;
            while (x < main_hall_end.x) : (x += 1) {
                mg.terrain.getExisting(x, main_hall_end.y).wall = .air;
            }
        }

        const rooms = [_]Rect{
            rectFromCorners(
                building_rect.x,
                north_divider_y,
                west_divider_x,
                main_hall_end.y - 1,
            ),
            rectFromCorners(
                building_rect.x,
                main_hall_end.y + 1,
                west_divider_x,
                south_divider_y,
            ),
            rectFromCorners(
                west_divider_x,
                main_hall_end.y + 1,
                main_hall_end.x,
                building_rect.bottom(),
            ),
            rectFromCorners(
                building_rect.x,
                south_divider_y,
                west_divider_x,
                building_rect.bottom(),
            ),
            rectFromCorners(
                west_divider_x,
                north_divider_y,
                main_hall_end.x,
                main_hall_end.y - 1,
            ),
            rectFromCorners(
                west_divider_x,
                building_rect.y,
                main_hall_end.x,
                north_divider_y,
            ),
            rectFromCorners(
                building_rect.x,
                building_rect.y,
                west_divider_x,
                north_divider_y,
            ),
            rectFromCorners(
                main_hall_end.x,
                building_rect.y,
                east_divider_x,
                building_rect.bottom(),
            ),
            rectFromCorners(
                east_divider_x,
                north_divider_y,
                building_rect.right(),
                south_divider_y,
            ),
            rectFromCorners(
                east_divider_x,
                building_rect.y,
                building_rect.right(),
                north_divider_y,
            ),
            rectFromCorners(
                east_divider_x,
                south_divider_y,
                building_rect.right(),
                building_rect.bottom(),
            ),
        };
        for (rooms) |room| {
            digOutRect(mg.terrain, room);
        }

        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[0].x + 1, rooms[0].right()), rooms[0].bottom()).wall = .door_open;
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[1].x + 1, rooms[1].right()), rooms[1].y).wall = .door_closed;
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[2].x + 1, rooms[2].right()), rooms[2].y).wall = .door_closed;
        {
            // join rooms 2-3 with all open space.
            var y = rooms[3].y + 1;
            while (y < rooms[3].bottom()) : (y += 1) {
                mg.terrain.getExisting(rooms[3].right(), y).wall = .air;
            }
            // cracks in the bottom of room 2-3.
            var cracks_remaining = mg.r.intRangeAtMost(usize, 1, 4);
            while (cracks_remaining > 0) : (cracks_remaining -= 1) {
                const x = mg.r.intRangeLessThan(i32, rooms[3].x + 1, if (cracks_remaining == 1)
                    rooms[3].right()
                else
                    rooms[2].right());
                mg.terrain.getExisting(x, rooms[3].bottom()).wall = .sandstone_cracked;
            }
        }
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[4].x + 1, rooms[4].right()), rooms[4].bottom()).wall = .door_closed;
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[5].x + 2, rooms[5].right() - 1), rooms[5].bottom()).wall = .door_closed;
        mg.terrain.getExisting(rooms[6].right(), mg.r.intRangeLessThan(i32, rooms[6].y + 1, rooms[6].bottom())).wall = .door_closed;
        mg.terrain.getExisting(rooms[5].right(), mg.r.intRangeLessThan(i32, rooms[5].y + 1, rooms[5].bottom())).wall = .door_closed;
        mg.terrain.getExisting(rooms[8].x, main_hall_end.y).wall = .air;
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[9].x + 1, rooms[9].right()), rooms[9].bottom()).wall = .door_closed;
        mg.terrain.getExisting(mg.r.intRangeLessThan(i32, rooms[10].x + 1, rooms[10].right()), rooms[10].y).wall = .door_closed;

        // enemy time!
        var possible_spawn_locations = ArrayList(Coord).init(mg.allocator);
        // room 0 - a captive orc or something.
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[0]);
        var individuals_remaining: usize = 1;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .orc).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 1 - a wood golem that might come in handy.
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[1]);
        individuals_remaining = 1;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .wood_golem).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 2,3 - a bunch of ants.
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[3]);
        // queen in the corner
        const queen_coord = popRandom(mg.r, &possible_spawn_locations) orelse unreachable;
        try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(queen_coord, Species{ .ant = .queen }).clone(mg.allocator));
        mg.next_id += 1;
        try appendInteriorCoords(&possible_spawn_locations, rooms[2]);
        individuals_remaining = mg.r.intRangeAtMost(usize, 12, 20);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            const ant = try makeIndividual(coord, Species{ .ant = .worker }).clone(mg.allocator);
            ant.perceived_origin = queen_coord;
            try mg.individuals.putNoClobber(mg.next_id, ant);
            mg.next_id += 1;
        }
        // room 4 - scorpions and snakes.
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[4]);
        individuals_remaining = mg.r.intRangeAtMost(usize, 2, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .scorpion).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = mg.r.intRangeAtMost(usize, 2, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .brown_snake).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 5 - centaurs.
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[5]);
        individuals_remaining = mg.r.intRangeAtMost(usize, 2, 4);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            if (coord.y >= rooms[5].bottom() - 2) continue; // this can make it impossible to progress.
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, Species{ .centaur = .archer }).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 6 - angel statue.
        const statue_coord = Coord{
            .x = rooms[6].x + 1,
            .y = mg.r.intRangeLessThan(i32, rooms[6].y + 2, rooms[6].bottom() - 2),
        };
        mg.terrain.getExistingCoord(statue_coord).* = .{
            .floor = .marble,
            .wall = .angel_statue,
        };
        mg.terrain.getExisting(statue_coord.x + 1, statue_coord.y).floor = .marble;
        // room 7 - ogres
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[7]);
        individuals_remaining = 2;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .ogre).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 8 - minotaur
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[8]);
        individuals_remaining = 1;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .minotaur).clone(mg.allocator));
            mg.next_id += 1;
        }
        // room 9 - chest
        const chest_coord = Coord{
            .x = mg.r.intRangeLessThan(i32, rooms[9].x + 2, rooms[9].right() - 2),
            .y = rooms[9].y + 1,
        };
        mg.terrain.getExistingCoord(chest_coord).wall = .chest;
        // room 10 - human
        try clearAndAppendInteriorCoords(&possible_spawn_locations, rooms[10]);
        individuals_remaining = 1;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .human).clone(mg.allocator));
            mg.next_id += 1;
        }

        // outdoor enemies
        var it = desert_rect.rowMajorIterator();
        while (it.next()) |coord| {
            const cell = mg.terrain.getCoord(coord);
            if (cell.wall != .air) continue;
            if (cell.floor != .sand) continue;
            try possible_spawn_locations.append(coord);
        }

        individuals_remaining = mg.r.intRangeAtMost(usize, 10, 20);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .scorpion).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = mg.r.intRangeAtMost(usize, 5, 10);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .brown_snake).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = mg.r.intRangeAtMost(usize, 2, 4);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(mg.r, &possible_spawn_locations) orelse break;
            const ant = try makeIndividual(coord, Species{ .ant = .worker }).clone(mg.allocator);
            ant.perceived_origin = queen_coord;
            try mg.individuals.putNoClobber(mg.next_id, ant);
            mg.next_id += 1;
        }
    }
}

pub fn generatePuzzleLevels(game_state: *GameState) !void {
    try buildTheTerrain(&game_state.terrain);

    // start level 0

    // human is always id 1. (and dagger is id 2)
    var non_human_id_cursor: u32 = 3;
    var found_human = false;
    const level_x = 0;
    for (the_levels[0].individuals) |_individual| {
        const individual = try _individual.clone(game_state.allocator);
        individual.abs_position = core.game_logic.offsetPosition(individual.abs_position, makeCoord(level_x, 0));

        var id: u32 = undefined;
        if (individual.species == .human) {
            id = 1;
            found_human = true;
            // Start with a dagger.
            try game_state.items.putNoClobber(2, try (Item{
                .location = .{ .held = .{ .holder_id = id, .equipped_to_slot = .right_hand } },
                .kind = .dagger,
            }).clone(game_state.allocator));
        } else {
            id = non_human_id_cursor;
            non_human_id_cursor += 1;
        }
        try game_state.individuals.putNoClobber(id, individual);
    }
    std.debug.assert(found_human);
}

pub const the_levels = blk: {
    @setEvalBranchQuota(10000);
    break :blk [_]Level{
        compileLevel("Single enemy", .{},
            \\#########
            \\        #
            \\ o      #
            \\        #
            \\        +
            \\        #
            \\        #
            \\      h #
            \\        #
            \\#########
        ),
        compileLevel("Multiple enemies", .{},
            \\#########
            \\        #
            \\        #
            \\  oo    +
            \\   _o   #
            \\        #
            \\        #
            \\        #
            \\        #
            \\#########
        ),
        compileLevel("Archer in narrow hallway", .{},
            \\#######
            \\;;;;;;#
            \\     o+
            \\_    C#
            \\     o#
            \\;;;;;;#
            \\#######
        ),
        compileLevel("Flanked by archers", .{},
            \\##########
            \\C        #
            \\   _     +
            \\         #
            \\         #
            \\         #
            \\        C#
            \\##########
        ),
        compileLevel("Wall of archers", .{},
            \\#############
            \\;;;;;;;;;;;;#
            \\     _      #
            \\            #
            \\            #
            \\            +
            \\            #
            \\   CCCCC    #
            \\;;;;;;;;;;;;#
            \\#############
        ),

        compileLevel("Kangaroo", .{},
            \\#########
            \\        #
            \\        #
            \\        #
            \\        #
            \\  _ k   +
            \\        #
            \\        #
            \\        #
            \\#########
        ),
        compileLevel("Turtles", .{},
            \\#########
            \\        #
            \\        #
            \\      t #
            \\   ;;   +
            \\ t ;; _ #
            \\   ;;   #
            \\        #
            \\        #
            \\#########
        ),
        compileLevel("Turtles and an archer", .{},
            \\#########
            \\        #
            \\        #
            \\        #
            \\ _t;;   +
            \\ t ;; C #
            \\   ;;   #
            \\        #
            \\        #
            \\#########
        ),

        compileLevel("Rhino", .{ .facing_directions = &[_]CardinalDirection{.west} },
            \\#######
            \\;;;;;;#
            \\      #
            \\    r #
            \\ _    #
            \\      #
            \\      #
            \\      +
            \\;;;;;;#
            \\#######
        ),
        compileLevel("Rhino and archer", .{ .facing_directions = &[_]CardinalDirection{.east} },
            \\#########
            \\        #
            \\  r     #
            \\        #
            \\        #
            \\      C #
            \\        #
            \\   _    +
            \\        #
            \\#########
        ),
        compileLevel("turtles and a rhino", .{ .facing_directions = &[_]CardinalDirection{.east} },
            \\#########
            \\        #
            \\        #
            \\   r    #
            \\        #
            \\   t    #
            \\     t  #
            \\   _t   +
            \\C       #
            \\#########
        ),
        compileLevel("Archer guarding a chokepoint", .{},
            \\#########
            \\     ;  #
            \\     ;  +
            \\ k   ;  #
            \\     ;  #
            \\      C #
            \\     ;  #
            \\ _o  ;  #
            \\ko   ;  #
            \\     ;  #
            \\#########
        ),

        compileLevel("Some blobs", .{},
            \\######
            \\  ;  #
            \\_  b +
            \\  ; b#
            \\######
        ),
        compileLevel("Blob and orcs", .{},
            \\#######
            \\  + o;#
            \\_ + o +
            \\  +   #
            \\  +b  #
            \\      #
            \\#######
        ),
        compileLevel("Blob and turtles", .{},
            \\#######
            \\  + t;#
            \\_ + t +
            \\  +   #
            \\  +b  #
            \\      #
            \\#######
        ),

        compileLevel("You're the archer now!", .{ .traps = &[_]Wall{
            .polymorph_trap_centaur,
        } },
            \\###########
            \\##        #
            \\^+ _      #
            \\##      h #
            \\##        +
            \\###########
        ),
        compileLevel("Whose side are you on?", .{},
            \\########
            \\  CCC  #
            \\      h+
            \\C     h#
            \\C  _   #
            \\C     h#
            \\      h#
            \\  ooo  #
            \\########
        ),

        compileLevel("Kangaroos can't attack", .{ .traps = &[_]Wall{
            .polymorph_trap_kangaroo,
        } },
            \\########
            \\##  oo #
            \\^+ _ o +
            \\##     #
            \\##     #
            \\##;;;;;#
            \\########
        ),

        compileLevel("Blobs can't see", .{ .traps = &[_]Wall{
            .polymorph_trap_blob,
        } },
            \\##########
            \\   +o   ;#
            \\_;^+o #  +
            \\;; +; + ;#
            \\   +o +  #
            \\ +;+  +; #
            \\     +#o #
            \\##########
        ),

        compileLevel("Learn to charge", .{ .traps = &[_]Wall{
            .polymorph_trap_rhino_west,
            .polymorph_trap_rhino_east,
        } },
            \\##########
            \\###   ;C #
            \\^^+   ;; #
            \\#_ooo ;  #
            \\#+o      +
            \\t+    ;  #
            \\ +    ;; #
            \\      ;C #
            \\##########
        ),

        compileLevel("Look how you've grown", .{ .traps = &[_]Wall{
            .polymorph_trap_turtle,
            .polymorph_trap_centaur,
            .polymorph_trap_blob_west,
            .polymorph_trap_blob_east,
        } },
            \\#########
            \\        #
            \\ #    o^+
            \\_#^o  ;;#
            \\ #    ; #
            \\##    ; #
            \\^^    ;C#
            \\#########
        ),

        compileLevel("Turtles can't kick", .{ .traps = &[_]Wall{
            .polymorph_trap_blob_west,
            .polymorph_trap_blob_east,
            .polymorph_trap_blob_west,
            .polymorph_trap_blob_east,
            .polymorph_trap_rhino_west,
            .polymorph_trap_rhino_east,
            .polymorph_trap_blob_west,
            .polymorph_trap_blob_east,
            .polymorph_trap_human,
            .polymorph_trap_turtle,
            .polymorph_trap_turtle,
            .polymorph_trap_blob,
        } },
            \\###########
            \\ ^^ ^^^^###
            \\_+++++++^^#
            \\        #^+
            \\ ^      ###
            \\  +   + ###
            \\^ +ttt+ ###
            \\^ +;;;+ ###
            \\###########
        ),

        compileLevel("Don't kick the blob", .{ .traps = &[_]Wall{
            .polymorph_trap_rhino_west,
            .polymorph_trap_rhino_east,
        } },
            \\#######
            \\^^    #
            \\      +
            \\  _k  #
            \\  b   #
            \\      #
            \\      #
            \\#######
        ),

        compileLevel("-_-", .{},
            \\##############
            \\             #
            \\ _ _  _  _ _ #
            \\  _  _ _ _ _ #
            \\  _   _   __ #
            \\             #
            \\ _   _ _ __  #
            \\ _ _ _ _ _ _ #
            \\  _ _  _ _ _ #
            \\##############
        ),
    };
};

fn makeIndividual(small_position: Coord, species: Species) Individual {
    return .{
        .species = species,
        .abs_position = .{ .small = small_position },
        .perceived_origin = small_position,
    };
}
fn makeLargeIndividual(head_position: Coord, tail_position: Coord, species: Species) Individual {
    return .{
        .species = species,
        .abs_position = .{ .large = .{ head_position, tail_position } },
        .perceived_origin = head_position,
    };
}

const Options = struct {
    /// Each '^'.
    traps: []const Wall = &[_]Wall{},
    /// Cardinal directions for 'r' individuals.
    facing_directions: []const CardinalDirection = &[_]CardinalDirection{},
};

const Level = struct {
    width: u16,
    height: u16,
    terrain: []const TerrainSpace,
    individuals: []const Individual,
    name: []const u8,
};
fn compileLevel(name: []const u8, comptime options: Options, comptime source: []const u8) Level {
    // measure dimensions.
    const width = @as(u16, @intCast(std.mem.indexOfScalar(u8, source, '\n').?));
    const height = @as(u16, @intCast(@divExact(source.len + 1, width + 1)));
    var terrain = [_]TerrainSpace{oob_terrain} ** (width * height);
    comptime var level = Level{
        .width = width,
        .height = height,
        .terrain = &terrain,
        .individuals = &[_]Individual{},
        .name = name,
    };

    comptime var traps_index: usize = 0;
    comptime var facing_directions_index: usize = 0;

    comptime var cursor: usize = 0;
    comptime var y: u16 = 0;
    while (y < height) : (y += 1) {
        comptime var x: u16 = 0;
        while (x < width) : (x += 1) {
            const index = y * width + x;
            switch (source[cursor]) {
                '#' => {
                    terrain[index] = TerrainSpace{
                        .floor = .unknown,
                        .wall = .stone,
                    };
                },
                '+' => {
                    terrain[index] = TerrainSpace{
                        .floor = .unknown,
                        .wall = .dirt,
                    };
                },
                ' ' => {
                    terrain[index] = TerrainSpace{
                        .floor = .dirt,
                        .wall = .air,
                    };
                },
                ';' => {
                    terrain[index] = TerrainSpace{
                        .floor = .lava,
                        .wall = .air,
                    };
                },
                '_' => {
                    terrain[index] = TerrainSpace{
                        .floor = .hatch,
                        .wall = .air,
                    };
                },
                '^' => {
                    terrain[index] = TerrainSpace{
                        .floor = .dirt,
                        .wall = options.traps[traps_index],
                    };
                    traps_index += 1;
                },
                'o' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .orc)};
                },
                'b' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), Species{ .blob = .small_blob })};
                },
                'C' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), Species{ .centaur = .archer })};
                },
                'k' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .kangaroo)};
                },
                't' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .turtle)};
                },
                'h' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .human)};
                },
                'r' => {
                    terrain[index] = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeLargeIndividual(
                        makeCoord(x, y),
                        makeCoord(x, y).minus(core.geometry.cardinalDirectionToDelta(options.facing_directions[facing_directions_index])),
                        .rhino,
                    )};
                    facing_directions_index += 1;
                },
                else => unreachable,
            }
            cursor += 1;
        }
        cursor += 1;
    }

    std.debug.assert(traps_index == options.traps.len);
    std.debug.assert(facing_directions_index == options.facing_directions.len);

    return level;
}

fn buildTheTerrain(terrain: *Terrain) !void {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        height = @max(height, level.height);
    }

    var level_x: u16 = 0;
    for (the_levels) |level| {
        try terrain.copyFromSlice(level.terrain, level.width, level.height, level_x, 0, 0, 0, level.width, level.height);
        level_x += level.width;
    }
}

fn choice(r: Random, arr: anytype) @TypeOf(arr[0]) {
    return arr[r.uintLessThan(usize, arr.len)];
}

fn clone(allocator: Allocator, array_list: anytype) !@TypeOf(array_list) {
    var result = try @TypeOf(array_list).initCapacity(allocator, array_list.items.len);
    result.appendSliceAssumeCapacity(array_list.items);
    return result;
}

fn popRandom(r: Random, array_list: anytype) ?@TypeOf(array_list.*.items[0]) {
    if (array_list.items.len == 0) return null;
    return array_list.swapRemove(r.uintLessThan(usize, array_list.items.len));
}

fn rectFromCorners(x: i32, y: i32, right: i32, bottom: i32) Rect {
    return Rect{
        .x = x,
        .y = y,
        .width = right - x,
        .height = bottom - y,
    };
}

fn digOutRect(terrain: *Terrain, room_rect: Rect) void {
    var y = room_rect.y + 1;
    while (y < room_rect.bottom()) : (y += 1) {
        var x = room_rect.x + 1;
        while (x < room_rect.right()) : (x += 1) {
            terrain.getExisting(x, y).wall = .air;
        }
    }
}

fn clearAndAppendInteriorCoords(coords: *ArrayList(Coord), room_rect: Rect) !void {
    coords.clearRetainingCapacity();
    return appendInteriorCoords(coords, room_rect);
}
fn appendInteriorCoords(coords: *ArrayList(Coord), room_rect: Rect) !void {
    var it = (Rect{
        .x = room_rect.x + 1,
        .y = room_rect.y + 1,
        .width = room_rect.width - 2,
        .height = room_rect.height - 2,
    }).rowMajorIterator();
    while (it.next()) |coord| {
        try coords.append(coord);
    }
}
