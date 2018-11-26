const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const GameEngine = core.game_engine.GameEngine;
const Channel = core.protocol.Channel;
const Request = core.protocol.Request;
const Response = core.protocol.Response;

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
    while (true) {
        switch (try channel.readRequest()) {
            Request.act => |action| {
                if (try game_engine.takeAction(action)) |event| {
                    try channel.writeResponse(Response{ .event = event });
                }
            },
            Request.rewind => {
                if (game_engine.rewind()) |event| {
                    try channel.writeResponse(Response{ .undo = event });
                }
            },
        }
    }
}
