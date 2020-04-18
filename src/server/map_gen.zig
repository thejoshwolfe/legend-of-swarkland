const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const TerrainSpace = core.protocol.TerrainSpace;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Terrain = game_model.Terrain;
const Level = game_model.Level;
const oob_terrain = game_model.oob_terrain;

fn makeIndividual(small_position: Coord, species: Species) Individual {
    return .{
        .id = 0,
        .species = species,
        .abs_position = .{ .small = small_position },
    };
}
fn makeLargeIndividual(head_position: Coord, tail_position: Coord, species: Species) Individual {
    return .{
        .id = 0,
        .species = species,
        .abs_position = .{ .large = .{ head_position, tail_position } },
    };
}

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

    {
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
    }

    return level;
}

pub fn generateLevels() []const Level {
    @setEvalBranchQuota(10000);
    return comptime &[_]Level{
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
        compileLevel(
            \\#######
            \\;;;;;;#
            \\     o+
            \\_    C#
            \\     o#
            \\;;;;;;#
            \\#######
        ),
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
        compileLevel(
            \\###########
            \\##        #
            \\=+ _      #
            \\##      h #
            \\##        +
            \\###########
        ),
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
}
