const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const GameEngine = core.game_engine.GameEngine;
const GameState = core.game_state.GameState;
const Channel = core.protocol.Channel;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;

const allocator = std.heap.c_allocator;

pub fn server_main(channel: *Channel) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init\n");
    defer core.debug.warn("shutdown\n");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    {
        const events = try game_engine.getStartGameEvents();
        try game_engine.applyEvents(events);
        try channel.writeResponse(Response{ .events = events });
    }

    core.debug.warn("start main loop\n");
    while (true) {
        switch ((try channel.readRequest()) orelse {
            core.debug.warn("clean shutdown. close\n");
            channel.close();
            break;
        }) {
            Request.act => |action| {
                if (!game_engine.validateAction(action)) continue;
                var actions = []Action{
                    action,
                    getAiAction(game_engine.game_state),
                };
                const events = try game_engine.actionsToEvents(actions[0..]);

                // TODO: make a copy of the events here to take ownership, then deinit the response.
                try game_engine.applyEvents(events);
                try channel.writeResponse(Response{ .events = events });
            },
            Request.rewind => {
                if (game_engine.rewind()) |events| {
                    try channel.writeResponse(Response{ .undo = events });
                    core.protocol.deinitEvents(allocator, events);
                }
            },
        }
    }
}

fn getAiAction(game_state: GameState) Action {
    // TODO: move this to another thread/process and communicate using a channel.
    const me = game_state.player_positions[1];
    if (game_state.player_is_alive[0]) {
        const you = game_state.player_positions[0];
        var delta = you.minus(me);
        if (delta.x * delta.y == 0 and (delta.x + delta.y) * (delta.x + delta.y) == 1) {
            return Action{ .attack = delta };
        }
    }
    return Action{ .move = Coord{ .x = -1, .y = 0 } };
}
