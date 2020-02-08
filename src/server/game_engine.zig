const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const isScaledCardinalDirection = core.geometry.isScaledCardinalDirection;
const directionToCardinalIndex = core.geometry.directionToCardinalIndex;
const makeCoord = core.geometry.makeCoord;
const zero_vector = makeCoord(0, 0);

const Action = core.protocol.Action;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const PerceivedFrame = core.protocol.PerceivedFrame;
const ThingPosition = core.protocol.ThingPosition;
const PerceivedThing = core.protocol.PerceivedThing;
const PerceivedActivity = core.protocol.PerceivedActivity;
const TerrainSpace = core.protocol.TerrainSpace;

const view_distance = core.game_logic.view_distance;
const isOpenSpace = core.game_logic.isOpenSpace;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const applyMovementToPosition = core.game_logic.applyMovementToPosition;
const getInertiaIndex = core.game_logic.getInertiaIndex;

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return HashMap(u32, V, core.geometry.hashU32, std.hash_map.getTrivialEqlFn(u32));
}

pub fn CoordMap(comptime V: type) type {
    return HashMap(Coord, V, Coord.hash, Coord.equals);
}

const Terrain = core.matrix.Matrix(TerrainSpace);
const oob_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};
const dirt_floor_terrain = TerrainSpace{
    .floor = .dirt,
    .wall = .air,
};
const stone_wall_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

/// Shallow copies the argument to a newly allocated pointer.
fn allocClone(allocator: *std.mem.Allocator, obj: var) !*@TypeOf(obj) {
    var x = try allocator.create(@TypeOf(obj));
    x.* = obj;
    return x;
}

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

const Level = struct {
    width: u16,
    height: u16,
    the_way_y: u16,
    transformer: ?Species,
    terrain: Terrain,
    individuals: []const Individual,
};
fn compileLevel(comptime source: []const u8, transformer: ?Species) Level {
    // measure dimensions.
    const width = @intCast(u16, std.mem.indexOfScalar(u8, source, '\n').?);
    const height = @intCast(u16, @divExact(source.len + 1, width + 1));
    comptime var level = Level{
        .width = width,
        .height = height,
        .the_way_y = height,
        .transformer = transformer,
        .terrain = Terrain.initData(
            width,
            height,
            &([_]TerrainSpace{oob_terrain} ** (width * height)),
        ),
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
                        if (level.the_way_y == height) {
                            // the first encountered hatch determines the way.
                            level.the_way_y = y;
                        }
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

const the_levels = blk: {
    @setEvalBranchQuota(10000);
    break :blk [_]Level{
        compileLevel(
            \\##########
            \\#        #
            \\# o      #
            \\#        #
            \\#        #
            \\#        #
            \\#        #
            \\#        #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#        #
            \\#  oo    #
            \\#  o_o   #
            \\#        #
            \\#        #
            \\#        #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\########
            \\#;;;;;;#
            \\#     o#
            \\#_    C#
            \\#     o#
            \\#;;;;;;#
            \\########
        , null),
        compileLevel(
            \\###########
            \\#C        #
            \\#   _     #
            \\#         #
            \\#         #
            \\#         #
            \\#        C#
            \\###########
        , null),
        compileLevel(
            \\##############
            \\#;;;;;;;;;;;;#
            \\#     _      #
            \\#            #
            \\#            #
            \\#            #
            \\#            #
            \\#  CCCCCCC   #
            \\#;;;;;;;;;;;;#
            \\##############
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#        #
            \\#        #
            \\#        #
            \\#  _ k   #
            \\#        #
            \\#        #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#        #
            \\#      t #
            \\#   ;;   #
            \\# t ;; _ #
            \\#   ;;   #
            \\#        #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#        #
            \\#        #
            \\# _t;;   #
            \\# t ;; C #
            \\#   ;;   #
            \\#        #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\########
            \\#;;;;;;#
            \\#      #
            \\#    < #
            \\# _    #
            \\#      #
            \\#      #
            \\#      #
            \\#;;;;;;#
            \\########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#  >     #
            \\#        #
            \\#        #
            \\#      C #
            \\#        #
            \\#   _    #
            \\#        #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\#        #
            \\#   >    #
            \\#        #
            \\#   t    #
            \\#     t  #
            \\#   _t   #
            \\#C       #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#     ;  #
            \\#     ;  #
            \\# k   ;  #
            \\#     ;  #
            \\#      C #
            \\#     ;  #
            \\# _o  ;  #
            \\#ko   ;  #
            \\#     ;  #
            \\##########
        , null),
        compileLevel(
            \\##########
            \\#        #
            \\# _      #
            \\#      h #
            \\#        #
            \\##########
        , .centaur),
        compileLevel(
            \\###############
            \\#             #
            \\# _ _  _  _ _ #
            \\#  _  _ _ _ _ #
            \\#  _   _   __ #
            \\#             #
            \\# _   _ _ __  #
            \\# _ _ _ _ _ _ #
            \\#  _ _  _ _ _ #
            \\###############
        , null),
    };
};

fn buildTheTerrain(allocator: *std.mem.Allocator) !Terrain {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        if (level.transformer != null) {
            width += 1;
        }
        height = std.math.max(height, level.height);
    }

    const border_wall = TerrainSpace{
        .floor = .unknown,
        .wall = .stone,
    };
    var terrain = try Terrain.initFill(allocator, width, height, border_wall);

    var level_x: u16 = 0;
    for (the_levels) |level| {
        if (level.transformer != null) {
            level_x += 1;
        }
        terrain.copy(level.terrain, level_x, 0, 0, 0, level.terrain.width, level.terrain.height);
        level_x += level.width;
    }

    return terrain;
}

fn assignId(individual: Individual, level_x: i32, id: u32) Individual {
    var ret = individual;
    switch (ret.abs_position) {
        .small => |*coord| {
            coord.x += level_x;
        },
        .large => |*coords| {
            for (coords) |*coord| {
                coord.x += level_x;
            }
        },
    }
    ret.id = id;
    return ret;
}
fn findAvailableId(cursor: *u32, usedIds: IdMap(*Individual)) u32 {
    while (usedIds.contains(cursor.*)) {
        cursor.* += 1;
    }
    defer cursor.* += 1;
    return cursor.*;
}

// TODO: sort all arrays to hide iteration order from the server
pub const GameEngine = struct {
    allocator: *std.mem.Allocator,

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{ .allocator = allocator };
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            .wait => return true,
            .move => |move_delta| return isCardinalDirection(move_delta),
            .fast_move => |move_delta| return isScaledCardinalDirection(move_delta, 2),
            .attack => |direction| return isCardinalDirection(direction),
            .kick => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn getStartGameHappenings(self: *const GameEngine) !Happenings {
        return Happenings{
            .individual_to_perception = IdMap([]PerceivedFrame).init(self.allocator),
            .state_changes = blk: {
                var ret = ArrayList(StateDiff).init(self.allocator);
                // human is always id 1
                try ret.append(StateDiff{ .spawn = Individual{ .id = 1, .abs_position = .{ .small = makeCoord(7, 7) }, .species = .human } });
                const level_x = 0;
                for (the_levels[0].individuals) |individual, i| {
                    try ret.append(StateDiff{ .spawn = assignId(individual, level_x, @intCast(u32, i) + 2) });
                }
                try ret.append(StateDiff{
                    .terrain_init = try buildTheTerrain(self.allocator),
                });
                break :blk ret.toOwnedSlice();
            },
        };
    }

    pub const Happenings = struct {
        individual_to_perception: IdMap([]PerceivedFrame),
        state_changes: []StateDiff,
    };

    /// Computes what would happen to the state of the game.
    /// The game_state object passed in should be distroyed and forgotten after this function returns.
    /// This is the entry point for all game rules.
    pub fn computeHappenings(self: *const GameEngine, game_state: *GameState, actions: IdMap(Action)) !Happenings {
        // cache the set of keys so iterator is easier.
        var everybody = try self.allocator.alloc(u32, game_state.individuals.count());
        {
            var iterator = game_state.individuals.iterator();
            for (everybody) |*x| {
                x.* = iterator.next().?.key;
            }
            std.debug.assert(iterator.next() == null);
        }
        const everybody_including_dead = try std.mem.dupe(self.allocator, u32, everybody);

        var individual_to_perception = IdMap(*MutablePerceivedHappening).init(self.allocator);
        for (everybody_including_dead) |id| {
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, MutablePerceivedHappening));
        }

        var current_positions = IdMap(ThingPosition).init(self.allocator);
        for (everybody) |id| {
            try current_positions.putNoClobber(id, game_state.individuals.getValue(id).?.abs_position);
        }

        var total_deaths = IdMap(void).init(self.allocator);

        {
            var intended_moves = IdMap(Coord).init(self.allocator);
            for (everybody) |id| {
                const move_delta: Coord = switch (actions.getValue(id).?) {
                    .move, .fast_move => |move_delta| move_delta,
                    else => continue,
                };
                try intended_moves.putNoClobber(id, move_delta);
            }

            try self.doMovementAndCollisions(
                game_state,
                &everybody,
                &individual_to_perception,
                &current_positions,
                &intended_moves,
                &total_deaths,
            );
        }

        // Kicks
        {
            var kicks = IdMap(Coord).init(self.allocator);
            var intended_moves = IdMap(Coord).init(self.allocator);
            var kicked_too_much = IdMap(void).init(self.allocator);
            for (everybody) |id| {
                const kick_direction: Coord = switch (actions.getValue(id).?) {
                    .kick => |direction| direction,
                    else => continue,
                };
                const attacker_coord = getHeadPosition(current_positions.getValue(id).?);
                const kick_position = attacker_coord.plus(kick_direction);
                for (everybody) |other_id| {
                    const position = current_positions.getValue(other_id).?;
                    for (getAllPositions(&position)) |coord, i| {
                        if (!coord.equals(kick_position)) continue;
                        // gotchya
                        if (getInertiaIndex(game_state.individuals.getValue(other_id).?.species) > 0) {
                            // Your kick is not stronk enough.
                            continue;
                        }
                        if (try intended_moves.put(other_id, kick_direction)) |_| {
                            // kicked multiple times at once!
                            _ = try kicked_too_much.put(other_id, {});
                        }
                    }
                }
                try kicks.putNoClobber(id, kick_direction);
            }
            if (kicks.count() > 0) {
                for (everybody) |id| {
                    try self.observeFrame(
                        game_state,
                        id,
                        individual_to_perception.getValue(id).?,
                        &current_positions,
                        Activities{
                            .kicks = &kicks,
                        },
                    );
                }

                // for now, multiple kicks at once just fail.
                var iterator = kicked_too_much.iterator();
                while (iterator.next()) |kv| {
                    intended_moves.removeAssertDiscard(kv.key);
                }

                try self.doMovementAndCollisions(
                    game_state,
                    &everybody,
                    &individual_to_perception,
                    &current_positions,
                    &intended_moves,
                    &total_deaths,
                );
            }
        }

        // Attacks
        var attacks = IdMap(Activities.Attack).init(self.allocator);
        var attack_deaths = IdMap(void).init(self.allocator);
        for (everybody) |id| {
            var attack_direction: Coord = switch (actions.getValue(id).?) {
                .attack => |direction| direction,
                else => continue,
            };
            var attacker_coord = getHeadPosition(current_positions.getValue(id).?);
            var attack_distance: i32 = 1;
            const range = core.game_logic.getAttackRange(game_state.individuals.getValue(id).?.species);
            range_loop: while (attack_distance <= range) : (attack_distance += 1) {
                var damage_position = attacker_coord.plus(attack_direction.scaled(attack_distance));
                for (everybody) |other_id| {
                    const position = current_positions.getValue(other_id).?;
                    for (getAllPositions(&position)) |coord, i| {
                        if (!coord.equals(damage_position)) continue;
                        // hit something.
                        if (core.game_logic.isAffectedByAttacks(game_state.individuals.getValue(other_id).?.species, i)) {
                            // get wrecked
                            _ = try attack_deaths.put(other_id, {});
                        }
                        break :range_loop;
                    }
                }
            }
            try attacks.putNoClobber(id, Activities.Attack{
                .direction = attack_direction,
                .distance = attack_distance,
            });
        }
        // Lava
        for (everybody) |id| {
            const position = current_positions.getValue(id).?;
            for (getAllPositions(&position)) |coord| {
                if (game_state.terrainAt(coord).floor == .lava) {
                    _ = try attack_deaths.put(id, {});
                }
            }
        }
        // Perception of Attacks and Death
        for (everybody) |id| {
            if (attacks.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    &current_positions,
                    Activities{ .attacks = &attacks },
                );
            }
            if (attack_deaths.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    &current_positions,
                    Activities{ .deaths = &attack_deaths },
                );
            }
        }
        try flushDeaths(&total_deaths, &attack_deaths, &everybody);

        // Traps
        var polymorphs = IdMap(Species).init(self.allocator);
        for (everybody) |id| {
            const position = current_positions.getValue(id).?;
            for (getAllPositions(&position)) |coord| {
                if (game_state.terrainAt(coord).wall == .centaur_transformer) {
                    try polymorphs.putNoClobber(id, .centaur);
                }
            }
        }
        if (polymorphs.count() != 0) {
            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    &current_positions,
                    Activities{ .polymorphs = &polymorphs },
                );
            }
        }

        var open_the_way = false;
        var button_getting_pressed: ?Coord = null;
        if (game_state.individuals.count() - total_deaths.count() <= 1) {
            // Only one person left. You win!
            if (total_deaths.count() > 0) {
                // Spawn the stairs onward.
                open_the_way = true;
            }
            // check for someone on the button
            for (everybody) |id| {
                const position = current_positions.getValue(id).?;
                for (getAllPositions(&position)) |coord| {
                    if (game_state.terrainAt(coord).floor == .hatch) {
                        button_getting_pressed = coord;
                        break;
                    }
                }
            }
        }

        var new_id_cursor: u32 = @intCast(u32, game_state.individuals.count());

        // build state changes
        var state_changes = ArrayList(StateDiff).init(self.allocator);
        for (everybody_including_dead) |id| {
            switch (game_state.individuals.getValue(id).?.abs_position) {
                .small => |coord| {
                    const from = coord;
                    const to = current_positions.getValue(id).?.small;
                    if (to.equals(from)) continue;
                    const delta = to.minus(from);
                    try state_changes.append(StateDiff{
                        .small_move = .{
                            .id = id,
                            .coord = delta,
                        },
                    });
                },
                .large => |coords| {
                    const from = coords;
                    const to = current_positions.getValue(id).?.large;
                    if (to[0].equals(from[0]) and to[1].equals(from[1])) continue;
                    try state_changes.append(StateDiff{
                        .large_move = .{
                            .id = id,
                            .coords = .{
                                to[0].minus(from[0]),
                                to[1].minus(from[1]),
                            },
                        },
                    });
                },
            }
        }
        {
            var iterator = polymorphs.iterator();
            while (iterator.next()) |kv| {
                try state_changes.append(StateDiff{
                    .polymorph = .{
                        .id = kv.key,
                        .from = game_state.individuals.getValue(kv.key).?.species,
                        .to = kv.value,
                    },
                });
            }
        }
        {
            var iterator = total_deaths.iterator();
            while (iterator.next()) |kv| {
                try state_changes.append(StateDiff{
                    .despawn = blk: {
                        var individual = game_state.individuals.getValue(kv.key).?.*;
                        individual.abs_position = current_positions.getValue(individual.id).?;
                        break :blk individual;
                    },
                });
            }
        }

        if (open_the_way and game_state.level_number + 1 < the_levels.len) {
            // open the way
            var level_x: u16 = 0;
            for (the_levels[0..game_state.level_number]) |level| {
                level_x += level.width;
                if (level.transformer != null) {
                    level_x += 1;
                }
            }
            const level = the_levels[game_state.level_number];
            const next_level = the_levels[game_state.level_number + 1];
            const the_way_y = next_level.the_way_y;
            if (next_level.transformer) |transformer| {
                for ([_]Coord{
                    makeCoord(level_x + level.width - 1, the_way_y),
                    makeCoord(level_x + level.width + 1, the_way_y),
                }) |coord| {
                    try state_changes.append(StateDiff{
                        .terrain_update = StateDiff.TerrainDiff{
                            .at = coord,
                            .from = game_state.terrain.getCoord(coord).?,
                            .to = TerrainSpace{
                                .floor = .dirt,
                                .wall = .air,
                            },
                        },
                    });
                }

                // the transformer
                const coord = makeCoord(level_x + level.width, the_way_y);
                try state_changes.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .at = coord,
                        .from = game_state.terrain.getCoord(coord).?,
                        .to = TerrainSpace{
                            .floor = .dirt,
                            .wall = .centaur_transformer,
                        },
                    },
                });
            } else {

                // no transformer
                for ([_]Coord{
                    makeCoord(level_x + level.width - 1, the_way_y),
                    makeCoord(level_x + level.width - 0, the_way_y),
                }) |coord| {
                    try state_changes.append(StateDiff{
                        .terrain_update = StateDiff.TerrainDiff{
                            .at = coord,
                            .from = game_state.terrain.getCoord(coord).?,
                            .to = TerrainSpace{
                                .floor = .dirt,
                                .wall = .air,
                            },
                        },
                    });
                }
            }
        }

        if (button_getting_pressed) |button_coord| {
            const new_level_number = blk: {
                if (game_state.level_number + 1 < the_levels.len) {
                    try state_changes.append(StateDiff.transition_to_next_level);
                    break :blk game_state.level_number + 1;
                } else {
                    break :blk game_state.level_number;
                }
            };
            var level_x: u16 = 0;
            for (the_levels[0..new_level_number]) |level| {
                level_x += level.width;
                if (level.transformer != null) {
                    level_x += 1;
                }
            }
            // close the way
            const the_way_y = the_levels[new_level_number].the_way_y;
            for ([_]Coord{
                makeCoord(level_x - 1, the_way_y),
                makeCoord(level_x + 0, the_way_y),
            }) |coord| {
                try state_changes.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .at = coord,
                        .from = game_state.terrain.getCoord(coord).?,
                        .to = TerrainSpace{
                            .floor = .unknown,
                            .wall = .stone,
                        },
                    },
                });
            }
            if (the_levels[new_level_number].transformer != null) {
                // one extra space
                const coord = makeCoord(level_x + 1, the_way_y);
                try state_changes.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .at = coord,
                        .from = game_state.terrain.getCoord(coord).?,
                        .to = TerrainSpace{
                            .floor = .unknown,
                            .wall = .stone,
                        },
                    },
                });
            }

            // destroy the button
            try state_changes.append(StateDiff{
                .terrain_update = StateDiff.TerrainDiff{
                    .at = button_coord,
                    .from = game_state.terrain.getCoord(button_coord).?,
                    .to = TerrainSpace{
                        .floor = .dirt,
                        .wall = .air,
                    },
                },
            });
            // spawn enemies
            for (the_levels[new_level_number].individuals) |individual| {
                const id = findAvailableId(&new_id_cursor, game_state.individuals);
                try state_changes.append(StateDiff{ .spawn = assignId(individual, level_x, id) });
            }
        }

        // final observations
        try game_state.applyStateChanges(state_changes.toSliceConst());
        current_positions.clear();
        for (everybody) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.getValue(id).?,
                null,
                Activities.static_state,
            );
        }

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap([]PerceivedFrame).init(self.allocator);
                for (everybody_including_dead) |id| {
                    var frame_list = individual_to_perception.getValue(id).?.frames;
                    // remove empty frames, except the last one
                    var i: usize = 0;
                    frameLoop: while (i + 1 < frame_list.len) : (i +%= 1) {
                        const frame = frame_list.at(i);
                        if (frame.self.activity != PerceivedActivity.none) continue :frameLoop;
                        for (frame.others) |other| {
                            if (other.activity != PerceivedActivity.none) continue :frameLoop;
                        }
                        // delete this frame
                        _ = frame_list.orderedRemove(i);
                        i -%= 1;
                    }
                    try ret.putNoClobber(id, frame_list.toOwnedSlice());
                }
                break :blk ret;
            },
            .state_changes = state_changes.toOwnedSlice(),
        };
    }

    fn doMovementAndCollisions(
        self: *const GameEngine,
        game_state: *GameState,
        everybody: *[]u32,
        individual_to_perception: *IdMap(*MutablePerceivedHappening),
        current_positions: *IdMap(ThingPosition),
        intended_moves: *IdMap(Coord),
        total_deaths: *IdMap(void),
    ) !void {
        var next_positions = IdMap(ThingPosition).init(self.allocator);

        for (everybody.*) |id| {
            // seek forward and stop at any wall.
            const initial_head_coord = getHeadPosition(current_positions.getValue(id).?);
            const move_delta = intended_moves.getValue(id) orelse continue;
            const move_unit = move_delta.signumed();
            const move_magnitude = move_delta.magnitudeDiag();
            var distance: i32 = 1;
            while (true) : (distance += 1) {
                const new_head_coord = initial_head_coord.plus(move_unit.scaled(distance));
                if (!isOpenSpace(game_state.terrainAt(new_head_coord).wall)) {
                    // bonk
                    distance -= 1;
                    break;
                }
                if (distance >= move_magnitude)
                    break;
            }
            if (distance == 0) {
                // no move for you
                continue;
            }
            // found an open space
            const adjusted_move_delta = move_unit.scaled(distance);
            _ = intended_moves.put(id, adjusted_move_delta) catch unreachable;
            try next_positions.putNoClobber(id, applyMovementToPosition(current_positions.getValue(id).?, adjusted_move_delta));
        }

        var trample_deaths = IdMap(void).init(self.allocator);

        // Collision detection and resolution.
        {
            const Collision = struct {
                /// this has at most one entry
                inertia_index_to_stationary_id: [2]?u32 = [_]?u32{null} ** 2,
                inertia_index_to_cardinal_index_to_enterer: [2][4]?u32 = [_][4]?u32{[_]?u32{null} ** 4} ** 2,
                inertia_index_to_cardinal_index_to_fast_enterer: [2][4]?u32 = [_][4]?u32{[_]?u32{null} ** 4} ** 2,
                winner_id: ?u32 = null,
            };

            // Collect collision information.
            var coord_to_collision = CoordMap(Collision).init(self.allocator);
            for (everybody.*) |id| {
                const inertia_index = getInertiaIndex(game_state.individuals.getValue(id).?.species);
                const old_position = current_positions.getValue(id).?;
                const new_position = next_positions.getValue(id) orelse old_position;
                for (getAllPositions(&new_position)) |new_coord, i| {
                    const old_coord = getAllPositions(&old_position)[i];
                    const delta = new_coord.minus(old_coord);
                    var collision = coord_to_collision.getValue(new_coord) orelse Collision{};
                    if (delta.equals(zero_vector)) {
                        collision.inertia_index_to_stationary_id[inertia_index] = id;
                    } else if (isCardinalDirection(delta)) {
                        collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][directionToCardinalIndex(delta)] = id;
                    } else if (isScaledCardinalDirection(delta, 2)) {
                        collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][directionToCardinalIndex(delta.scaledDivTrunc(2))] = id;
                    } else unreachable;
                    _ = try coord_to_collision.put(new_coord, collision);
                }
            }

            // Determine who wins each collision
            {
                var iterator = coord_to_collision.iterator();
                while (iterator.next()) |kv| {
                    var collision: *Collision = &kv.value;
                    var is_trample = false;
                    // higher inertia trumps any lower inertia
                    for ([_]u1{ 1, 0 }) |inertia_index| {
                        if (is_trample) {
                            // You can't win. You can only get trampled.
                            for ([_]?u32{
                                collision.inertia_index_to_stationary_id[inertia_index],
                                collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][0],
                                collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][1],
                                collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][2],
                                collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][3],
                                collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][0],
                                collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][1],
                                collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][2],
                                collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][3],
                            }) |maybe_id| {
                                if (maybe_id) |trampled_id| {
                                    try trample_deaths.putNoClobber(trampled_id, {});
                                }
                            }
                        } else {
                            var incoming_vector_set: u9 = 0;
                            if (collision.inertia_index_to_stationary_id[inertia_index] != null) incoming_vector_set |= 1 << 0;
                            for (collision.inertia_index_to_cardinal_index_to_enterer[inertia_index]) |maybe_id, i| {
                                if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (1 + @as(u4, @intCast(u2, i)));
                            }
                            for (collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index]) |maybe_id, i| {
                                if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (5 + @as(u4, @intCast(u2, i)));
                            }
                            if (incoming_vector_set & 1 != 0) {
                                // Stationary entities always win.
                                collision.winner_id = collision.inertia_index_to_stationary_id[inertia_index];
                                // Standing still doesn't trample.
                            } else if (cardinalIndexBitSetToCollisionWinnerIndex(@intCast(u4, (incoming_vector_set & 0b111100000) >> 5))) |index| {
                                // fast bois beat slow bois.
                                collision.winner_id = collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][index].?;
                                is_trample = inertia_index > 0;
                            } else if (cardinalIndexBitSetToCollisionWinnerIndex(@intCast(u4, (incoming_vector_set & 0b11110) >> 1))) |index| {
                                // a slow boi wins.
                                collision.winner_id = collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][index].?;
                                is_trample = inertia_index > 0;
                            } else {
                                // nobody wins. winner stays null.
                            }
                        }
                    }
                }
            }
            id_loop: for (everybody.*) |id| {
                if (trample_deaths.contains(id)) {
                    // The move succeeds so that we see where the squashing happens.
                    continue;
                }
                const next_position = next_positions.getValue(id) orelse continue;
                for (getAllPositions(&next_position)) |coord| {
                    var collision = coord_to_collision.getValue(coord).?;
                    // TODO: https://github.com/ziglang/zig/issues/1332 if (collision.winner_id != id)
                    if (!(collision.winner_id != null and collision.winner_id.? == id)) {
                        // i lose.
                        next_positions.removeAssertDiscard(id);
                        continue :id_loop;
                    }
                }
            }

            // Check for unsuccessful conga lines.
            // for example:
            //   🐕 -> 🐕
            //         V
            //   🐕 <- 🐕
            // This conga line would be unsuccessful because the head of the line isn't successfully moving.
            for (everybody.*) |head_id| {
                // anyone who fails to move could be the head of a sad conga line.
                if (next_positions.contains(head_id)) continue;
                if (trample_deaths.contains(head_id)) {
                    // You're dead to me.
                    continue;
                }
                var id = head_id;
                conga_loop: while (true) {
                    const position = current_positions.getValue(id).?;
                    for (getAllPositions(&position)) |coord| {
                        const follower_id = (coord_to_collision.remove(coord) orelse continue).value.winner_id orelse continue;
                        if (follower_id == id) continue; // your tail is always allowed to follow your head.
                        // conga line is botched.
                        if (getInertiaIndex(game_state.individuals.getValue(follower_id).?.species) >
                            getInertiaIndex(game_state.individuals.getValue(head_id).?.species))
                        {
                            // Outta my way!
                            try trample_deaths.putNoClobber(head_id, {});
                            // Party don't stop for this pushover's failure! Viva la conga!
                            break :conga_loop;
                        }
                        _ = next_positions.remove(follower_id);
                        // and now you get to pass on the bad news.
                        id = follower_id;
                        continue :conga_loop;
                    }
                    // no one wants to move into this space.
                    break;
                }
            }
        }

        // Observe movement.
        for (everybody.*) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.getValue(id).?,
                current_positions,
                Activities{
                    .movement = .{
                        .intended_moves = intended_moves,
                        .next_positions = &next_positions,
                    },
                },
            );
        }

        // Update positions from movement.
        for (everybody.*) |id| {
            const next_position = next_positions.getValue(id) orelse continue;
            current_positions.get(id).?.value = next_position;
        }

        for (everybody.*) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.getValue(id).?,
                current_positions,
                Activities{ .deaths = &trample_deaths },
            );
        }
        try flushDeaths(total_deaths, &trample_deaths, everybody);
    }

    const Activities = union(enum) {
        static_state,
        movement: Movement,
        const Movement = struct {
            intended_moves: *const IdMap(Coord),
            next_positions: *const IdMap(ThingPosition),
        };

        attacks: *const IdMap(Attack),
        const Attack = struct {
            direction: Coord,
            distance: i32,
        };

        kicks: *const IdMap(Coord),
        polymorphs: *const IdMap(Species),

        deaths: *const IdMap(void),
    };
    fn observeFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        perception: *MutablePerceivedHappening,
        maybe_current_positions: ?*const IdMap(ThingPosition),
        activities: Activities,
    ) !void {
        try perception.frames.append(try getPerceivedFrame(
            self,
            game_state,
            my_id,
            maybe_current_positions,
            activities,
        ));
    }

    pub fn getStaticPerception(self: *const GameEngine, game_state: GameState, individual_id: u32) !PerceivedFrame {
        return getPerceivedFrame(
            self,
            &game_state,
            individual_id,
            null,
            Activities.static_state,
        );
    }

    fn getPerceivedFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        maybe_current_positions: ?*const IdMap(ThingPosition),
        activities: Activities,
    ) !PerceivedFrame {
        const your_coord = getHeadPosition(if (maybe_current_positions) |current_positions|
            current_positions.getValue(my_id).?
        else
            game_state.individuals.getValue(my_id).?.abs_position);
        var yourself: ?PerceivedThing = null;
        var others = ArrayList(PerceivedThing).init(self.allocator);

        var iterator = game_state.individuals.iterator();
        while (iterator.next()) |kv| {
            const id = kv.key;
            const activity = switch (activities) {
                .movement => |data| if (data.intended_moves.getValue(id)) |move_delta|
                    if (data.next_positions.contains(id))
                        PerceivedActivity{ .movement = move_delta }
                    else
                        PerceivedActivity{ .failed_movement = move_delta }
                else
                    PerceivedActivity{ .none = {} },

                .attacks => |data| if (data.getValue(id)) |attack|
                    PerceivedActivity{
                        .attack = PerceivedActivity.Attack{
                            .direction = attack.direction,
                            .distance = attack.distance,
                        },
                    }
                else
                    PerceivedActivity{ .none = {} },

                .kicks => |data| if (data.getValue(id)) |coord|
                    PerceivedActivity{ .kick = coord }
                else
                    PerceivedActivity{ .none = {} },

                .polymorphs => |data| if (data.getValue(id)) |species|
                    PerceivedActivity{ .polymorph = species }
                else
                    PerceivedActivity{ .none = {} },

                .deaths => |data| if (data.getValue(id)) |_|
                    PerceivedActivity{ .death = {} }
                else
                    PerceivedActivity{ .none = {} },

                .static_state => PerceivedActivity{ .none = {} },
            };

            var abs_position = if (maybe_current_positions) |current_positions|
                current_positions.getValue(id).?
            else
                game_state.individuals.getValue(id).?.abs_position;
            var rel_position: ThingPosition = undefined;
            switch (abs_position) {
                .small => |coord| {
                    rel_position = .{ .small = coord.minus(your_coord) };
                },
                .large => |coords| {
                    rel_position = .{
                        .large = .{
                            coords[0].minus(your_coord),
                            coords[1].minus(your_coord),
                        },
                    };
                },
            }
            // if any position is within view, we can see all of it.
            const within_view = blk: for (getAllPositions(&rel_position)) |delta| {
                if (delta.magnitudeDiag() <= view_distance) break :blk true;
            } else false;
            if (!within_view) continue;
            const thing = PerceivedThing{
                .species = game_state.individuals.getValue(id).?.species,
                .rel_position = rel_position,
                .activity = activity,
            };
            if (id == my_id) {
                yourself = thing;
            } else {
                try others.append(thing);
            }
        }

        const view_size = view_distance * 2 + 1;
        var terrain_chunk = core.protocol.TerrainChunk{
            .rel_position = makeCoord(-view_distance, -view_distance),
            .matrix = try Terrain.initFill(self.allocator, view_size, view_size, oob_terrain),
        };
        const view_origin = your_coord.minus(makeCoord(view_distance, view_distance));
        var cursor = Coord{ .x = undefined, .y = 0 };
        while (cursor.y < view_size) : (cursor.y += 1) {
            cursor.x = 0;
            while (cursor.x < view_size) : (cursor.x += 1) {
                if (game_state.terrain.getCoord(cursor.plus(view_origin))) |cell| {
                    terrain_chunk.matrix.atCoord(cursor).?.* = cell;
                }
            }
        }

        var you_win = blk: for (game_state.terrain.data) |space| {
            if (space.floor == .hatch) {
                break :blk false;
            }
        } else true;

        return PerceivedFrame{
            .self = yourself.?,
            .others = others.toOwnedSlice(),
            .terrain = terrain_chunk,
            .you_win = you_win,
        };
    }
};

pub const Individual = struct {
    id: u32,
    species: Species,
    abs_position: ThingPosition,
};
pub const StateDiff = union(enum) {
    spawn: Individual,
    despawn: Individual,
    small_move: IdAndCoord,
    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };

    large_move: IdAndCoords,
    pub const IdAndCoords = struct {
        id: u32,
        coords: [2]Coord,
    };

    polymorph: Polymorph,
    pub const Polymorph = struct {
        id: u32,
        from: Species,
        to: Species,
    };

    /// can only be in the start game events. can never be undone.
    terrain_init: Terrain,

    terrain_update: TerrainDiff,
    pub const TerrainDiff = struct {
        at: Coord,
        from: TerrainSpace,
        to: TerrainSpace,
    };

    transition_to_next_level,
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),
    level_number: u16,

    pub fn init(allocator: *std.mem.Allocator) GameState {
        return GameState{
            .allocator = allocator,
            .terrain = Terrain.initEmpty(),
            .individuals = IdMap(*Individual).init(allocator),
            .level_number = 0,
        };
    }

    pub fn clone(self: GameState) !GameState {
        return GameState{
            .allocator = self.allocator,
            .terrain = self.terrain,
            .individuals = blk: {
                var ret = IdMap(*Individual).init(self.allocator);
                var iterator = self.individuals.iterator();
                while (iterator.next()) |kv| {
                    try ret.putNoClobber(kv.key, try allocClone(self.allocator, kv.value.*));
                }
                break :blk ret;
            },
            .level_number = self.level_number,
        };
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .despawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.plus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.getValue(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].plus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].plus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.getValue(polymorph.id).?;
                    individual.species = polymorph.to;
                },
                .terrain_init => |terrain| {
                    self.terrain = terrain;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.to;
                },
                .transition_to_next_level => {
                    self.level_number += 1;
                },
            }
        }
    }
    fn undoStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .despawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.minus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.getValue(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].minus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].minus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.getValue(polymorph.id).?;
                    individual.species = polymorph.from;
                },
                .terrain_init => {
                    @panic("can't undo terrain init");
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.from;
                },
                .transition_to_next_level => {
                    self.level_number -= 1;
                },
            }
        }
    }

    pub fn terrainAt(self: GameState, coord: Coord) TerrainSpace {
        return self.terrain.getCoord(coord) orelse oob_terrain;
    }
};

const MutablePerceivedHappening = struct {
    frames: ArrayList(PerceivedFrame),
    pub fn init(allocator: *std.mem.Allocator) MutablePerceivedHappening {
        return MutablePerceivedHappening{
            .frames = ArrayList(PerceivedFrame).init(allocator),
        };
    }
};

fn flushDeaths(total_deaths: *IdMap(void), local_deaths: *IdMap(void), everybody: *[]u32) !void {
    var iterator = local_deaths.iterator();
    while (iterator.next()) |kv| {
        const id = kv.key;
        // write local deaths to total deaths
        if (null == try total_deaths.put(id, {})) {
            // actually removed.
            const index = std.mem.indexOfScalar(u32, everybody.*, id).?;
            // swap remove
            everybody.*[index] = everybody.*[everybody.len - 1];
            everybody.len -= 1;
        }
    }
    local_deaths.clear();
}

/// See geometry for cardinal index definition.
/// e.g. 0b0001: just right, 0b1100: left and up.
/// Note that the directions are the movement directions, not the "from" directions.
/// Returning null means no winner.
fn cardinalIndexBitSetToCollisionWinnerIndex(cardinal_index_bit_set: u4) ?u2 {
    switch (cardinal_index_bit_set) {
        // nobody here.
        0b0000 => return null,
        // only one person
        0b0001 => return 0,
        0b0010 => return 1,
        0b0100 => return 2,
        0b1000 => return 3,
        // 2-way head-on collision
        0b0101 => return null,
        0b1010 => return null,
        // 2-way right-angle collision.
        // give right of way to the one on the right.
        0b0011 => return 0,
        0b0110 => return 1,
        0b1100 => return 2,
        0b1001 => return 3,
        // 3-way collision. the middle one wins.
        0b1011 => return 0,
        0b0111 => return 1,
        0b1110 => return 2,
        0b1101 => return 3,
        // 4-way collision
        0b1111 => return null,
    }
}
