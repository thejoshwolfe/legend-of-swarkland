const std = @import("std");
const debug = @import("./debug.zig");
const build_options = @import("build_options");
const displayMain = @import("../src-client/main.zig").displayMain;
const protocol = @import("./protocol.zig");
const ServerChannel = protocol.ServerChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = protocol.Action;
const Event = protocol.Event;

pub fn main() error!void {
    if (build_options.headless) {
        debug.prefix_name = "server";
        debug.warn("init\n");
        return headlessMain();
    } else {
        debug.prefix_name = "client";
        debug.warn("init\n");
        return displayMain();
    }
}

fn headlessMain() !void {
    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = ServerChannel.create(&in_adapter.stream, &out_adapter.stream);

    var position: i32 = 0;
    while (true) {
        switch (try channel.readAction()) {
            Action.Move => |direction| {
                position += direction;
                try channel.writeEvent(Event.{ .Moved = position });
            },
            Action._Unused => unreachable,
        }
    }
}
