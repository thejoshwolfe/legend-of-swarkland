const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const isScaledCardinalDirection = core.geometry.isScaledCardinalDirection;
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
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const applyMovementToPosition = core.game_logic.applyMovementToPosition;

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

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

/// Shallow copies the argument to a newly allocated pointer.
fn allocClone(allocator: *std.mem.Allocator, obj: var) !*@typeOf(obj) {
    var x = try allocator.create(@typeOf(obj));
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
    hatch_positions: []const Coord,
    lava_positions: []const Coord,
    individuals: []const Individual,
};
const the_levels = [_]Level{
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            makeIndividual(makeCoord(2, 2), .orc),
        },
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(4, 4)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            makeIndividual(makeCoord(3, 3), .orc),
            makeIndividual(makeCoord(3, 4), .orc),
            makeIndividual(makeCoord(4, 3), .orc),
            makeIndividual(makeCoord(5, 4), .orc),
        },
    },
    Level{
        .width = 8,
        .height = 7,
        .hatch_positions = [_]Coord{makeCoord(1, 3)},
        .lava_positions = [_]Coord{
            makeCoord(1, 1), makeCoord(2, 1), makeCoord(3, 1), makeCoord(4, 1), makeCoord(5, 1), makeCoord(6, 1),
            makeCoord(1, 5), makeCoord(2, 5), makeCoord(3, 5), makeCoord(4, 5), makeCoord(5, 5), makeCoord(6, 5),
        },
        .individuals = [_]Individual{
            makeIndividual(makeCoord(6, 2), .orc),
            makeIndividual(makeCoord(6, 3), .centaur),
            makeIndividual(makeCoord(6, 4), .orc),
        },
    },
    Level{
        .width = 11,
        .height = 8,
        .hatch_positions = [_]Coord{makeCoord(4, 2)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            makeIndividual(makeCoord(1, 1), .centaur),
            makeIndividual(makeCoord(9, 6), .centaur),
        },
    },
    Level{
        .width = 14,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(6, 2)},
        .lava_positions = [_]Coord{
            makeCoord(1, 1), makeCoord(2, 1), makeCoord(3, 1), makeCoord(4, 1),  makeCoord(5, 1),  makeCoord(6, 1),
            makeCoord(7, 1), makeCoord(8, 1), makeCoord(9, 1), makeCoord(10, 1), makeCoord(11, 1), makeCoord(12, 1),
            makeCoord(1, 8), makeCoord(2, 8), makeCoord(3, 8), makeCoord(4, 8),  makeCoord(5, 8),  makeCoord(6, 8),
            makeCoord(7, 8), makeCoord(8, 8), makeCoord(9, 8), makeCoord(10, 8), makeCoord(11, 8), makeCoord(12, 8),
        },
        .individuals = [_]Individual{
            makeIndividual(makeCoord(3, 7), .centaur),
            makeIndividual(makeCoord(4, 7), .centaur),
            makeIndividual(makeCoord(5, 7), .centaur),
            makeIndividual(makeCoord(6, 7), .centaur),
            makeIndividual(makeCoord(7, 7), .centaur),
            makeIndividual(makeCoord(8, 7), .centaur),
            makeIndividual(makeCoord(9, 7), .centaur),
        },
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(7, 5)},
        .lava_positions = [_]Coord{
            makeCoord(4, 4), makeCoord(4, 5), makeCoord(4, 6),
            makeCoord(5, 4), makeCoord(5, 5), makeCoord(5, 6),
        },
        .individuals = [_]Individual{
            makeIndividual(makeCoord(7, 3), .turtle),
            makeIndividual(makeCoord(2, 5), .turtle),
        },
    },
    Level{
        .width = 8,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(2, 4)},
        .lava_positions = [_]Coord{
            makeCoord(1, 1), makeCoord(2, 1), makeCoord(3, 1), makeCoord(4, 1), makeCoord(5, 1), makeCoord(6, 1),
            makeCoord(1, 8), makeCoord(2, 8), makeCoord(3, 8), makeCoord(4, 8), makeCoord(5, 8), makeCoord(6, 8),
        },
        .individuals = [_]Individual{
            makeLargeIndividual(makeCoord(5, 3), makeCoord(6, 3), .rhino),
        },
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(4, 7)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            makeLargeIndividual(makeCoord(3, 2), makeCoord(2, 2), .rhino),
            makeIndividual(makeCoord(7, 5), .centaur),
        },
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(4, 7)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            makeLargeIndividual(makeCoord(3, 2), makeCoord(2, 2), .rhino),
            makeIndividual(makeCoord(7, 5), .orc),
        },
    },
    // the last level must have no enemies so that you can't win it.
    Level{
        .width = 15,
        .height = 10,
        .hatch_positions = [_]Coord{
            makeCoord(2, 2), makeCoord(3, 3), makeCoord(4, 2), makeCoord(3, 4),
        } ++ [_]Coord{
            makeCoord(7, 2), makeCoord(8, 3), makeCoord(7, 4), makeCoord(6, 3),
        } ++ [_]Coord{
            makeCoord(10, 2), makeCoord(10, 3), makeCoord(11, 4), makeCoord(12, 4), makeCoord(12, 3), makeCoord(12, 2),
        } ++ [_]Coord{
            makeCoord(2, 6), makeCoord(2, 7), makeCoord(3, 8), makeCoord(4, 7), makeCoord(5, 8), makeCoord(6, 7), makeCoord(6, 6),
        } ++ [_]Coord{
            makeCoord(8, 6), makeCoord(8, 7), makeCoord(8, 8),
        } ++ [_]Coord{
            makeCoord(10, 6), makeCoord(10, 7), makeCoord(10, 8), makeCoord(11, 6), makeCoord(12, 7), makeCoord(12, 8),
        },
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{},
    },
};

fn buildTheTerrain(allocator: *std.mem.Allocator) !Terrain {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        height = std.math.max(height, level.height);
    }

    var terrain = try Terrain.initFill(allocator, width, height, TerrainSpace{
        .floor = .dirt,
        .wall = .air,
    });
    const border_wall = TerrainSpace{
        .floor = .unknown,
        .wall = .stone,
    };

    var level_x: u16 = 0;
    for (the_levels) |level| {
        defer level_x += level.width;
        {
            var x: u16 = 0;
            while (x < level.width) : (x += 1) {
                terrain.atUnchecked(level_x + x, 0).* = border_wall;
                var y: u16 = level.height - 1;
                while (y < height) : (y += 1) {
                    terrain.atUnchecked(level_x + x, y).* = border_wall;
                }
            }
        }
        {
            var y: u16 = 1;
            while (y < level.height - 1) : (y += 1) {
                terrain.atUnchecked(level_x + 0, y).* = border_wall;
                terrain.atUnchecked(level_x + level.width - 1, y).* = border_wall;
            }
        }
        for (level.hatch_positions) |coord| {
            terrain.at(level_x + @intCast(u16, coord.x), coord.y).?.floor = .hatch;
        }
        for (level.lava_positions) |coord| {
            terrain.at(level_x + @intCast(u16, coord.x), coord.y).?.floor = .lava;
        }
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

        var moves_history = ArrayList(*IdMap(Coord)).init(self.allocator);
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        var next_moves = moves_history.at(moves_history.len - 1);
        var previous_moves = moves_history.at(moves_history.len - 2);

        var positions_history = ArrayList(*IdMap(ThingPosition)).init(self.allocator);
        try positions_history.append(try createInit(self.allocator, IdMap(ThingPosition)));
        try positions_history.append(try createInit(self.allocator, IdMap(ThingPosition)));
        var next_positions = positions_history.at(positions_history.len - 1);
        var current_positions = positions_history.at(positions_history.len - 2);

        for (everybody) |id| {
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, MutablePerceivedHappening));
            try current_positions.putNoClobber(id, game_state.individuals.getValue(id).?.abs_position);
        }

        var total_deaths = IdMap(void).init(self.allocator);

        var trample_deaths = IdMap(void).init(self.allocator);

        for (everybody) |id| {
            switch (actions.getValue(id).?) {
                .move => |move_delta| {
                    try next_moves.putNoClobber(id, move_delta);
                },
                .fast_move => |move_delta| {
                    try next_moves.putNoClobber(id, move_delta);
                },
                else => {},
            }
        }

        while (true) {
            // Adjust next_moves so that they aren't entering walls.
            for (everybody) |id| {
                var move_delta = next_moves.getValue(id) orelse continue;
                const current_position = current_positions.getValue(id).?;
                var anything_changed = false;
                while (true) {
                    var next_position = applyMovementToPosition(current_position, move_delta);
                    if (core.game_logic.isOpenSpace((game_state.terrain.getCoord(getHeadPosition(next_position)) orelse oob_terrain).wall)) {
                        // This move is allowed.
                        if (anything_changed) {
                            _ = next_moves.putAssumeCapacity(id, move_delta);
                        }
                        break;
                    }
                    // Can't move into a wall.
                    if (move_delta.magnitudeSquared() > 1) {
                        // Fast move into a wall. Try moving less to snap against the wall.
                        move_delta = move_delta.minus(move_delta.signumed());
                        anything_changed = true;
                        continue;
                    }
                    // Normal move into a wall. Cancel movement.
                    next_moves.removeAssertDiscard(id);
                    break;
                }
            }

            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities{
                        .movement = Activities.Movement{
                            .previous_moves = previous_moves,
                            .next_moves = next_moves,
                        },
                    },
                );
                if (trample_deaths.count() != 0) {
                    try self.observeFrame(
                        game_state,
                        id,
                        individual_to_perception.getValue(id).?,
                        current_positions,
                        Activities{ .deaths = &trample_deaths },
                    );
                }
            }
            try flushDeaths(&total_deaths, &trample_deaths, &everybody);

            // Check for steady state.
            if (next_moves.count() == 0) break;

            // Record everyone's intended movement.
            for (everybody) |id| {
                const current_position = current_positions.getValue(id).?;
                const next_position = if (next_moves.getValue(id)) |move_delta|
                    applyMovementToPosition(current_position, move_delta)
                else
                    current_position;
                try next_positions.putNoClobber(id, next_position);
            }

            // Commit the intended movements.

            try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_moves = moves_history.at(moves_history.len - 1);
            previous_moves = moves_history.at(moves_history.len - 2);

            try positions_history.append(try createInit(self.allocator, IdMap(ThingPosition)));
            next_positions = positions_history.at(positions_history.len - 1);
            current_positions = positions_history.at(positions_history.len - 2);

            // ==================================
            // Collision detection and resolution
            // ==================================
            var collision_counter = CoordMap(usize).init(self.allocator);

            for (everybody) |id| {
                const position = current_positions.getValue(id).?;
                // You can't be in a wall.
                std.debug.assert(core.game_logic.isOpenSpace((game_state.terrain.getCoord(getHeadPosition(position)) orelse oob_terrain).wall));

                // Count how many people are in this one spot.
                for (getAllPositions(position)) |coord| {
                    _ = try collision_counter.put(coord, 1 + (collision_counter.getValue(coord) orelse 0));
                }
            }

            {
                var ids = ArrayList(u32).init(self.allocator);
                var iterator = collision_counter.iterator();
                while (iterator.next()) |kv| {
                    if (kv.value <= 1) continue;
                    const here_coord = kv.key;
                    // find individuals involved in this collision
                    var large_boi_count: usize = 0;
                    ids.shrink(0);
                    for (everybody) |id| {
                        switch (current_positions.getValue(id).?) {
                            .small => |coord| if (here_coord.equals(coord)) try ids.append(id),
                            .large => |coords| if (here_coord.equals(coords[0]) or here_coord.equals(coords[1])) {
                                large_boi_count += 1;
                                try ids.append(id);
                            },
                        }
                    }

                    if (large_boi_count > 0) {
                        // large boi tramples/gores everyone else.
                        for (ids.toSliceConst()) |me| {
                            if (current_positions.getValue(me).? == .large) {
                                if (large_boi_count >= 2) {
                                    // large boi gets trampled by other large bois
                                    _ = try trample_deaths.put(me, {});
                                } else {
                                    // large boi is unaffected
                                }
                            } else {
                                // small bois get wrecked
                                _ = try trample_deaths.put(me, {});
                            }
                        }
                    } else {
                        // bounce off of each other.
                        for (ids.toSliceConst()) |me| {
                            // consider forces from everyone but yourself
                            var external_force: u4 = 0;
                            for (ids.toSliceConst()) |id| {
                                if (id == me) continue;
                                const prior_velocity = previous_moves.getValue(id) orelse zero_vector;
                                external_force |= core.geometry.directionToCardinalBitmask(prior_velocity);
                            }
                            if (core.geometry.cardinalBitmaskToDirection(external_force)) |push_velocity| {
                                try next_moves.putNoClobber(me, push_velocity);
                            } else {
                                // clusterfuck. reverse course.
                                try next_moves.putNoClobber(me, (previous_moves.getValue(me) orelse zero_vector).negated());
                            }
                        }
                    }
                }
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
                    for (getAllPositions(current_positions.getValue(other_id).?)) |coord, i| {
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
            for (getAllPositions(current_positions.getValue(id).?)) |coord| {
                if ((game_state.terrain.getCoord(coord) orelse oob_terrain).floor == .lava) {
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
                    current_positions,
                    Activities{ .attacks = &attacks },
                );
            }
            if (attack_deaths.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities{ .deaths = &attack_deaths },
                );
            }
        }
        try flushDeaths(&total_deaths, &attack_deaths, &everybody);

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
                for (getAllPositions(current_positions.getValue(id).?)) |coord| {
                    if ((game_state.terrain.getCoord(coord) orelse oob_terrain).floor == .hatch) {
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
            var level_x: u16 = 0;
            for (the_levels[0..game_state.level_number]) |level| {
                level_x += level.width;
            }
            const level = the_levels[game_state.level_number];
            const the_way_y = the_levels[game_state.level_number + 1].hatch_positions[0].y;
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
            }
            // close the way
            const the_way_y = the_levels[new_level_number].hatch_positions[0].y;
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

    const Activities = union(enum) {
        static_state,
        movement: Movement,
        const Movement = struct {
            previous_moves: *const IdMap(Coord),
            next_moves: *const IdMap(Coord),
        };

        attacks: *const IdMap(Attack),
        const Attack = struct {
            direction: Coord,
            distance: i32,
        };

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
                .movement => |data| blk: {
                    const prior_velocity = data.previous_moves.getValue(id) orelse zero_vector;
                    const next_velocity = data.next_moves.getValue(id) orelse zero_vector;
                    if (prior_velocity.equals(zero_vector) and next_velocity.equals(zero_vector)) {
                        break :blk PerceivedActivity{ .none = {} };
                    }
                    const a = PerceivedActivity{
                        .movement = PerceivedActivity.Movement{
                            .prior_velocity = prior_velocity,
                            .next_velocity = next_velocity,
                        },
                    };
                    break :blk a;
                },

                .attacks => |data| if (data.getValue(id)) |attack|
                    PerceivedActivity{
                        .attack = PerceivedActivity.Attack{
                            .direction = attack.direction,
                            .distance = attack.distance,
                        },
                    }
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
            const within_view = blk: for (getAllPositions(rel_position)) |delta| {
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
