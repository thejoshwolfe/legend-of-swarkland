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
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;
const StaticPerception = core.protocol.StaticPerception;

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

// TODO: sort all arrays to hide iteration order from the server
pub const GameEngine = struct {
    allocator: *std.mem.Allocator,

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{ .allocator = allocator };
    }

    pub fn getStartGameHappenings(self: *const GameEngine) !Happenings {
        var individuals = ArrayList(Individual).init(self.allocator);
        try individuals.append(Individual{ .id = 1, .species = .human, .abs_position = makeCoord(7, 14) });
        try individuals.append(Individual{ .id = 2, .species = .orc, .abs_position = makeCoord(6, 13) });
        try individuals.append(Individual{ .id = 3, .species = .snake, .abs_position = makeCoord(5, 11) });
        try individuals.append(Individual{ .id = 4, .species = .ogre, .abs_position = makeCoord(5, 10) });
        try individuals.append(Individual{ .id = 5, .species = .ant, .abs_position = makeCoord(5, 8) });
        return Happenings{
            // TODO: maybe put static perception in here or something.
            .individual_to_perception = IdMap(PerceivedHappening).init(self.allocator),
            .state_changes = blk: {
                var arr = try self.allocator.alloc(StateDiff, individuals.len);
                for (arr) |*x, i| {
                    x.* = StateDiff{ .spawn = individuals.at(i) };
                }
                break :blk arr;
            },
        };
    }

    pub fn getStaticPerception(self: *const GameEngine, game_state: GameState, individual_id: usize) !StaticPerception {
        var yourself: StaticPerception.StaticIndividual = undefined;
        var others = ArrayList(StaticPerception.StaticIndividual).init(self.allocator);
        var iterator = game_state.individuals.iterator();
        while (iterator.next()) |kv| {
            if (kv.key == individual_id) {
                yourself = StaticPerception.StaticIndividual{
                    .species = kv.value.species,
                    .abs_position = kv.value.abs_position,
                };
            } else {
                try others.append(StaticPerception.StaticIndividual{
                    .species = kv.value.species,
                    .abs_position = kv.value.abs_position,
                });
            }
        }
        return StaticPerception{
            .terrain = game_state.terrain,
            .self = yourself,
            .others = others.toOwnedSlice(),
        };
    }

    pub const Happenings = struct {
        individual_to_perception: IdMap(PerceivedHappening),
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

        var individual_to_perception = IdMap(*Perception).init(self.allocator);

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
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, Perception));
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
                try self.observeMovement(
                    game_state,
                    everybody,
                    game_state.individuals.getValue(id).?,
                    individual_to_perception.getValue(id).?,
                    previous_moves,
                    next_moves,
                    current_positions,
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
                if (!isOpenSpace(game_state.wallAt(position))) {
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

        // TODO: handle attacks

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap(PerceivedHappening).init(self.allocator);
                var iterator = individual_to_perception.iterator();
                for (everybody) |id| {
                    var perceived_frame = ArrayList(PerceivedFrame).init(self.allocator);
                    for (individual_to_perception.getValue(id).?.movement_frames.toSliceConst()) |movement_frame| {
                        if (movement_frame.len > 0) {
                            try perceived_frame.append(PerceivedFrame{
                                .movements = movement_frame.toOwnedSlice(),
                            });
                        }
                    }
                    try ret.putNoClobber(id, PerceivedHappening{ .frames = perceived_frame.toOwnedSlice() });
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
                break :blk ret.toOwnedSlice();
            },
        };
    }

    fn observeMovement(
        self: *const GameEngine,
        game_state: GameState,
        everybody: []const u32,
        yourself: *const Individual,
        perception: *Perception,
        previous_moves: *const IdMap(Coord),
        next_moves: *const IdMap(Coord),
        current_positions: *const IdMap(Coord),
    ) !void {
        var movement_frame = try self.allocator.create(ArrayList(PerceivedFrame.PerceivedMovement));
        movement_frame.* = ArrayList(PerceivedFrame.PerceivedMovement).init(self.allocator);

        for (everybody) |other_id| {
            try movement_frame.append(PerceivedFrame.PerceivedMovement{
                .prior_velocity = previous_moves.getValue(other_id) orelse zero_vector,
                .abs_position = current_positions.getValue(other_id).?,
                // TODO: does species belong here?
                .species = game_state.individuals.getValue(other_id).?.species,
                .next_velocity = next_moves.getValue(other_id) orelse zero_vector,
            });
        }

        try perception.movement_frames.append(movement_frame);
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            .move => |direction| return isCardinalDirection(direction),
            .attack => |direction| return isCardinalDirection(direction),
        }
    }
};

pub const HistoryFrame = struct {
    event: []Event,
};

pub const Individual = struct {
    id: u32,
    species: Species,
    abs_position: Coord,
};
pub const StateDiff = union(enum) {
    spawn: Individual,
    die: Individual,
    move: IdAndCoord,

    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),

    pub fn init(allocator: *std.mem.Allocator) GameState {
        return GameState{
            .allocator = allocator,
            .terrain = blk: {
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
                break :blk terrain;
            },
            .individuals = IdMap(*Individual).init(allocator),
        };
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try clone(self.allocator, individual));
                },
                .die => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.plus(id_and_coord.coord);
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
                .die => |individual| {
                    try self.individuals.putNoClobber(individual.id, try clone(self.allocator, individual));
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.minus(id_and_coord.coord);
                },
            }
        }
    }

    pub fn wallAt(self: GameState, coord: Coord) Wall {
        if (coord.x < 0 or coord.y < 0) return .unknown;
        if (coord.x >= 16 or coord.y >= 16) return .unknown;
        return self.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)];
    }
};

const Perception = struct {
    movement_frames: ArrayList(*ArrayList(PerceivedFrame.PerceivedMovement)),
    pub fn init(allocator: *std.mem.Allocator) Perception {
        return Perception{
            .movement_frames = ArrayList(*ArrayList(PerceivedFrame.PerceivedMovement)).init(allocator),
        };
    }
};

fn isOpenSpace(wall: Wall) bool {
    return wall == Wall.air;
}
