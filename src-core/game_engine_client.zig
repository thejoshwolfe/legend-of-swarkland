const std = @import("std");

pub const GameEngine = struct {
    child_process: *std.os.ChildProcess,

    socket_file: std.os.File,
    in_adapter: std.io.FileInStream,
    in_stream: *std.io.InStream(std.os.File.ReadError),
    out_adapter: std.io.FileOutStream,
    out_stream: *std.io.OutStream(std.os.File.WriteError),
    position: i32,

    pub fn startEngine(self: *GameEngine) !void {
        var dir = try std.os.selfExeDirPathAlloc(std.heap.c_allocator);
        defer std.heap.c_allocator.free(dir);
        var path = try std.os.path.join(std.heap.c_allocator, dir, "legend-of-swarkland_headless");
        defer std.heap.c_allocator.free(path);

        const args = []const []const u8{path};
        self.child_process = try std.os.ChildProcess.init(args, std.heap.c_allocator);
        self.child_process.stdout_behavior = std.os.ChildProcess.StdIo.Pipe;
        try self.child_process.spawn();
        var stdout = self.child_process.stdout.?;
        var port_buffer = []u8{0} ** 5;
        const bytes_read = try stdout.read(port_buffer[0..]);
        std.debug.assert(bytes_read == 5);
        std.debug.assert(port_buffer[4] == '\n');
        const port_number = try std.fmt.parseInt(u16, port_buffer[0..4], 16);

        std.debug.warn("connecting to port {}...", port_number);
        var socket_fd = try std.os.posixSocket(std.os.posix.AF_INET, std.os.posix.SOCK_STREAM | std.os.posix.SOCK_CLOEXEC, std.os.posix.PROTO_tcp);
        var addr = std.net.Address.initIp4(std.net.parseIp4("127.0.0.1") catch unreachable, port_number);
        try std.os.posixConnect(socket_fd, addr.os_addr);
        std.debug.warn("done\n");

        self.socket_file = std.os.File.openHandle(socket_fd);
        self.in_adapter = std.io.FileInStream.init(self.socket_file);
        self.in_stream = &self.in_adapter.stream;
        self.out_adapter = std.io.FileOutStream.init(self.socket_file);
        self.out_stream = &self.out_adapter.stream;
        self.position = 0;
    }

    pub fn move(self: *GameEngine, direction: i8) !void {
        try self.out_stream.writeIntBe(i8, direction);
        self.position = try self.in_stream.readIntBe(i32);
    }
};
