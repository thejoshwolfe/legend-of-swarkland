const std = @import("std");
const core = @import("./index.zig");
const Channel = core.protocol.Channel;

pub fn main() anyerror!void {
    core.debug.init();
    // communicate with stdio
    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = Channel.create(&in_adapter.stream, &out_adapter.stream);
    return core.game_server.server_main(&channel);
}
