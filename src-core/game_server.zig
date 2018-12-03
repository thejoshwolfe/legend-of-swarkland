const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const GameEngine = core.game_engine.GameEngine;
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
        switch (channel.readRequest() catch |err| switch (err) {
            error.CleanShutdown => {
                core.debug.warn("clean shutdown. close\n");
                channel.close();
                break;
            },
            else => return err,
        }) {
            Request.act => |action| {
                if (!game_engine.validateAction(action)) continue;
                var actions = []Action{
                    action,
                    Action{ .move = Coord{ .x = -1, .y = 0 } },
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
