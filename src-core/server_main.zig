const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const GameEngine = core.game_engine.GameEngine;
const BaseChannel = core.protocol.BaseChannel(std.os.File.ReadError, std.os.File.WriteError);
const Request = core.protocol.Request;
const Response = core.protocol.Response;

pub fn main() anyerror!void {
    core.debug.prefix_name = "server";
    core.debug.warn("init\n");

    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = BaseChannel.create(&in_adapter.stream, &out_adapter.stream);

    var game_engine: GameEngine = undefined;
    game_engine.init(std.heap.c_allocator);

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
