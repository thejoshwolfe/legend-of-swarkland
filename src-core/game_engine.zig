const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const GameState = core.game_state.GameState;
const Terrain = core.game_state.Terrain;
const Floor = core.game_state.Floor;
const Wall = core.game_state.Wall;

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    game_state: GameState,
    history: HistoryList,

    const HistoryList = std.LinkedList([]Event);
    const HistoryNode = HistoryList.Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = GameState.init(),
            .history = HistoryList.init(),
        };
    }

    pub fn getStartGameEvents(self: *const GameEngine) ![]Event {
        var events = try self.allocator.alloc(Event, 1);
        events[0] = Event{
            .init_state = Event.InitState{
                .terrain = undefined,
                .player_positions = []Coord{ makeCoord(3, 3), makeCoord(9, 5) },
            },
        };

        var terrain: *Terrain = &events[0].init_state.terrain;
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

        return events;
    }

    pub fn actionsToEvents(self: *const GameEngine, actions: []const Action) ![]Event {
        var events = ArrayList(Event).init(self.allocator);
        for (actions) |action, i| {
            switch (action) {
                Action.move => |direction| {
                    const old_position = self.game_state.player_positions[i];
                    const new_position = old_position.plus(direction);
                    if (self.isOpenSpace(new_position)) {
                        try events.append(Event{
                            .moved = Event.Moved{
                                .player_index = i,
                                .from = old_position,
                                .to = new_position,
                            },
                        });
                    }
                },
            }
        }
        return events.toOwnedSlice();
    }

    pub fn applyEvents(self: *GameEngine, events: []Event) !void {
        try self.recordEvents(events);
        self.game_state.applyEvents(events);
    }

    pub fn isOpenSpace(self: *const GameEngine, coord: Coord) bool {
        if (coord.x < 0 or coord.y < 0) return false;
        if (coord.x >= 16 or coord.y >= 16) return false;
        return self.game_state.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)] == Wall.air;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            Action.move => |direction| {
                if (direction.x * direction.y != 0) return false;
                if ((direction.x + direction.y) * (direction.x + direction.y) != 1) return false;
                return true;
            },
        }
    }

    pub fn rewind(self: *GameEngine) ?[]Event {
        if (self.history.len <= 1) {
            // that's enough pal.
            return null;
        }
        const node = self.history.pop() orelse return null;
        const events = node.data;
        self.allocator.destroy(node);
        self.game_state.undoEvents(events);
        return events;
    }

    fn recordEvents(self: *GameEngine, events: []Event) !void {
        const history_node: *HistoryNode = try self.allocator.createOne(HistoryNode);
        history_node.data = events;
        self.history.append(history_node);
    }
};

pub const HistoryFrame = struct {
    event: []Event,
};
