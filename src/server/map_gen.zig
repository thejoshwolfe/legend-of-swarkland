const std = @import("std");
const Allocator = std.mem.Allocator;
const ArrayList = std.ArrayList;

const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
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

    var y: i32 = -10;
    while (y < 10) : (y += 1) {
        var x: i32 = -10;
        while (x < 10) : (x += 1) {
            const cell = if (x == 9 or y == 9 or x == -10 or y == -10) TerrainSpace{
                .floor = .dirt,
                .wall = .dirt,
            } else TerrainSpace{
                .floor = .dirt,
                .wall = .air,
            };
            try terrain.put(x, y, cell);
        }
    }

    try individuals.putNoClobber(1, try makeIndividual(makeCoord(0, 0), .human).clone(allocator));
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
