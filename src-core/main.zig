const std = @import("std");
const build_options = @import("build_options");
const displayMain = @import("../src-client/main.zig").displayMain;

pub fn main() error!void {
    if (build_options.headless) {
        return headlessMain();
    } else {
        return displayMain();
    }
}

fn headlessMain() !void {
    var stdout_file = try std.io.getStdOut();
    var stdout_file_out_stream = std.io.FileOutStream.init(stdout_file);
    var stdout_stream = &stdout_file_out_stream.stream;

    var addr = std.net.Address.initIp4(std.net.parseIp4("127.0.0.1") catch unreachable, 0);
    var loop: std.event.Loop = undefined;
    try loop.initSingleThreaded(std.heap.c_allocator);

    var server = @"💩Server"{ .tcp_server = std.event.tcp.Server.init(&loop) };
    defer server.tcp_server.deinit();
    try server.tcp_server.listen(addr, @"💩Server".handler);

    try stdout_stream.print("{x4}\n", u16(server.tcp_server.listen_address.port()));

    loop.run();
}

const @"💩Server" = struct {
    tcp_server: std.event.tcp.Server,

    const Self = @This();
    async<*std.mem.Allocator> fn handler(tcp_server: *std.event.tcp.Server, _addr: *const std.net.Address, _socket: *const std.os.File) void {
        const self = @fieldParentPtr(Self, "tcp_server", tcp_server);
        var socket = _socket.*;
        defer socket.close();
        // TODO guarantee elision of this allocation
        const next_handler = async mainLoop(tcp_server.loop, socket) catch unreachable;
        (await next_handler) catch |err| {
            std.debug.panic("unable to handle connection: {}\n", err);
        };
        suspend {
            cancel @handle();
        }
    }
};
async fn mainLoop(loop: *std.event.Loop, socket: std.os.File) !void {
    var out_adapter = std.io.FileOutStream.init(socket);
    var out_stream = &out_adapter.stream;

    var in_adapter = std.io.FileInStream.init(socket);
    var in_stream = &in_adapter.stream;

    var position: i32 = 0;
    while (true) {
        {
            var buffer: [1]u8 = undefined;
            try await try async socketRead(loop, socket.handle, buffer[0..]);
            const direction = @bitCast(i8, buffer[0]);
            position += direction;
        }
        {
            var buffer: [4]u8 = undefined;
            std.mem.writeIntBE(i32, &buffer, position);
            try await try async socketWrite(loop, socket.handle, buffer[0..]);
        }
    }
}

async fn socketRead(loop: *std.event.Loop, fd: std.os.FileHandle, buffer: []u8) !void {
    while (true) {
        return std.os.posixRead(fd, buffer) catch |err| switch (err) {
            error.WouldBlock => {
                try await try async loop.linuxWaitFd(fd, std.os.posix.EPOLLET | std.os.posix.EPOLLIN);
                continue;
            },
            else => return err,
        };
    }
}
async fn socketWrite(loop: *std.event.Loop, fd: std.os.FileHandle, buffer: []const u8) !void {
    while (true) {
        return std.os.posixWrite(fd, buffer) catch |err| switch (err) {
            error.WouldBlock => {
                try await try async loop.linuxWaitFd(fd, std.os.posix.EPOLLET | std.os.posix.EPOLLOUT);
                continue;
            },
            else => return err,
        };
    }
}
