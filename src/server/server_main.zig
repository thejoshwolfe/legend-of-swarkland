const std = @import("std");
const core = @import("core");
const game_server = @import("../server/game_server.zig");
const SomeQueues = @import("../client/game_engine_client.zig").SomeQueues;
const Socket = core.protocol.Socket;
const Request = core.protocol.Request;
const Response = core.protocol.Response;

const allocator = std.heap.c_allocator;

const FdToQueueAdapter = struct {
    socket: Socket,
    send_thread: *std.Thread,
    recv_thread: *std.Thread,
    queues: *SomeQueues,

    pub fn init(
        self: *FdToQueueAdapter,
        in_stream: std.fs.File.InStream,
        out_stream: std.fs.File.OutStream,
        queues: *SomeQueues,
    ) !void {
        self.socket = Socket.init(in_stream, out_stream);
        self.queues = queues;
        self.send_thread = try std.Thread.spawn(self, sendMain);
        self.recv_thread = try std.Thread.spawn(self, recvMain);
    }

    pub fn wait(self: *FdToQueueAdapter) void {
        self.send_thread.wait();
        self.recv_thread.wait();
    }

    fn sendMain(self: *FdToQueueAdapter) void {
        core.debug.nameThisThread("server send");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init", .{});
        defer core.debug.thread_lifecycle.print("shutdown", .{});

        while (true) {
            const msg = self.queues.waitAndTakeResponse() orelse {
                core.debug.thread_lifecycle.print("clean shutdown", .{});
                break;
            };
            self.socket.out().write(msg) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *FdToQueueAdapter) void {
        core.debug.nameThisThread("server recv");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init", .{});
        defer core.debug.thread_lifecycle.print("shutdown", .{});

        while (true) {
            const msg = self.socket.in(allocator).read(Request) catch |err| {
                switch (err) {
                    error.EndOfStream => {
                        core.debug.thread_lifecycle.print("clean shutdown", .{});
                        self.queues.closeRequests();
                        break;
                    },
                    else => @panic("TODO: proper error handling"),
                }
            };
            self.queues.enqueueRequest(msg) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }
};

pub fn main() anyerror!void {
    core.debug.init();

    core.debug.nameThisThread("server process");
    defer core.debug.unnameThisThread();
    core.debug.thread_lifecycle.print("init", .{});
    defer core.debug.thread_lifecycle.print("shutdown", .{});

    var queues: SomeQueues = undefined;
    queues.init();

    var adapter: FdToQueueAdapter = undefined;
    try adapter.init(
        std.io.getStdIn().inStream(),
        std.io.getStdOut().outStream(),
        &queues,
    );
    defer {
        core.debug.thread_lifecycle.print("join adapter threads", .{});
        adapter.wait();
    }

    return game_server.server_main(&queues);
}
