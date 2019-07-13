const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const makeCoord = core.geometry.makeCoord;
const zero_vector = makeCoord(0, 0);

const Action = core.protocol.Action;
const Terrain = core.protocol.Terrain;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const PerceivedActivity = core.protocol.PerceivedActivity;

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return HashMap(u32, V, core.geometry.hashU32, std.hash_map.getTrivialEqlFn(u32));
}

pub fn CoordMap(comptime V: type) type {
    return HashMap(Coord, V, Coord.hash, Coord.equals);
}

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

/// Shallow copies the argument to a newly allocated pointer.
pub fn clone(allocator: *std.mem.Allocator, obj: var) !*@typeOf(obj) {
    var x = try allocator.create(@typeOf(obj));
    x.* = obj;
    return x;
}

const Level = struct {
    individuals: []const Individual,
    hatch_positions: []const Coord,
};
const the_levels = [_]Level{
    Level{
        .individuals = [_]Individual{Individual{ .id = 0, .abs_position = makeCoord(2, 2), .species = .orc }},
        .hatch_positions = [_]Coord{makeCoord(7, 5)},
    },
    Level{
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(2, 2), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(12, 2), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(7, 8), .species = .orc },
        },
        .hatch_positions = [_]Coord{makeCoord(7, 5)},
    },
    Level{
        .individuals = [_]Individual{
            // human at 7,5
            Individual{ .id = 0, .abs_position = makeCoord(2, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(3, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 4), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(7, 6), .species = .orc },
        },
        .hatch_positions = [_]Coord{makeCoord(6, 2)},
    },
    Level{
        .individuals = [_]Individual{
            // human at 6,2
            Individual{ .id = 0, .abs_position = makeCoord(11, 1), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(11, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(11, 3), .species = .orc },
        },
        .hatch_positions = [_]Coord{makeCoord(6, 6)},
    },
    Level{
        .individuals = [_]Individual{
            // human at 6, 6
            Individual{ .id = 0, .abs_position = makeCoord(3, 5), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(11, 10), .species = .centaur },
        },
        .hatch_positions = [_]Coord{makeCoord(7, 7)},
    },
    Level{
        .individuals = [_]Individual{
            // human at 7,7
            Individual{ .id = 0, .abs_position = makeCoord(4, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(5, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(6, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(7, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(8, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(9, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(10, 2), .species = .centaur },
        },
        .hatch_positions = [_]Coord{makeCoord(7, 7)},
    },
    Level{
        .individuals = [_]Individual{
            // human at 7,7
            Individual{ .id = 0, .abs_position = makeCoord(5, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 7), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 8), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 9), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 7), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 8), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 9), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(10, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(9, 1), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(5, 1), .species = .centaur },
        },
        .hatch_positions = [_]Coord{makeCoord(8, 6)},
    },
    // the last level must have no enemies so that you can't win it.
    Level{
        .individuals = [_]Individual{},
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
    },
};

fn assignId(individual: Individual, id: u32) Individual {
    var ret = individual;
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
            .move => |direction| return isCardinalDirection(direction),
            .attack => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn getStartGameHappenings(self: *const GameEngine) !Happenings {
        return Happenings{
            .individual_to_perception = IdMap([]PerceivedFrame).init(self.allocator),
            .state_changes = blk: {
                var ret = ArrayList(StateDiff).init(self.allocator);
                // human is always id 1
                try ret.append(StateDiff{ .spawn = Individual{ .id = 1, .abs_position = makeCoord(7, 7), .species = .human } });
                for (the_levels[0].individuals) |individual, i| {
                    try ret.append(StateDiff{ .spawn = assignId(individual, @intCast(u32, i) + 2) });
                }
                try ret.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .from = makeTerrain([_]Coord{}, false),
                        .to = makeTerrain(the_levels[0].hatch_positions, false),
                    },
                });
                break :blk ret.toOwnedSlice();
            },
        };
    }

    pub fn getStaticPerception(self: *const GameEngine, game_state: GameState, individual_id: u32) !PerceivedFrame {
        var yourself: ?PerceivedThing = null;
        var others = ArrayList(PerceivedThing).init(self.allocator);
        var iterator = game_state.individuals.iterator();
        while (iterator.next()) |kv| {
            if (kv.key == individual_id) {
                yourself = PerceivedThing{
                    .species = kv.value.species,
                    .abs_position = kv.value.abs_position,
                    .activity = .none,
                };
            } else {
                try others.append(PerceivedThing{
                    .species = kv.value.species,
                    .abs_position = kv.value.abs_position,
                    .activity = .none,
                });
            }
        }
        return PerceivedFrame{
            .terrain = game_state.terrain,
            .self = yourself,
            .others = others.toOwnedSlice(),
        };
    }

    pub const Happenings = struct {
        individual_to_perception: IdMap([]PerceivedFrame),
        state_changes: []StateDiff,
    };

    /// Computes what would happen but does not change the state of the engine.
    /// This is the entry point for all game rules.
    pub fn computeHappenings(self: *const GameEngine, game_state: GameState, actions: IdMap(Action)) !Happenings {
        // cache the set of keys so iterator is easier.
        const everybody = try self.allocator.alloc(u32, game_state.individuals.count());
        {
            var iterator = game_state.individuals.iterator();
            for (everybody) |*x| {
                x.* = iterator.next().?.key;
            }
            std.debug.assert(iterator.next() == null);
        }

        var individual_to_perception = IdMap(*MutablePerceivedHappening).init(self.allocator);

        var moves_history = ArrayList(*IdMap(Coord)).init(self.allocator);
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        var next_moves = moves_history.at(moves_history.len - 1);
        var previous_moves = moves_history.at(moves_history.len - 2);

        var positions_history = ArrayList(*IdMap(Coord)).init(self.allocator);
        try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
        try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
        var next_positions = positions_history.at(positions_history.len - 1);
        var current_positions = positions_history.at(positions_history.len - 2);

        for (everybody) |id| {
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, MutablePerceivedHappening));
            try current_positions.putNoClobber(id, game_state.individuals.getValue(id).?.abs_position);
        }

        var attacks = IdMap(Coord).init(self.allocator);

        for (everybody) |id| {
            var actor = game_state.individuals.getValue(id).?;
            switch (actions.getValue(id).?) {
                .move => |direction| {
                    try next_moves.putNoClobber(id, direction);
                },
                .attack => |direction| {
                    try attacks.putNoClobber(id, direction);
                },
            }
        }

        while (true) {
            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    everybody,
                    current_positions,
                    Activities{
                        .movement = Activities.Movement{
                            .previous_moves = previous_moves,
                            .next_moves = next_moves,
                        },
                    },
                );
            }

            if (next_moves.count() == 0) break;

            for (everybody) |id| {
                try next_positions.putNoClobber(id, current_positions.getValue(id).?.plus(next_moves.getValue(id) orelse zero_vector));
            }

            try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_moves = moves_history.at(moves_history.len - 1);
            previous_moves = moves_history.at(moves_history.len - 2);

            try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_positions = positions_history.at(positions_history.len - 1);
            current_positions = positions_history.at(positions_history.len - 2);

            // ==================================
            // Collision detection and resolution
            // ==================================
            var collision_counter = CoordMap(usize).init(self.allocator);

            for (everybody) |id| {
                const position = current_positions.getValue(id).?;
                // walls
                if (!core.game_logic.isOpenSpace(game_state.wallAt(position))) {
                    // bounce off the wall
                    try next_moves.putNoClobber(id, previous_moves.getValue(id).?.negated());
                }
                // and count entity collision
                _ = try collision_counter.put(position, 1 + (collision_counter.getValue(position) orelse 0));
            }

            {
                var ids = ArrayList(u32).init(self.allocator);
                var iterator = collision_counter.iterator();
                while (iterator.next()) |kv| {
                    if (kv.value <= 1) continue;
                    const position = kv.key;
                    // collect the individuals involved in this collision
                    ids.shrink(0);
                    for (everybody) |id| {
                        if (!current_positions.getValue(id).?.equals(position)) continue;
                        try ids.append(id);
                    }

                    // treat each individual separately
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

        // Attacks
        var deaths = IdMap(void).init(self.allocator);
        for (everybody) |id| {
            var attack_direction = attacks.getValue(id) orelse continue;
            var attacker_position = current_positions.getValue(id).?;
            var attack_distance: i32 = 1;
            const range = core.game_logic.getAttackRange(game_state.individuals.getValue(id).?.species);
            while (attack_distance <= range) : (attack_distance += 1) {
                var damage_position = attacker_position.plus(attack_direction.scaled(attack_distance));
                for (everybody) |other_id| {
                    if (current_positions.getValue(other_id).?.equals(damage_position)) {
                        _ = try deaths.put(other_id, {});
                    }
                }
            }
        }
        for (everybody) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.getValue(id).?,
                everybody,
                current_positions,
                Activities{ .attacks = &attacks },
            );
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.getValue(id).?,
                everybody,
                current_positions,
                Activities{ .deaths = &deaths },
            );
        }

        var spawn_the_stairs = false;
        var do_transition = false;
        var current_level_number = game_state.level_number;
        if (game_state.individuals.count() - deaths.count() <= 1) {
            // Only one person left. You win!
            if (deaths.count() > 0) {
                // Spawn the stairs onward.
                spawn_the_stairs = true;
            }
            // check for someone on the stairs
            for (everybody) |id| {
                if (deaths.contains(id)) continue;
                if (game_state.floorAt(current_positions.getValue(id).?) == if (spawn_the_stairs) Floor.hatch else Floor.stairs_down) {
                    do_transition = true;
                    current_level_number += 1;
                }
            }
        }

        // spawn new level
        var new_id_cursor: u32 = @intCast(u32, game_state.individuals.count());

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap([]PerceivedFrame).init(self.allocator);
                for (everybody) |id| {
                    try ret.putNoClobber(id, individual_to_perception.getValue(id).?.frames.toOwnedSlice());
                }
                break :blk ret;
            },
            .state_changes = blk: {
                var ret = ArrayList(StateDiff).init(self.allocator);
                for (everybody) |id| {
                    const from = game_state.individuals.getValue(id).?.abs_position;
                    const to = current_positions.getValue(id).?;
                    if (to.equals(from)) continue;
                    const delta = to.minus(from);
                    try ret.append(StateDiff{
                        .move = StateDiff.IdAndCoord{
                            .id = id,
                            .coord = delta,
                        },
                    });
                }
                {
                    var iterator = deaths.iterator();
                    while (iterator.next()) |kv| {
                        try ret.append(StateDiff{
                            .despawn = blk: {
                                var individual = game_state.individuals.getValue(kv.key).?.*;
                                individual.abs_position = current_positions.getValue(individual.id).?;
                                break :blk individual;
                            },
                        });
                    }
                }
                if (spawn_the_stairs or do_transition) {
                    try ret.append(StateDiff{
                        .terrain_update = StateDiff.TerrainDiff{
                            .from = game_state.terrain,
                            .to = blk: {
                                if (do_transition) {
                                    break :blk makeTerrain(the_levels[game_state.level_number + 1].hatch_positions, false);
                                } else {
                                    break :blk makeTerrain(the_levels[game_state.level_number].hatch_positions, true);
                                }
                            },
                        },
                    });
                }
                if (do_transition) {
                    try ret.append(StateDiff.transition_to_next_level);
                    for (the_levels[game_state.level_number + 1].individuals) |individual| {
                        const id = findAvailableId(&new_id_cursor, game_state.individuals);
                        try ret.append(StateDiff{ .spawn = assignId(individual, id) });
                    }
                }
                break :blk ret.toOwnedSlice();
            },
        };
    }

    const Activities = union(enum) {
        movement: Movement,
        const Movement = struct {
            previous_moves: *const IdMap(Coord),
            next_moves: *const IdMap(Coord),
        };

        attacks: *const IdMap(Coord),
        deaths: *const IdMap(void),
    };
    fn observeFrame(
        self: *const GameEngine,
        game_state: GameState,
        my_id: u32,
        perception: *MutablePerceivedHappening,
        everybody: []const u32,
        current_positions: *const IdMap(Coord),
        activities: Activities,
    ) !void {
        var yourself: ?PerceivedThing = null;
        var others = ArrayList(PerceivedThing).init(self.allocator);

        for (everybody) |id| {
            const activity = switch (activities) {
                .movement => |data| blk: {
                    const a = PerceivedActivity{
                        .movement = PerceivedActivity.Movement{
                            .prior_velocity = data.previous_moves.getValue(id) orelse zero_vector,
                            .next_velocity = data.next_moves.getValue(id) orelse zero_vector,
                        },
                    };
                    break :blk a;
                },

                .attacks => |data| if (data.getValue(id)) |direction|
                    PerceivedActivity{
                        .attack = PerceivedActivity.Attack{ .direction = direction },
                    }
                else
                    PerceivedActivity{ .none = {} },

                .deaths => |data| if (data.getValue(id)) |_|
                    PerceivedActivity{ .death = {} }
                else
                    PerceivedActivity{ .none = {} },
            };
            const thing = PerceivedThing{
                .species = game_state.individuals.getValue(id).?.species,
                .abs_position = current_positions.getValue(id).?,
                .activity = activity,
            };
            if (id == my_id) {
                yourself = thing;
            } else {
                try others.append(thing);
            }
        }

        try perception.frames.append(PerceivedFrame{
            .self = yourself,
            .others = others.toOwnedSlice(),
            .terrain = game_state.terrain,
        });
    }
};

pub const Individual = struct {
    id: u32,
    species: Species,
    abs_position: Coord,
};
pub const StateDiff = union(enum) {
    spawn: Individual,
    despawn: Individual,
    move: IdAndCoord,
    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };

    terrain_update: TerrainDiff,
    pub const TerrainDiff = struct {
        from: Terrain,
        to: Terrain,
    };

    transition_to_next_level,
};

fn makeTerrain(hatch_positions: []const Coord, turn_hatches_into_stairs: bool) Terrain {
    // someday move this outta here
    var terrain: Terrain = undefined;
    {
        var x: usize = 0;
        while (x < 16) : (x += 1) {
            terrain.floor[0][x] = Floor.unknown;
            terrain.floor[16 - 1][x] = Floor.unknown;
            terrain.walls[0][x] = Wall.stone;
            terrain.walls[16 - 1][x] = Wall.stone;
        }
    }
    {
        var y: usize = 1;
        while (y < 16 - 1) : (y += 1) {
            terrain.floor[y][0] = Floor.unknown;
            terrain.floor[y][16 - 1] = Floor.unknown;
            terrain.walls[y][0] = Wall.stone;
            terrain.walls[y][16 - 1] = Wall.stone;
        }
    }
    {
        var y: usize = 1;
        while (y < 16 - 1) : (y += 1) {
            var x: usize = 1;
            while (x < 16 - 1) : (x += 1) {
                terrain.floor[y][x] = Floor.dirt;
                terrain.walls[y][x] = Wall.air;
            }
        }
    }
    const hatch_or_stairs = if (turn_hatches_into_stairs) Floor.stairs_down else Floor.hatch;
    for (hatch_positions) |coord| {
        terrain.floor[@intCast(usize, coord.y)][@intCast(usize, coord.x)] = hatch_or_stairs;
    }
    return terrain;
}

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),
    level_number: usize,

    pub fn init(allocator: *std.mem.Allocator) GameState {
        return GameState{
            .allocator = allocator,
            .terrain = makeTerrain([_]Coord{}, false),
            .individuals = IdMap(*Individual).init(allocator),
            .level_number = 0,
        };
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try clone(self.allocator, individual));
                },
                .despawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.plus(id_and_coord.coord);
                },
                .terrain_update => |terrain_diff| {
                    self.terrain = terrain_diff.to;
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
                    try self.individuals.putNoClobber(individual.id, try clone(self.allocator, individual));
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.minus(id_and_coord.coord);
                },
                .terrain_update => |terrain_diff| {
                    self.terrain = terrain_diff.from;
                },
                .transition_to_next_level => {
                    self.level_number -= 1;
                },
            }
        }
    }

    pub fn wallAt(self: GameState, coord: Coord) Wall {
        if (coord.x < 0 or coord.y < 0) return .unknown;
        if (coord.x >= 16 or coord.y >= 16) return .unknown;
        return self.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)];
    }
    pub fn floorAt(self: GameState, coord: Coord) Floor {
        if (coord.x < 0 or coord.y < 0) return .unknown;
        if (coord.x >= 16 or coord.y >= 16) return .unknown;
        return self.terrain.floor[@intCast(usize, coord.y)][@intCast(usize, coord.x)];
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
