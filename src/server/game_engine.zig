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

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    game_state: GameState,
    history: HistoryList,

    const HistoryList = std.LinkedList(void); // TODO
    const HistoryNode = HistoryList.Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) !void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = try GameState.init(allocator),
            .history = HistoryList.init(),
        };
    }

    pub const Happenings = struct {
        individual_perception_frames: [][]PerceivedFrame,
        state_changes: void, // TODO

        pub fn deinit(self: Happenings, allocator: *std.mem.Allocator) void {
            for (self.individual_perception_frames) |perception_frames| {
                for (perception_frames) |frame| {
                    frame.deinit(allocator);
                }
                allocator.free(perception_frames);
            }
            allocator.free(self.individual_perception_frames);
        }
    };

    /// computes what would happen but does not change the state of the engine.
    pub fn computeHappenings(self: *const GameEngine, actions: []const Action) !Happenings {
        // all game rules are here

        // TODO: this is just to have an inferred error
        self.allocator.free(try self.allocator.alloc(usize, 1));

        // TODO: do anything here.
        return Happenings{
            .individual_perception_frames = [_][]PerceivedFrame{},
            .state_changes = {},
        };
    }

    pub fn applyStateChanges(self: *GameEngine, state_changes: void) !void {
        try self.pushHistoryRecord(state_changes);
        try self.game_state.applyStateChanges(state_changes);
    }

    fn isOpenSpace(self: *const GameEngine, coord: Coord) bool {
        if (coord.x < 0 or coord.y < 0) return false;
        if (coord.x >= 16 or coord.y >= 16) return false;
        return self.game_state.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)] == Wall.air;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            Action.move => |direction| return isCardinalDirection(direction),
            Action.attack => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn rewind(self: *GameEngine) ?void {
        if (self.history.len <= 1) {
            // that's enough pal.
            return null;
        }
        const node = self.history.pop() orelse return null;
        const state_changes = node.data;
        self.allocator.destroy(node);
        try self.game_state.undoEvents(state_changes);
        return state_changes;
    }

    fn pushHistoryRecord(self: *GameEngine, state_changes: void) !void {
        const history_node: *HistoryNode = try self.allocator.create(HistoryNode);
        history_node.data = state_changes;
        self.history.append(history_node);
    }
};

pub const HistoryFrame = struct {
    event: []Event,
};

pub const Individual = struct {
    species: Species,
    abs_position: Coord,
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: []Individual,

    pub fn init(allocator: *std.mem.Allocator) !GameState {
        return GameState{
            .allocator = allocator,
            .terrain = blk: {
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
            .individuals = blk: {
                var individuals = try std.mem.dupe(allocator, Individual, [_]Individual{Individual{ .species = Species.human, .abs_position = makeCoord(7, 14) }});
                // TODO: these are supposed to be inside the above expression but zig fmt would delete them. https://github.com/ziglang/zig/issues/2658
                // Individual{ .species = Species.orc, .abs_position = makeCoord(3, 2) },
                // Individual{ .species = Species.orc, .abs_position = makeCoord(5, 2) },
                // Individual{ .species = Species.orc, .abs_position = makeCoord(12, 2) },
                // Individual{ .species = Species.orc, .abs_position = makeCoord(14, 2) },
                break :blk individuals;
            },
        };
    }
    pub fn deinit(self: *GameState) void {
        self.allocator.free(self.individuals);
    }

    fn applyStateChanges(self: *GameState, state_changes: void) !void {
        // TODO: this is just to have an inferred error
        self.allocator.free(try self.allocator.alloc(usize, 1));

        @panic("todo");
    }
    fn undoEvents(self: *GameState, state_changes: void) void {
        @panic("todo");
    }
};
