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
const shrinkRect = core.geometry.shrinkRect;
const deltaToRotation = core.geometry.deltaToRotation;

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

    //debugTerrain(mg.terrain);

    game_state.warp_points = try mg.warp_points_list.toOwnedSlice();
}

fn generateMine(mg: *MapGenerator) !Rect {
    const max_area = 45;
    // Example layout:
    //
    //  start ▓▓▓                            \
    //   orcs ▓0▓                            |
    // dagger ▓▓▓-----▓▓▓ rats               |
    //                ▓1▓                    |row -1
    //                ▓▓▓  ▓▓▓ boss          |
    //                |    ▓5▓ orcs          |
    //                |    ▓▓▓ wolfs         /
    //              |--    |
    //              |      |     |---- exit  \
    //            ▓▓▓      ▓▓▓----           |
    // rats ▓▓▓▓  ▓2▓------▓4▓               |row 0
    // orcs ▓3▓▓--▓▓▓      ▓▓▓               /
    //            orcs     wolf
    //                     orcs
    //      \---/ \-----/  \-/
    //      col 0  col 1  col 2

    // col, row.
    var coarse_room_locations: [6]Coord = undefined;
    // rooms 0 and 1 can be north or south of room 2.
    coarse_room_locations[0] = makeCoord(0, choice(mg.r, [_]i32{ -1, 1 }));
    coarse_room_locations[1] = makeCoord(1, coarse_room_locations[0].y);
    coarse_room_locations[2] = makeCoord(1, 0);
    // room 3 either goes in the depicted position (0, 0), or in (1, -1) or (1, 1) whichever doesn't collide with room 1.
    coarse_room_locations[3].x = choice(mg.r, [_]i32{ 0, 1 });
    coarse_room_locations[3].y = if (coarse_room_locations[3].x == 0) 0 else -coarse_room_locations[1].y;
    coarse_room_locations[4] = makeCoord(2, 0);
    // room 5 can be north or south of room 4.
    coarse_room_locations[5] = makeCoord(2, choice(mg.r, [_]i32{ -1, 1 }));

    // traverse roughly backwards from right to left.
    const iteration_order = [_]usize{ 4, 5, 2, 1, 0, 3 };
    var rooms: [6]Rect = undefined;
    for (iteration_order, 0..) |room_index, i| {
        const center = coarse_room_locations[room_index].scaled(10).offset(
            mg.r.intRangeAtMost(i32, -4, 4),
            mg.r.intRangeAtMost(i32, -4, 4),
        );
        const area = mg.r.intRangeAtMost(i32, 35, max_area);
        var size: Coord = undefined;
        size.x = mg.r.intRangeAtMost(i32, 5, 9);
        size.y = @divTrunc(area, size.x);
        const position = center.minus(size.scaledDivTrunc(2));
        rooms[room_index] = makeRect(position, size);
        // don't overlap another room.
        for (iteration_order[0..i]) |other_index| {
            if (room_index == 2 and other_index == 4) {}
            var failsafe: u8 = 255;
            while (rooms[room_index].intersects(rooms[other_index])) {
                // because we're iterating right to left, sliding left is a reliable conflict resolution.
                rooms[room_index].x -= mg.r.intRangeLessThan(i32, 1, 3);
                failsafe -= 1; // overflow means we're in an infinite loop.
            }
        }
    }

    // dig out rooms
    for (rooms, 0..) |room, room_index| {
        core.debug.map_gen.print("room {}: {},{} {},{}", .{ room_index, room.x, room.y, room.width, room.height });
        var y = room.y;
        while (y <= room.bottom()) : (y += 1) {
            var x = room.x;
            while (x <= room.right()) : (x += 1) {
                try mg.terrain.put(x, y, if (x == room.x or y == room.y or x == room.right() or y == room.bottom())
                    TerrainSpace{ .floor = .dirt, .wall = .dirt }
                else
                    TerrainSpace{ .floor = .dirt, .wall = .air });
            }
        }
    }
    core.debug.map_gen.print("map: {},{}, {},{}", .{ mg.terrain.metrics.min_x, mg.terrain.metrics.min_y, mg.terrain.metrics.max_x, mg.terrain.metrics.max_y });
    debugTerrain(mg.terrain);

    // dig hallways
    const connections = [_][2]u8{
        .{ 4, 5 },
        .{ 4, 2 },
        .{ 2, 3 },
        .{ 2, 1 },
        .{ 1, 0 },
    };
    for (connections) |pair| {
        const inner_room_a = shrinkRect(rooms[pair[0]], 1);
        const inner_room_b = shrinkRect(rooms[pair[1]], 1);
        const direction_vector = Coord{
            .x = if (inner_room_a.right() < inner_room_b.x)
                1
            else if (inner_room_b.right() < inner_room_a.x)
                -1
            else
                0,
            .y = if (inner_room_a.bottom() < inner_room_b.y)
                1
            else if (inner_room_b.bottom() < inner_room_a.y)
                -1
            else
                0,
        };
        const direction_rotation = deltaToRotation(direction_vector);

        // the orcmages used clairvoyance to know the optimal digging path.
        const HallwayCarving = struct {
            start: Coord,
            end: Coord,
            step: Coord,
            wall_offsets: [2]Coord,
            fn dig(data: @This(), mg_: *MapGenerator) !void {
                var cursor = data.start.plus(data.step);
                var failsafe: u8 = 255;
                while (!cursor.equals(data.end)) : (cursor = cursor.plus(data.step)) {
                    try mg_.terrain.putCoord(cursor, TerrainSpace{ .floor = .dirt, .wall = .air });
                    try mg_.terrain.putCoord(cursor.plus(data.wall_offsets[0]), TerrainSpace{ .floor = .dirt, .wall = .dirt });
                    try mg_.terrain.putCoord(cursor.plus(data.wall_offsets[1]), TerrainSpace{ .floor = .dirt, .wall = .dirt });
                    failsafe -= 1; // overflow means we're in an infinite loop.
                }
            }
        };
        const horizontal_wall_offsets = [2]Coord{ makeCoord(0, -1), makeCoord(0, 1) };
        const vertical_wall_offsets = [2]Coord{ makeCoord(-1, 0), makeCoord(1, 0) };
        switch (@as(union(enum) {
            straight: HallwayCarving,
            diagonal: struct {
                hallways: [2]HallwayCarving,
                corner_walls: [3]Coord,
            },
        }, switch (direction_rotation) {
            .north => blk: {
                const x = mg.r.intRangeAtMost(
                    i32,
                    @max(inner_room_a.x, inner_room_b.x),
                    @min(inner_room_a.right(), inner_room_b.right()),
                );
                break :blk .{ .straight = .{
                    .start = makeCoord(x, inner_room_a.y),
                    .end = makeCoord(x, inner_room_b.bottom()),
                    .step = makeCoord(0, -1),
                    .wall_offsets = vertical_wall_offsets,
                } };
            },
            .south => blk: {
                const x = mg.r.intRangeAtMost(
                    i32,
                    @max(inner_room_a.x, inner_room_b.x),
                    @min(inner_room_a.right(), inner_room_b.right()),
                );
                break :blk .{ .straight = .{
                    .start = makeCoord(x, inner_room_a.bottom()),
                    .end = makeCoord(x, inner_room_b.y),
                    .step = makeCoord(0, 1),
                    .wall_offsets = vertical_wall_offsets,
                } };
            },
            .west => blk: {
                const y = mg.r.intRangeAtMost(
                    i32,
                    @max(inner_room_a.y, inner_room_b.y),
                    @min(inner_room_a.bottom(), inner_room_b.bottom()),
                );
                break :blk .{ .straight = .{
                    .start = makeCoord(inner_room_a.x, y),
                    .end = makeCoord(inner_room_b.right(), y),
                    .step = makeCoord(-1, 0),
                    .wall_offsets = horizontal_wall_offsets,
                } };
            },
            .east => blk: {
                const y = mg.r.intRangeAtMost(
                    i32,
                    @max(inner_room_a.y, inner_room_b.y),
                    @min(inner_room_a.bottom(), inner_room_b.bottom()),
                );
                break :blk .{ .straight = .{
                    .start = makeCoord(inner_room_a.right(), y),
                    .end = makeCoord(inner_room_b.x, y),
                    .step = makeCoord(1, 0),
                    .wall_offsets = horizontal_wall_offsets,
                } };
            },
            .northeast => blk: {
                // north, then east.
                const turn = makeCoord(inner_room_a.right(), inner_room_b.bottom());
                break :blk .{ .diagonal = .{
                    .hallways = .{
                        .{
                            .start = makeCoord(inner_room_a.right(), inner_room_a.y),
                            .end = turn,
                            .step = makeCoord(0, -1),
                            .wall_offsets = vertical_wall_offsets,
                        },
                        .{
                            .start = turn,
                            .end = makeCoord(inner_room_b.x, inner_room_b.bottom()),
                            .step = makeCoord(1, 0),
                            .wall_offsets = horizontal_wall_offsets,
                        },
                    },
                    .corner_walls = .{
                        turn.offset(-1, 0),
                        turn.offset(-1, -1),
                        turn.offset(0, -1),
                    },
                } };
            },
            .southeast => blk: {
                // south, then east.
                const turn = makeCoord(inner_room_a.right(), inner_room_b.y);
                break :blk .{ .diagonal = .{
                    .hallways = .{
                        .{
                            .start = makeCoord(inner_room_a.right(), inner_room_a.bottom()),
                            .end = turn,
                            .step = makeCoord(0, 1),
                            .wall_offsets = vertical_wall_offsets,
                        },
                        .{
                            .start = turn,
                            .end = makeCoord(inner_room_b.x, inner_room_b.y),
                            .step = makeCoord(1, 0),
                            .wall_offsets = horizontal_wall_offsets,
                        },
                    },
                    .corner_walls = .{
                        turn.offset(-1, 0),
                        turn.offset(-1, 1),
                        turn.offset(0, 1),
                    },
                } };
            },
            .southwest => blk: {
                // south, then west.
                const turn = makeCoord(inner_room_a.x, inner_room_b.y);
                break :blk .{ .diagonal = .{
                    .hallways = .{
                        .{
                            .start = makeCoord(inner_room_a.x, inner_room_a.bottom()),
                            .end = turn,
                            .step = makeCoord(0, 1),
                            .wall_offsets = vertical_wall_offsets,
                        },
                        .{
                            .start = turn,
                            .end = makeCoord(inner_room_b.right(), inner_room_b.y),
                            .step = makeCoord(-1, 0),
                            .wall_offsets = horizontal_wall_offsets,
                        },
                    },
                    .corner_walls = .{
                        turn.offset(1, 0),
                        turn.offset(1, 1),
                        turn.offset(0, 1),
                    },
                } };
            },
            .northwest => blk: {
                // north, then west.
                const turn = makeCoord(inner_room_a.x, inner_room_b.bottom());
                break :blk .{ .diagonal = .{
                    .hallways = .{
                        .{
                            .start = makeCoord(inner_room_a.x, inner_room_a.y),
                            .end = turn,
                            .step = makeCoord(0, -1),
                            .wall_offsets = vertical_wall_offsets,
                        },
                        .{
                            .start = turn,
                            .end = makeCoord(inner_room_b.right(), inner_room_b.bottom()),
                            .step = makeCoord(-1, 0),
                            .wall_offsets = horizontal_wall_offsets,
                        },
                    },
                    .corner_walls = .{
                        turn.offset(1, 0),
                        turn.offset(1, -1),
                        turn.offset(0, -1),
                    },
                } };
            },
        })) {
            .straight => |data| {
                try data.dig(mg);
            },
            .diagonal => |data| {
                try data.hallways[0].dig(mg);
                try mg.terrain.putCoord(data.hallways[0].end, TerrainSpace{ .floor = .dirt, .wall = .air });
                for (data.corner_walls) |coord| {
                    try mg.terrain.putCoord(coord, TerrainSpace{ .floor = .dirt, .wall = .dirt });
                }
                try data.hallways[1].dig(mg);
            },
        }
    }
    debugTerrain(mg.terrain);

    // Spawns
    for (rooms, 0..) |room, room_index| {
        var possible_spawn_locations: [max_area]Coord = undefined;
        var possible_spawn_locations_len: usize = 0;
        var it = shrinkRect(room, 1).rowMajorIterator();
        while (it.next()) |coord| {
            possible_spawn_locations[possible_spawn_locations_len] = coord;
            possible_spawn_locations_len += 1;
        }

        var orc_count: usize = 0;
        var wolf_count: usize = 0;
        var rat_count: usize = 0;
        var boss_room = false;
        var starting_room = false;
        switch (room_index) {
            0 => {
                starting_room = true;
                orc_count = 1;
            },
            1 => {
                rat_count = 5;
            },
            2 => {
                orc_count = mg.r.intRangeAtMost(usize, 3, 4);
            },
            3 => {
                orc_count = 2;
                rat_count = 7;
            },
            4 => {
                orc_count = mg.r.intRangeAtMost(usize, 2, 3);
                wolf_count = 1;
            },
            5 => {
                orc_count = 3;
                wolf_count = 2;
                boss_room = true;
            },
            else => unreachable,
        }

        if (starting_room) {
            const start_point = popRandom2(mg.r, &possible_spawn_locations, &possible_spawn_locations_len) orelse break;
            const human = try makeIndividual(start_point, .human).clone(mg.allocator);
            try mg.individuals.putNoClobber(1, human);
            try mg.warp_points_list.append(start_point);
            // It's dangerous to go alone. Take this.
            try mg.items.putNoClobber(mg.next_id, try (Item{
                .location = .{ .floor_coord = start_point },
                .kind = .dagger,
            }).clone(mg.allocator));
            mg.next_id += 1;
        }

        var individuals_remaining = orc_count;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom2(mg.r, &possible_spawn_locations, &possible_spawn_locations_len) orelse break;
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
            const coord = popRandom2(mg.r, &possible_spawn_locations, &possible_spawn_locations_len) orelse break;
            try mg.individuals.putNoClobber(mg.next_id, try makeIndividual(coord, .wolf).clone(mg.allocator));
            mg.next_id += 1;
        }
        individuals_remaining = rat_count;
        // All the rats in a room use an arbitrary but matching origin coordinate to make caching their ai decision more efficient.
        const rat_home = popRandom2(mg.r, &possible_spawn_locations, &possible_spawn_locations_len) orelse undefined;
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom2(mg.r, &possible_spawn_locations, &possible_spawn_locations_len) orelse break;
            const rat = try makeIndividual(coord, .rat).clone(mg.allocator);
            rat.perceived_origin = rat_home;
            try mg.individuals.putNoClobber(mg.next_id, rat);
            mg.next_id += 1;
        }
    }

    // room 4 has the exit path.
    return rooms[4];
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
fn popRandom2(r: Random, buf_pointer: anytype, slice_len_ptr: *usize) ?@TypeOf(buf_pointer[0]) {
    if (slice_len_ptr.* == 0) return null;
    const index = r.uintLessThan(usize, slice_len_ptr.*);
    const item = buf_pointer[index];
    buf_pointer[index] = buf_pointer[slice_len_ptr.* - 1];
    slice_len_ptr.* -= 1;
    return item;
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

fn debugTerrain(terrain: *Terrain) void {
    var line: [0x1000]u8 = undefined;
    var y = terrain.metrics.min_y;
    while (y <= terrain.metrics.max_y) : (y += 1) {
        var x = terrain.metrics.min_x;
        var i: usize = 0;
        while (x <= terrain.metrics.max_x) : (x += 1) {
            const cell = terrain.get(x, y);

            line[i] = if (cell.wall == .stone) ' ' else if (core.game_logic.isOpenSpace(cell.wall)) '.' else '#';
            i += 1;
        }
        core.debug.map_gen.print("{s}", .{line[0..i]});
    }
    core.debug.map_gen.print("---", .{});
}
