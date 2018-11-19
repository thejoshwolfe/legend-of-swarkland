const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const GameEngine = core.game_engine.GameEngine;
const ServerChannel = core.protocol.ServerChannel(std.os.File.ReadError, std.os.File.WriteError);

pub fn main() anyerror!void {
    core.debug.prefix_name = "server";
    core.debug.warn("init\n");

    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = ServerChannel.create(&in_adapter.stream, &out_adapter.stream);

    var game_engine: GameEngine = undefined;
    game_engine.init(std.heap.c_allocator);

    while (true) {
        const action = try channel.readAction();
        const event = try game_engine.takeAction(action);
        try channel.writeEvent(event);
    }
}
