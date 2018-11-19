const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    current_position: Coord,
    history: std.LinkedList(Event),

    const HistoryNode = std.LinkedList(Event).Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
            .current_position = makeCoord(0, 0),
            .history = std.LinkedList(Event).init(),
        };
    }

    pub fn takeAction(self: *GameEngine, action: Action) !Event {
        switch (action) {
            Action.Move => |direction| {
                const old_position = self.current_position;
                const new_position = self.current_position.plus(direction);
                self.current_position = new_position;
                return try self.recordEvent(Event{
                    .Moved = MovedEvent{
                        .from = old_position,
                        .to = new_position,
                    },
                });
            },
            Action._Unused => unreachable,
        }
    }

    fn recordEvent(self: *GameEngine, event: Event) !Event {
        const history_node: *HistoryNode = try self.allocator.createOne(HistoryNode);
        history_node.data = event;
        self.history.append(history_node);
        return event;
    }
};

pub const HistoryFrame = struct {
    event: core.protocol.Event,
};
