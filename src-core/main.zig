const std = @import("std");
const build_options = @import("build_options");
const displayMain = @import("../src-client/main.zig").displayMain;
const core = @import("./index.zig");
const debug = core.debug;
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const ServerChannel = core.protocol.ServerChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

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

    var position: Coord = makeCoord(0, 0);
    while (true) {
        switch (try channel.readAction()) {
            Action.Move => |direction| {
                const new_position = position.plus(direction);
                try channel.writeEvent(Event.{
                    .Moved = MovedEvent.{
                        .from = position,
                        .to = new_position,
                    },
                });
                position = new_position;
            },
            Action._Unused => unreachable,
        }
    }
}
