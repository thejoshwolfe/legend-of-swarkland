const std = @import("std");
const Random = std.rand.Random;
const Allocator = std.mem.Allocator;
const ArrayList = std.ArrayList;

const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const makeRect = core.geometry.makeRect;
const Cardinal = core.geometry.Cardinal;

const NewGameSettings = core.protocol.NewGameSettings;
const Species = core.protocol.Species;
const TerrainSpace = core.protocol.TerrainSpace;
const Wall = core.protocol.Wall;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Terrain = game_model.Terrain;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

/// overwrites terrain. populates individuals.
pub fn generate(allocator: Allocator, terrain: *Terrain, individuals: *IdMap(*Individual), new_game_settings: NewGameSettings) !void {
    switch (new_game_settings) {
        .regular => return generateRegular(allocator, terrain, individuals),
        .puzzle_levels => return generatePuzzleLevels(allocator, terrain, individuals),
    }
}

pub fn generateRegular(allocator: Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
    terrain.* = Terrain.init(allocator);
    var _r = std.rand.DefaultPrng.init(std.crypto.random.int(u64));
    var r = _r.random();

    var rooms = ArrayList(Rect).init(allocator);

    var rooms_remaining = r.intRangeAtMost(u32, 5, 9);
    while (rooms_remaining > 0) : (rooms_remaining -= 1) {
        const size = makeCoord(
            r.intRangeAtMost(i32, 5, 9),
            r.intRangeAtMost(i32, 5, 9),
        );
        const position = makeCoord(
            r.intRangeAtMost(i32, terrain.metrics.min_x - 5 - size.x, terrain.metrics.max_x + 5),
            r.intRangeAtMost(i32, terrain.metrics.min_y - 5 - size.y, terrain.metrics.max_y + 5),
        );
        const room_rect = makeRect(position, size);
        try rooms.append(room_rect);

        var y = position.y;
        while (y <= room_rect.bottom()) : (y += 1) {
            var x = position.x;
            while (x <= room_rect.right()) : (x += 1) {
                const cell_ptr = try terrain.getOrPut(x, y);
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
    var next_id: u32 = 2;
    var rooms_for_spawn = try clone(allocator, rooms);
    var possible_spawn_locations = ArrayList(Coord).init(allocator);
    while (rooms_for_spawn.items.len > 1) {
        const room = popRandom(r, &rooms_for_spawn).?;
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

        // this is orc country
        var individuals_remaining = r.intRangeAtMost(usize, 1, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .orc).clone(allocator));
            next_id += 1;
        }
        individuals_remaining = r.intRangeAtMost(usize, 0, 1);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .wolf).clone(allocator));
            next_id += 1;
        }
        individuals_remaining = r.intRangeAtMost(usize, 3, 7) * r.int(u1);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .rat).clone(allocator));
            next_id += 1;
        }
    }

    try individuals.putNoClobber(1, try makeIndividual(makeCoord(
        r.intRangeLessThan(i32, rooms_for_spawn.items[0].x + 1, rooms_for_spawn.items[0].right() - 1),
        r.intRangeLessThan(i32, rooms_for_spawn.items[0].y + 1, rooms_for_spawn.items[0].bottom() - 1),
    ), .human).clone(allocator));

    // join rooms
    var rooms_to_join = try clone(allocator, rooms);
    while (rooms_to_join.items.len > 1) {
        const room_a = popRandom(r, &rooms_to_join).?;
        const room_b = choice(r, rooms_to_join.items);

        var cursor = makeCoord(
            r.intRangeAtMost(i32, room_a.x + 1, room_a.right() - 1),
            r.intRangeAtMost(i32, room_a.y + 1, room_a.bottom() - 1),
        );
        var dest = makeCoord(
            r.intRangeAtMost(i32, room_b.x + 1, room_b.right() - 1),
            r.intRangeAtMost(i32, room_b.y + 1, room_b.bottom() - 1),
        );
        var unit_diagonal = dest.minus(cursor).signumed();
        while (!cursor.equals(dest)) : ({
            // derpizontal, and then derpicle
            if (cursor.x != dest.x) {
                cursor.x += unit_diagonal.x;
            } else {
                cursor.y += unit_diagonal.y;
            }
        }) {
            {
                const cell_ptr = try terrain.getOrPutCoord(cursor);
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
                const cell_ptr = try terrain.getOrPutCoord(wall_cursor);
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

    // forest
    var forest_rect: Rect = undefined;
    {
        // path to forest
        var cursor = makeCoord(
            r.intRangeAtMost(i32, rooms_to_join.items[0].x + 1, rooms_to_join.items[0].right() - 1),
            r.intRangeAtMost(i32, rooms_to_join.items[0].y + 1, rooms_to_join.items[0].bottom() - 1),
        );
        const forest_start_x = terrain.metrics.max_x + 3;
        while (cursor.x < forest_start_x) : (cursor.x += 1) {
            {
                const cell_ptr = try terrain.getOrPutCoord(cursor);
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
                const cell_ptr = try terrain.getOrPutCoord(wall_cursor);
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
        forest_rect = Rect{
            .x = cursor.x - 1,
            .y = cursor.y - r.intRangeAtMost(i32, 20, 30),
            .width = r.intRangeAtMost(i32, 40, 60),
            .height = r.intRangeAtMost(i32, 40, 60),
        };
        // dig out the forest
        var y = forest_rect.y;
        while (y <= forest_rect.bottom()) : (y += 1) {
            var x = forest_rect.x;
            while (x <= forest_rect.right()) : (x += 1) {
                const cell_ptr = try terrain.getOrPut(x, y);
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
        possible_spawn_locations.clearRetainingCapacity();
        var it = (Rect{
            .x = forest_rect.x + 2,
            .y = forest_rect.y + 2,
            .width = forest_rect.width - 4,
            .height = forest_rect.height - 4,
        }).rowMajorIterator();
        while (it.next()) |coord| {
            const cell_ptr = try terrain.getOrPutCoord(coord);
            if (cell_ptr.wall != .air) continue;
            switch (r.int(u4)) {
                0...10 => {},
                11...13 => {
                    const next_cell_ptr = (try terrain.getOrPut(coord.x + 1, coord.y + 0));
                    if (next_cell_ptr.wall == .air) {
                        cell_ptr.wall = .tree_northwest;
                        next_cell_ptr.wall = .tree_northeast;
                        (try terrain.getOrPut(coord.x + 0, coord.y + 1)).wall = .tree_southwest;
                        (try terrain.getOrPut(coord.x + 1, coord.y + 1)).wall = .tree_southeast;
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
        var individuals_remaining = r.intRangeAtMost(usize, 4, 10);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .centaur).clone(allocator));
            next_id += 1;
        }
        individuals_remaining = r.intRangeAtMost(usize, 1, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .wood_golem).clone(allocator));
            next_id += 1;
        }
        individuals_remaining = r.intRangeAtMost(usize, 1, 3);
        while (individuals_remaining > 0) : (individuals_remaining -= 1) {
            const coord = popRandom(r, &possible_spawn_locations) orelse break;
            try individuals.putNoClobber(next_id, try makeIndividual(coord, .kangaroo).clone(allocator));
            next_id += 1;
        }
    }

    // desert
    var desert_rect: Rect = undefined;
    {
        // path to desert
        var opening = makeCoord(
            forest_rect.right() + 1,
            r.intRangeAtMost(i32, forest_rect.y + 1, forest_rect.bottom() - 2),
        );
        try terrain.put(opening.x - 1, opening.y, TerrainSpace{
            .floor = .dirt,
            .wall = .air,
        });
        try terrain.putCoord(opening, TerrainSpace{
            .floor = .dirt,
            .wall = .air,
        });

        // we've arrived at the forest
        desert_rect = Rect{
            .x = opening.x,
            .y = opening.y - 1, // offset below
            .width = r.intRangeAtMost(i32, 40, 60),
            .height = r.intRangeAtMost(i32, 40, 60),
        };
        const y_offset = r.intRangeLessThan(i32, 0, desert_rect.height - 2);
        desert_rect.y -= y_offset;

        // dig out the desert
        var y = desert_rect.y;
        while (y <= desert_rect.bottom()) : (y += 1) {
            var x = desert_rect.x;
            while (x <= desert_rect.right()) : (x += 1) {
                const cell_ptr = try terrain.getOrPut(x, y);
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
}

pub fn generatePuzzleLevels(allocator: Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
    terrain.* = try buildTheTerrain(allocator);

    // start level 0

    // human is always id 1
    var non_human_id_cursor: u32 = 2;
    var found_human = false;
    const level_x = 0;
    for (the_levels[0].individuals) |_individual| {
        const individual = try _individual.clone(allocator);
        individual.abs_position = core.game_logic.offsetPosition(individual.abs_position, makeCoord(level_x, 0));

        var id: u32 = undefined;
        if (individual.species == .human) {
            id = 1;
            found_human = true;
        } else {
            id = non_human_id_cursor;
            non_human_id_cursor += 1;
        }
        try individuals.putNoClobber(id, individual);
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

        compileLevel("Rhino", .{ .facing_directions = &[_]u2{Cardinal.left} },
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
        compileLevel("Rhino and archer", .{ .facing_directions = &[_]u2{Cardinal.right} },
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
        compileLevel("turtles and a rhino", .{ .facing_directions = &[_]u2{Cardinal.right} },
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
    facing_directions: []const u2 = &[_]u2{},
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
    const width = @intCast(u16, std.mem.indexOfScalar(u8, source, '\n').?);
    const height = @intCast(u16, @divExact(source.len + 1, width + 1));
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
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .centaur)};
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
                        makeCoord(x, y).minus(core.geometry.cardinalIndexToDirection(options.facing_directions[facing_directions_index])),
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

fn buildTheTerrain(allocator: Allocator) !Terrain {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        height = std.math.max(height, level.height);
    }

    var terrain = Terrain.init(allocator);

    var level_x: u16 = 0;
    for (the_levels) |level| {
        try terrain.copyFromSlice(level.terrain, level.width, level.height, level_x, 0, 0, 0, level.width, level.height);
        level_x += level.width;
    }

    return terrain;
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
