const std = @import("std");
const core = @import("./index.zig");
const Channel = core.protocol.Channel;

pub fn main() anyerror!void {
    core.debug.init();
    // communicate with stdio
    var channel: Channel = undefined;
    channel.init(try std.io.getStdIn(), try std.io.getStdOut());
    return core.game_server.server_main(&channel);
}
