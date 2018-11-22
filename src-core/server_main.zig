const std = @import("std");
const core = @import("./index.zig");
const BaseChannel = core.protocol.BaseChannel(std.os.File.ReadError, std.os.File.WriteError);

pub fn main() anyerror!void {
    // communicate with stdio
    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = BaseChannel.create(&in_adapter.stream, &out_adapter.stream);
    return core.game_server.server_main(&channel);
}
