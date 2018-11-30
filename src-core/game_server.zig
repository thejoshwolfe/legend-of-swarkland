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

pub fn server_main(channel: *Channel) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init\n");
    defer core.debug.warn("shutdown\n");

    var game_engine: GameEngine = undefined;
    game_engine.init(std.heap.c_allocator);
    {
        const event = game_engine.startGame();
        try channel.writeResponse(Response{ .event = event });
    }

    core.debug.warn("start main loop\n");
    var events = ArrayList(Event).init(std.heap.c_allocator);
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
                try game_engine.actionsToEvents(actions[0..], &events);

                try game_engine.applyEvents(events.toSliceConst());
                for (events.toSliceConst()) |event| {
                    try channel.writeResponse(Response{ .event = event });
                }
                events.shrink(0);
            },
            Request.rewind => {
                if (game_engine.rewind()) |event| {
                    try channel.writeResponse(Response{ .undo = event });
                }
            },
        }
    }
}
