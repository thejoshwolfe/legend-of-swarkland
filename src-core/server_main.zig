const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const ServerChannel = core.protocol.ServerChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

pub fn main() anyerror!void {
    core.debug.prefix_name = "server";
    core.debug.warn("init\n");

    var in_adapter = (try std.io.getStdIn()).inStream();
    var out_adapter = (try std.io.getStdOut()).outStream();
    var channel = ServerChannel.create(&in_adapter.stream, &out_adapter.stream);

    var position: Coord = makeCoord(0, 0);
    while (true) {
        switch (try channel.readAction()) {
            Action.Move => |direction| {
                const new_position = position.plus(direction);
                try channel.writeEvent(Event{
                    .Moved = MovedEvent{
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
