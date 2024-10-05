const std = @import("std");
const core = @import("core");
const game_server = @import("server");
const makeCoord = core.geometry.makeCoord;
const Socket = core.protocol.Socket;
const SomeQueues = core.protocol.SomeQueues;
const Request = core.protocol.Request;
const Response = core.protocol.Response;

const allocator = std.heap.c_allocator;

const QueueToFdAdapter = struct {
    socket: Socket,
    send_thread: std.Thread,
    recv_thread: std.Thread,
    queues: *SomeQueues,

    pub fn init(
        self: *QueueToFdAdapter,
        in_stream: std.fs.File.Reader,
        out_stream: std.fs.File.Writer,
        queues: *SomeQueues,
    ) !void {
        self.socket = Socket.init(in_stream, out_stream);
        self.queues = queues;
        self.send_thread = try std.Thread.spawn(.{}, sendMain, .{self});
        self.recv_thread = try std.Thread.spawn(.{}, recvMain, .{self});
    }

    pub fn wait(self: *QueueToFdAdapter) void {
        self.send_thread.join();
        self.recv_thread.join();
    }

    fn sendMain(self: *QueueToFdAdapter) void {
        core.debug.nameThisThread("client send");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init", .{});
        defer core.debug.thread_lifecycle.print("shutdown", .{});

        while (true) {
            const request = self.queues.waitAndTakeRequest() orelse {
                core.debug.thread_lifecycle.print("clean shutdown", .{});
                break;
            };
            self.socket.out().write(request) catch {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *QueueToFdAdapter) void {
        core.debug.nameThisThread("client recv");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init", .{});
        defer core.debug.thread_lifecycle.print("shutdown", .{});

        while (true) {
            const response = self.socket.in(allocator).read(Response) catch |err| {
                switch (err) {
                    error.EndOfStream => {
                        core.debug.thread_lifecycle.print("clean shutdown", .{});
                        break;
                    },
                    else => @panic("TODO: proper error handling"),
                }
            };
            self.queues.enqueueResponse(response) catch {
                @panic("TODO: proper error handling");
            };
        }
    }
};

const Connection = union(enum) {
    child_process: ChildProcessData,
    thread: ThreadData,

    const ChildProcessData = struct {
        child_process: *std.process.Child,
        adapter: *QueueToFdAdapter,
    };
    const ThreadData = struct {
        core_thread: std.Thread,
    };
};

pub const GameEngineClient = struct {
    // initialized in startAs*()
    connection: Connection,

    // initialized in init()
    queues: SomeQueues,

    pub fn init() @This() {
        return .{
            .connection = undefined,
            .queues = SomeQueues.init(allocator),
        };
    }

    pub const StartAsChildProcessOptions = struct {
        exe_path: []const u8 = "",
    };
    pub fn startAsChildProcess(self: *@This(), options: StartAsChildProcessOptions) !void {
        var path = options.exe_path;
        if (path.len == 0) {
            const dir = try std.fs.selfExeDirPathAlloc(allocator);
            defer allocator.free(dir);
            path = try std.fs.path.join(allocator, &[_][]const u8{ dir, "legend-of-swarkland_headless" });
        }
        defer if (options.exe_path.len == 0) allocator.free(path);

        self.connection = Connection{
            .child_process = blk: {
                const args = [_][]const u8{path};
                core.debug.thread_lifecycle.print("starting child process: {s}", .{path});
                var child_process = try std.process.Child.init(&args, allocator);
                child_process.stdout_behavior = std.process.Child.StdIo.Pipe;
                child_process.stdin_behavior = std.process.Child.StdIo.Pipe;
                try child_process.spawn();

                const adapter = try allocator.create(QueueToFdAdapter);
                try adapter.init(
                    child_process.stdout.?.reader(),
                    child_process.stdin.?.writer(),
                    &self.queues,
                );

                break :blk Connection.ChildProcessData{
                    .child_process = child_process,
                    .adapter = adapter,
                };
            },
        };
    }
    pub fn startAsThread(self: *@This()) !void {
        self.connection = Connection{
            .thread = blk: {
                const LambdaPlease = struct {
                    pub fn f(context: *SomeQueues) void {
                        core.debug.nameThisThread("server thread");
                        defer core.debug.unnameThisThread();
                        core.debug.thread_lifecycle.print("init", .{});
                        defer core.debug.thread_lifecycle.print("shutdown", .{});

                        game_server.server_main(context) catch |err| {
                            std.log.warn("error: {s}", .{@errorName(err)});
                            if (@errorReturnTrace()) |trace| {
                                std.debug.dumpStackTrace(trace.*);
                            }
                            @panic("");
                        };
                    }
                };
                break :blk Connection.ThreadData{
                    .core_thread = try std.Thread.spawn(.{}, LambdaPlease.f, .{&self.queues}),
                };
            },
        };
    }

    pub fn stopEngine(self: *@This()) void {
        core.debug.thread_lifecycle.print("close", .{});
        self.queues.closeRequests();
        switch (self.connection) {
            .child_process => |*data| {
                data.child_process.stdin.?.close();
                // FIXME: workaround for wait() trying to close already closed fds
                data.child_process.stdin = null;

                core.debug.thread_lifecycle.print("join adapter threads", .{});
                data.adapter.wait();

                core.debug.thread_lifecycle.print("join child process", .{});
                _ = data.child_process.wait() catch undefined;
            },
            .thread => |*data| {
                data.core_thread.join();
            },
        }
        core.debug.thread_lifecycle.print("all threads done", .{});
    }
};

test "startAsThread" {
    // init
    core.debug.init();
    core.debug.nameThisThread("test main");
    defer core.debug.unnameThisThread();
    var client = GameEngineClient.init();
    try client.startAsThread();
    defer client.stopEngine();
    try client.queues.enqueueRequest(Request{ .start_game = .puzzle_levels });

    const startup_response = client.queues.waitAndTakeResponse().?;
    try std.testing.expect(startup_response == .load_state);
}

test "server child process" {
    // init
    core.debug.init();
    core.debug.nameThisThread("test main");
    defer core.debug.unnameThisThread();
    var client = GameEngineClient.init();
    try client.startAsChildProcess(.{
        .exe_path = "zig-out/bin/legend-of-swarkland_headless",
    });
    defer client.stopEngine();
    try client.queues.enqueueRequest(Request{ .start_game = .puzzle_levels });

    const startup_response = client.queues.waitAndTakeResponse().?;
    try std.testing.expect(startup_response == .load_state);
}
