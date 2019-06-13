const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const makeCoord = core.geometry.makeCoord;

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
        try self.game_state.applyStateChanges([_]StateDiff{StateDiff{ .spawn = Individual{ .id = 1, .species = Species.human, .abs_position = makeCoord(7, 14) } }});
        //StateDiff{ .spawn = Individual{ .id = self.nextId(), .species = Species.orc, .abs_position = makeCoord(3, 2) } },
        //StateDiff{ .spawn = Individual{ .id = self.nextId(), .species = Species.orc, .abs_position = makeCoord(5, 2) } },
        //StateDiff{ .spawn = Individual{ .id = self.nextId(), .species = Species.orc, .abs_position = makeCoord(12, 2) } },
        //StateDiff{ .spawn = Individual{ .id = self.nextId(), .species = Species.orc, .abs_position = makeCoord(14, 2) } },
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
        individual_to_perception: IdMap(ArrayList(PerceivedFrame)),
        state_changes: []StateDiff,
    };

    /// computes what would happen but does not change the state of the engine.
    pub fn computeHappenings(self: *const GameEngine, actions: IdMap(Action)) !Happenings {
        // all game rules are here
        var individual_to_perception = IdMap(ArrayList(PerceivedFrame)).init(self.allocator);
        {
            var iterator = self.game_state.individuals.iterator();
            while (iterator.next()) |kv| {
                try individual_to_perception.putNoClobber(kv.key, ArrayList(PerceivedFrame).init(self.allocator));
            }
        }

        var action_iterator = actions.iterator();
        while (action_iterator.next()) |action_kv| {
            var actor = &self.game_state.individuals.get(action_kv.key).?.value;
            switch (action_kv.value) {
                .move => |direction| {
                    const from = actor.abs_position;
                    const to = from.plus(direction);
                },
                .attack => {
                    @panic("TODO");
                },
            }
        }

        return Happenings{
            .individual_to_perception = individual_to_perception,
            .state_changes = [_]StateDiff{},
        };
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
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(Individual),

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
            .individuals = IdMap(Individual).init(allocator),
        };
    }
    pub fn deinit(self: *GameState) void {
        self.allocator.free(self.individuals);
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
                    var abs_position = &self.individuals.get(id_and_coord.id).?.value.abs_position;
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
                    var abs_position = &self.individuals.get(id_and_coord.id).?.value.abs_position;
                    abs_position.* = abs_position.minus(id_and_coord.coord);
                },
            }
        }
    }
};
