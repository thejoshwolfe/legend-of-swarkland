const std = @import("std");

pub const GameEngine = struct.{
    child_process: *std.os.ChildProcess,

    in_adapter: std.os.File.InStream,
    in_stream: *std.io.InStream(std.os.File.ReadError),
    out_adapter: std.os.File.OutStream,
    out_stream: *std.io.OutStream(std.os.File.WriteError),
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
        self.in_stream = &self.in_adapter.stream;
        self.out_adapter = self.child_process.stdin.?.outStream();
        self.out_stream = &self.out_adapter.stream;

        self.position = 0;
    }

    pub fn move(self: *GameEngine, direction: i8) !void {
        try self.out_stream.writeIntBe(i8, direction);
        self.position = try self.in_stream.readIntBe(i32);
    }
};
