const std = @import("std");
const core = @import("core");
const game_server = @import("../server/game_server.zig");
const Socket = core.protocol.Socket;

const allocator = std.heap.c_allocator;

pub fn main() anyerror!void {
    core.debug.init();

    var socket = Socket.init(
        (try std.io.getStdIn()).inStream(),
        (try std.io.getStdOut()).outStream(),
    );
    return game_server.server_main(&socket);
}
