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

pub fn server_main(player_channel: *Channel) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init\n");
    defer core.debug.warn("shutdown\n");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    {
        const events = try game_engine.getStartGameEvents();
        try game_engine.applyEvents(events);
        try player_channel.writeResponse(Response{ .events = events });
    }

    core.debug.warn("start main loop\n");
    while (true) {
        switch ((try player_channel.readRequest()) orelse {
            core.debug.warn("clean shutdown. close\n");
            player_channel.close();
            break;
        }) {
            Request.act => |player_action| {
                if (!game_engine.validateAction(player_action)) continue;
                var actions: [5]Action = undefined;
                for (actions) |*action, i| {
                    if (i == 0) {
                        action.* = player_action;
                    } else {
                        action.* = getAiAction(game_engine.game_state, i);
                    }
                }
                const events = try game_engine.actionsToEvents(actions[0..]);

                // TODO: make a copy of the events here to take ownership, then deinit the response.
                try game_engine.applyEvents(events);
                try player_channel.writeResponse(Response{ .events = events });
            },
            Request.rewind => {
                if (game_engine.rewind()) |events| {
                    try player_channel.writeResponse(Response{ .undo = events });
                    core.protocol.deinitEvents(allocator, events);
                }
            },
        }
    }
}

fn getAiAction(game_state: GameState, i: usize) Action {
    // TODO: move this to another thread/process and communicate using a channel.
    const me = game_state.player_positions[i];
    if (game_state.player_is_alive[0]) {
        // chase the opponent
        const you = game_state.player_positions[0];
        var delta = you.minus(me);
        std.debug.assert(!(delta.x == 0 and delta.y == 0));
        if (delta.x * delta.y == 0 and (delta.x + delta.y) * (delta.x + delta.y) == 1) {
            // orthogonally adjacent.
            return Action{ .attack = delta };
        }
        // move toward the target
        if (delta.x != 0) {
            // prioritize sideways movement
            return Action{ .move = Coord{ .x = core.geometry.sign(delta.x), .y = 0 } };
        } else {
            return Action{ .move = Coord{ .x = 0, .y = core.geometry.sign(delta.y) } };
        }
    }
    // everyone is dead
    return Action{ .move = Coord{ .x = 0, .y = -1 } };
}
