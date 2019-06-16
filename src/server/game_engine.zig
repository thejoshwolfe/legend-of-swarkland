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
const StaticPerception = core.protocol.StaticPerception;

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return HashMap(u32, V, core.geometry.hashU32, std.hash_map.getTrivialEqlFn(u32));
}

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    game_state: GameState,
    history: HistoryList,
    next_id: u32,

    const HistoryList = std.TailQueue([]StateDiff);
    const HistoryNode = HistoryList.Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) !void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = GameState.init(allocator),
            .history = HistoryList.init(),
            .next_id = 1,
        };
        try self.game_state.applyStateChanges([_]StateDiff{
            StateDiff{ .spawn = Individual{ .id = 1, .species = .human, .abs_position = makeCoord(7, 14) } },
            StateDiff{ .spawn = Individual{ .id = 2, .species = .orc, .abs_position = makeCoord(3, 2) } },
        });
        //StateDiff{ .spawn = Individual{ .id = 3, .species = .orc, .abs_position = makeCoord(5, 2) } },
        //StateDiff{ .spawn = Individual{ .id = 4, .species = .orc, .abs_position = makeCoord(12, 2) } },
        //StateDiff{ .spawn = Individual{ .id = 5, .species = .orc, .abs_position = makeCoord(14, 2) } },
    }

    pub fn getStaticPerception(self: *const GameEngine, individual_id: usize) !StaticPerception {
        var yourself: StaticPerception.StaticIndividual = undefined;
        var others = ArrayList(StaticPerception.StaticIndividual).init(self.allocator);
        var iterator = self.game_state.individuals.iterator();
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
            .terrain = self.game_state.terrain,
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
    pub fn computeHappenings(self: *const GameEngine, actions: IdMap(Action)) !Happenings {
        // cache the set of keys so iterator is easier.
        const everybody = try self.allocator.alloc(u32, self.game_state.individuals.count());
        {
            var iterator = self.game_state.individuals.iterator();
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
            try current_positions.putNoClobber(id, self.game_state.individuals.getValue(id).?.abs_position);
        }

        var attacks = IdMap(Coord).init(self.allocator);

        for (everybody) |id| {
            var actor = &self.game_state.individuals.getValue(id).?;
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
                    everybody,
                    self.game_state.individuals.getValue(id).?,
                    individual_to_perception.getValue(id).?,
                    previous_moves,
                    next_moves,
                    current_positions,
                );
            }

            if (next_moves.count() == 0) break;

            // TODO: collision detection/resolution

            for (everybody) |id| {
                try next_positions.putNoClobber(id, current_positions.getValue(id).?.plus(next_moves.getValue(id) orelse zero_vector));
            }

            try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_moves = moves_history.at(moves_history.len - 1);
            previous_moves = moves_history.at(moves_history.len - 2);

            try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_positions = positions_history.at(positions_history.len - 1);
            current_positions = positions_history.at(positions_history.len - 2);
        }

        // TODO: handle attacks

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap([]PerceivedFrame).init(self.allocator);
                var iterator = individual_to_perception.iterator();
                for (everybody) |id| {
                    var perceived_frame = ArrayList(PerceivedFrame).init(self.allocator);
                    for (individual_to_perception.getValue(id).?.movement_frames.toSliceConst()) |movement_frame| {
                        if (movement_frame.len > 0) {
                            try perceived_frame.append(PerceivedFrame{
                                .individuals_by_location = movement_frame.toOwnedSlice(),
                            });
                        }
                    }
                    try ret.putNoClobber(id, perceived_frame.toOwnedSlice());
                }
                break :blk ret;
            },
            .state_changes = blk: {
                var ret = ArrayList(StateDiff).init(self.allocator);
                for (everybody) |id| {
                    const from = self.game_state.individuals.getValue(id).?.abs_position;
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
        everybody: []const u32,
        yourself: Individual,
        perception: *Perception,
        previous_moves: *const IdMap(Coord),
        next_moves: *const IdMap(Coord),
        current_positions: *const IdMap(Coord),
    ) !void {
        var movement_frame = try self.allocator.create(ArrayList(PerceivedFrame.IndividualWithMotion));
        movement_frame.* = ArrayList(PerceivedFrame.IndividualWithMotion).init(self.allocator);

        for (everybody) |other_id| {
            try movement_frame.append(PerceivedFrame.IndividualWithMotion{
                .prior_velocity = previous_moves.getValue(other_id) orelse zero_vector,
                .abs_position = current_positions.getValue(other_id).?,
                // TODO: does species belong here?
                .species = self.game_state.individuals.getValue(other_id).?.species,
                .next_velocity = next_moves.getValue(other_id) orelse zero_vector,
            });
        }

        try perception.movement_frames.append(movement_frame);
    }

    fn isOpenSpace(self: *const GameEngine, coord: Coord) bool {
        if (coord.x < 0 or coord.y < 0) return false;
        if (coord.x >= 16 or coord.y >= 16) return false;
        return self.game_state.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)] == Wall.air;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            .move => |direction| return isCardinalDirection(direction),
            .attack => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn applyStateChanges(self: *GameEngine, state_changes: []StateDiff) !void {
        try self.pushHistoryRecord(state_changes);
        try self.game_state.applyStateChanges(state_changes);

        // advance our next_id cursor
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    std.debug.assert(individual.id == self.next_id);
                    self.next_id = individual.id + 1;
                },
                else => {},
            }
        }
    }

    pub fn rewind(self: *GameEngine) ?[]StateDiff {
        if (self.history.len <= 1) {
            // that's enough pal.
            return null;
        }
        const node = self.history.pop() orelse return null;
        const state_changes = node.data;
        self.allocator.destroy(node);
        try self.game_state.undoEvents(state_changes);

        // move our next_id cursor backwards
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |individual| {
                    std.debug.assert(individual.id == self.next_id - 1);
                    self.next_id = Individual.id;
                },
                else => {},
            }
        }
        return state_changes;
    }

    fn pushHistoryRecord(self: *GameEngine, state_changes: []StateDiff) !void {
        const history_node: *HistoryNode = try self.allocator.create(HistoryNode);
        history_node.data = state_changes;
        self.history.append(history_node);
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
    terrain: Terrain,
    individuals: IdMap(Individual),

    pub fn init(allocator: *std.mem.Allocator) GameState {
        return GameState{
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
            .individuals = IdMap(Individual).init(allocator),
        };
    }
    pub fn deinit(self: *GameState) void {
        self.individuals.deinit();
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, individual);
                },
                .die => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .move => |id_and_coord| {
                    var abs_position = &self.individuals.getValue(id_and_coord.id).?.abs_position;
                    abs_position.* = abs_position.plus(id_and_coord.coord);
                },
            }
        }
    }
    fn undoEvents(self: *GameState, state_changes: []const StateDiff) void {
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .die => |individuall| {
                    try self.individuals.putNoClobber(individual.id, individual);
                },
                .move => |_| {
                    var abs_position = &self.individuals.getValue(id_and_coord.id).value.abs_position;
                    abs_position.* = abs_position.minus(id_and_coord.coord);
                },
            }
        }
    }
};

const Perception = struct {
    movement_frames: ArrayList(*ArrayList(PerceivedFrame.IndividualWithMotion)),
    pub fn init(allocator: *std.mem.Allocator) Perception {
        return Perception{
            .movement_frames = ArrayList(*ArrayList(PerceivedFrame.IndividualWithMotion)).init(allocator),
        };
    }
};
