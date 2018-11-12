const std = @import("std");
const debug = @import("./debug.zig");
const protocol = @import("./protocol.zig");
const ClientChannel = protocol.ClientChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = protocol.Action;
const Event = protocol.Event;

pub const GameEngine = struct.{
    child_process: *std.os.ChildProcess,

    in_adapter: std.os.File.InStream,
    out_adapter: std.os.File.OutStream,
    channel: ClientChannel,
    position: i32,

    pub fn startEngine(self: *GameEngine) !void {
        var dir = try std.os.selfExeDirPathAlloc(std.heap.c_allocator);
        defer std.heap.c_allocator.free(dir);
        var path = try std.os.path.join(std.heap.c_allocator, dir, "legend-of-swarkland_headless");
        defer std.heap.c_allocator.free(path);

        const args = []const []const u8.{path};
        self.child_process = try std.os.ChildProcess.init(args, std.heap.c_allocator);
        self.child_process.stdout_behavior = std.os.ChildProcess.StdIo.Pipe;
        self.child_process.stdin_behavior = std.os.ChildProcess.StdIo.Pipe;
        try self.child_process.spawn();
        self.in_adapter = self.child_process.stdout.?.inStream();
        self.out_adapter = self.child_process.stdin.?.outStream();
        self.channel = ClientChannel.create(&self.in_adapter.stream, &self.out_adapter.stream);

        self.position = 0;
    }

    pub fn move(self: *GameEngine, direction: i8) !void {
        try self.channel.writeAction(Action.{ .Move = direction });
        self.position = switch (try self.channel.readEvent()) {
            Event.Moved => |position| position,
            Event._Unused => unreachable,
        };
    }
};
