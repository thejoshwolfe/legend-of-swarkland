const std = @import("std");
const core = @import("core");
const game_server = @import("../server/game_server.zig");
const Channel = core.protocol.Channel;

const allocator = std.heap.c_allocator;

pub fn main() anyerror!void {
    core.debug.init();
    // communicate with stdio
    var channel: Channel = undefined;
    channel.init(allocator, try std.io.getStdIn(), try std.io.getStdOut());
    return game_server.server_main(&channel);
}
