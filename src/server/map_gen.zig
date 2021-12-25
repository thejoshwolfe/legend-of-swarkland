const std = @import("std");
const Allocator = std.mem.Allocator;
const ArrayList = std.ArrayList;

const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

const Species = core.protocol.Species;
const TerrainSpace = core.protocol.TerrainSpace;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Terrain = game_model.Terrain;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

/// overwrites terrain. populates individuals.
pub fn generate(allocator: Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
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
        // Single enemy
        compileLevel(
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
        // Multiple enemies
        compileLevel(
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
        // Archer in narrow hallway
        compileLevel(
            \\#######
            \\;;;;;;#
            \\     o+
            \\_    C#
            \\     o#
            \\;;;;;;#
            \\#######
        ),
        // Flanked by archers
        compileLevel(
            \\##########
            \\C        #
            \\   _     +
            \\         #
            \\         #
            \\         #
            \\        C#
            \\##########
        ),
        // Wall of archers
        compileLevel(
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
        // Single kagaroo
        compileLevel(
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
        // Invincible enemies
        compileLevel(
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
        // Invincible enemies and an archer
        compileLevel(
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
        // Charging enemy and lava
        compileLevel(
            \\#######
            \\;;;;;;#
            \\      #
            \\    < #
            \\ _    #
            \\      #
            \\      #
            \\      +
            \\;;;;;;#
            \\#######
        ),
        // Charging enemy and an archer
        compileLevel(
            \\#########
            \\        #
            \\  >     #
            \\        #
            \\        #
            \\      C #
            \\        #
            \\   _    +
            \\        #
            \\#########
        ),
        // Invicible enmies and a charging enemy
        compileLevel(
            \\#########
            \\        #
            \\        #
            \\   >    #
            \\        #
            \\   t    #
            \\     t  #
            \\   _t   +
            \\C       #
            \\#########
        ),
        // Archer guarding chokepoint
        compileLevel(
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
        // You're the archer now!
        compileLevel(
            \\###########
            \\##        #
            \\=+ _      #
            \\##      h #
            \\##        +
            \\###########
        ),
        // Whose side are you on?
        compileLevel(
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
        // -_-
        compileLevel(
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
    };
}
fn makeLargeIndividual(head_position: Coord, tail_position: Coord, species: Species) Individual {
    return .{
        .species = species,
        .abs_position = .{ .large = .{ head_position, tail_position } },
    };
}

const Level = struct {
    width: u16,
    height: u16,
    terrain: Terrain,
    individuals: []const Individual,
};
fn compileLevel(comptime source: []const u8) Level {
    // measure dimensions.
    const width = @intCast(u16, std.mem.indexOfScalar(u8, source, '\n').?);
    const height = @intCast(u16, @divExact(source.len + 1, width + 1));
    var terrain_space = [_]TerrainSpace{oob_terrain} ** (width * height);
    comptime var level = Level{
        .width = width,
        .height = height,
        .terrain = Terrain.initData(width, height, &terrain_space),
        .individuals = &[_]Individual{},
    };

    comptime var cursor: usize = 0;
    comptime var y: u16 = 0;
    while (y < height) : (y += 1) {
        comptime var x: u16 = 0;
        while (x < width) : (x += 1) {
            switch (source[cursor]) {
                '#' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .unknown,
                        .wall = .stone,
                    };
                },
                '+' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .unknown,
                        .wall = .dirt,
                    };
                },
                ' ' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .dirt,
                        .wall = .air,
                    };
                },
                ';' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .lava,
                        .wall = .air,
                    };
                },
                '_' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .hatch,
                        .wall = .air,
                    };
                },
                '=' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{
                        .floor = .dirt,
                        .wall = .centaur_transformer,
                    };
                },
                'o' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .orc)};
                },
                'C' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .centaur)};
                },
                'k' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .kangaroo)};
                },
                't' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .turtle)};
                },
                'h' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeIndividual(makeCoord(x, y), .human)};
                },
                '<' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeLargeIndividual(
                        makeCoord(x, y),
                        makeCoord(x + 1, y),
                        .rhino,
                    )};
                },
                '>' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeLargeIndividual(
                        makeCoord(x, y),
                        makeCoord(x - 1, y),
                        .rhino,
                    )};
                },
                '^' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeLargeIndividual(
                        makeCoord(x, y),
                        makeCoord(x, y + 1),
                        .rhino,
                    )};
                },
                'v' => {
                    level.terrain.atUnchecked(x, y).* = TerrainSpace{ .floor = .dirt, .wall = .air };
                    level.individuals = level.individuals ++ [_]Individual{makeLargeIndividual(
                        makeCoord(x, y),
                        makeCoord(x, y - 1),
                        .rhino,
                    )};
                },
                else => unreachable,
            }
            cursor += 1;
        }
        cursor += 1;
    }

    return level;
}

fn buildTheTerrain(allocator: Allocator) !Terrain {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        height = std.math.max(height, level.height);
    }

    const border_wall = TerrainSpace{
        .floor = .unknown,
        .wall = .stone,
    };
    var terrain = try Terrain.initFill(allocator, width, height, border_wall);

    var level_x: u16 = 0;
    for (the_levels) |level| {
        terrain.copy(level.terrain, level_x, 0, 0, 0, level.terrain.width, level.terrain.height);
        level_x += level.width;
    }

    return terrain;
}
