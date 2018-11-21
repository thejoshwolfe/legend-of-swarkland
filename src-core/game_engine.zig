const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    game_state: GameState,
    history: std.LinkedList(Event),

    const HistoryNode = std.LinkedList(Event).Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = GameState.init(),
            .history = std.LinkedList(Event).init(),
        };
    }

    pub fn takeAction(self: *GameEngine, action: Action) !?Event {
        const event = self.validateAction(action) orelse return null;
        try self.recordEvent(event);
        self.game_state.applyEvent(event);
        return event;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) ?Event {
        switch (action) {
            Action.Move => |direction| {
                const old_position = self.game_state.position;
                const new_position = old_position.plus(direction);
                return Event{
                    .Moved = MovedEvent{
                        .from = old_position,
                        .to = new_position,
                    },
                };
            },
        }
    }

    pub fn rewind(self: *GameEngine) ?Event {
        const node = self.history.pop() orelse return null;
        const event = node.data;
        self.allocator.destroy(node);
        self.game_state.undoEvent(event);
        return event;
    }

    fn recordEvent(self: *GameEngine, event: Event) !void {
        const history_node: *HistoryNode = try self.allocator.createOne(HistoryNode);
        history_node.data = event;
        self.history.append(history_node);
    }
};

pub const HistoryFrame = struct {
    event: core.protocol.Event,
};

pub const GameState = struct {
    position: Coord,
    pub fn init() GameState {
        return GameState{ .position = makeCoord(0, 0) };
    }

    fn applyEvent(self: *GameState, event: Event) void {
        switch (event) {
            Event.Moved => |e| {
                self.position = e.to;
            },
        }
    }
    fn undoEvent(self: *GameState, event: Event) void {
        switch (event) {
            Event.Moved => |e| {
                self.position = e.from;
            },
        }
    }
};
