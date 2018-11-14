const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const ClientChannel = core.protocol.ClientChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

pub const GameEngine = struct {
    child_process: *std.os.ChildProcess,

    in_adapter: std.os.File.InStream,
    out_adapter: std.os.File.OutStream,
    channel: ClientChannel,
    position: Coord,

    pub fn startEngine(self: *GameEngine) !void {
        var dir = try std.os.selfExeDirPathAlloc(std.heap.c_allocator);
        defer std.heap.c_allocator.free(dir);
        var path = try std.os.path.join(std.heap.c_allocator, dir, "legend-of-swarkland_headless");
        defer std.heap.c_allocator.free(path);

        const args = []const []const u8{path};
        self.child_process = try std.os.ChildProcess.init(args, std.heap.c_allocator);
        self.child_process.stdout_behavior = std.os.ChildProcess.StdIo.Pipe;
        self.child_process.stdin_behavior = std.os.ChildProcess.StdIo.Pipe;
        try self.child_process.spawn();
        self.in_adapter = self.child_process.stdout.?.inStream();
        self.out_adapter = self.child_process.stdin.?.outStream();
        self.channel = ClientChannel.create(&self.in_adapter.stream, &self.out_adapter.stream);

        self.position = makeCoord(0, 0);
    }

    pub fn move(self: *GameEngine, direction: Coord) !void {
        try self.channel.writeAction(Action{ .Move = direction });

        switch (try self.channel.readEvent()) {
            Event.Moved => |event| {
                self.position = event.to;
            },
            Event._Unused => unreachable,
        }
    }
};
