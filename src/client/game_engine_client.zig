const std = @import("std");
const core = @import("../index.zig");
const game_server = @import("../server/game_server.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Socket = core.protocol.Socket;
const Request = core.protocol.Request;
const Action = core.protocol.Action;
const Response = core.protocol.Response;
const Event = core.protocol.Event;

const allocator = std.heap.c_allocator;

const QueueToFdAdapter = struct {
    socket: Socket,
    send_thread: *std.Thread,
    recv_thread: *std.Thread,
    queues: *SomeQueues,

    pub fn init(
        self: *QueueToFdAdapter,
        in_stream: std.fs.File.InStream,
        out_stream: std.fs.File.OutStream,
        queues: *SomeQueues,
    ) !void {
        self.socket = Socket.init(in_stream, out_stream);
        self.queues = queues;
        self.send_thread = try std.Thread.spawn(self, sendMain);
        self.recv_thread = try std.Thread.spawn(self, recvMain);
    }

    pub fn wait(self: *QueueToFdAdapter) void {
        self.send_thread.wait();
        self.recv_thread.wait();
    }

    fn sendMain(self: *QueueToFdAdapter) void {
        core.debug.nameThisThread("client send");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init");
        defer core.debug.thread_lifecycle.print("shutdown");

        while (true) {
            const request = self.queues.waitAndTakeRequest() orelse {
                core.debug.thread_lifecycle.print("clean shutdown");
                break;
            };
            self.socket.out().write(request) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *QueueToFdAdapter) void {
        core.debug.nameThisThread("client recv");
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init");
        defer core.debug.thread_lifecycle.print("shutdown");

        while (true) {
            const response = self.socket.in(allocator).read(Response) catch |err| {
                switch (err) {
                    error.EndOfStream => {
                        core.debug.thread_lifecycle.print("clean shutdown");
                        break;
                    },
                    else => @panic("TODO: proper error handling"),
                }
            };
            self.queues.enqueueResponse(response) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }
};

pub const SomeQueues = struct {
    requests_alive: std.atomic.Int(u8),
    requests: std.atomic.Queue(Request),
    responses_alive: std.atomic.Int(u8),
    responses: std.atomic.Queue(Response),

    pub fn init(self: *SomeQueues) void {
        self.requests_alive = std.atomic.Int(u8).init(1);
        self.requests = std.atomic.Queue(Request).init();
        self.responses_alive = std.atomic.Int(u8).init(1);
        self.responses = std.atomic.Queue(Response).init();
    }

    pub fn closeRequests(self: *SomeQueues) void {
        self.requests_alive.set(0);
    }
    pub fn enqueueRequest(self: *SomeQueues, request: Request) !void {
        try queuePut(Request, &self.requests, request);
    }

    /// null means requests have been closed
    pub fn waitAndTakeRequest(self: *SomeQueues) ?Request {
        while (self.requests_alive.get() != 0) {
            if (queueGet(Request, &self.requests)) |response| {
                return response;
            }
            // :ResidentSleeper:
            std.time.sleep(17 * std.time.millisecond);
        }
        return null;
    }

    pub fn closeResponses(self: *SomeQueues) void {
        self.responses_alive.set(0);
    }
    pub fn enqueueResponse(self: *SomeQueues, response: Response) !void {
        try queuePut(Response, &self.responses, response);
    }
    pub fn takeResponse(self: *SomeQueues) ?Response {
        return queueGet(Response, &self.responses);
    }

    /// null means responses have been closed
    pub fn waitAndTakeResponse(self: *SomeQueues) ?Response {
        while (self.responses_alive.get() != 0) {
            if (self.takeResponse()) |response| {
                return response;
            }
            // :ResidentSleeper:
            std.time.sleep(17 * std.time.millisecond);
        }
        return null;
    }
};
pub const ClientQueues = SomeQueues(Response, Request);
pub const ServerQueues = SomeQueues(Request, Response);

const Connection = union(enum) {
    child_process: ChildProcessData,
    const ChildProcessData = struct {
        child_process: *std.ChildProcess,
        adapter: *QueueToFdAdapter,
    };

    thread: ThreadData,
    const ThreadData = struct {
        core_thread: *std.Thread,
    };
};

pub const GameEngineClient = struct {
    // initialized in startAs*()
    connection: Connection,

    // initialized in init()
    queues: SomeQueues,

    fn init(self: *GameEngineClient) void {
        self.queues.init();
    }

    pub fn startAsChildProcess(self: *GameEngineClient) !void {
        self.init();

        const dir = try std.fs.selfExeDirPathAlloc(allocator);
        defer allocator.free(dir);
        var path = try std.fs.path.join(allocator, [_][]const u8{ dir, "legend-of-swarkland_headless" });
        defer allocator.free(path);

        self.connection = Connection{
            .child_process = blk: {
                const args = [_][]const u8{path};
                var child_process = try std.ChildProcess.init(args, allocator);
                child_process.stdout_behavior = std.ChildProcess.StdIo.Pipe;
                child_process.stdin_behavior = std.ChildProcess.StdIo.Pipe;
                try child_process.*.spawn();

                const adapter = try allocator.create(QueueToFdAdapter);
                try adapter.init(
                    child_process.*.stdout.?.inStream(),
                    child_process.*.stdin.?.outStream(),
                    &self.queues,
                );

                break :blk Connection.ChildProcessData{
                    .child_process = child_process,
                    .adapter = adapter,
                };
            },
        };
    }
    pub fn startAsThread(self: *GameEngineClient) !void {
        self.init();

        self.connection = Connection{
            .thread = blk: {
                const LambdaPlease = struct {
                    pub fn f(context: *SomeQueues) void {
                        core.debug.nameThisThread("server thread");
                        defer core.debug.unnameThisThread();
                        core.debug.thread_lifecycle.print("init");
                        defer core.debug.thread_lifecycle.print("shutdown");

                        game_server.server_main(context) catch |err| {
                            std.debug.warn("error: {}", @errorName(err));
                            if (@errorReturnTrace()) |trace| {
                                std.debug.dumpStackTrace(trace.*);
                            }
                            @panic("");
                        };
                    }
                };
                break :blk Connection.ThreadData{
                    .core_thread = try std.Thread.spawn(&self.queues, LambdaPlease.f),
                };
            },
        };
    }

    pub fn stopEngine(self: *GameEngineClient) void {
        core.debug.thread_lifecycle.print("close");
        self.queues.closeRequests();
        switch (self.connection) {
            .child_process => |*data| {
                data.child_process.stdin.?.close();
                // FIXME: workaround for wait() trying to close already closed fds
                data.child_process.stdin = null;

                core.debug.thread_lifecycle.print("join adapter threads");
                data.adapter.wait();

                core.debug.thread_lifecycle.print("join child process");
                _ = data.child_process.wait() catch undefined;
            },
            .thread => |*data| {
                data.core_thread.wait();
            },
        }
        core.debug.thread_lifecycle.print("all threads done");
    }

    pub fn act(self: *GameEngineClient, action: Action) !void {
        try self.queues.enqueueRequest(Request{ .act = action });
    }
    pub fn rewind(self: *GameEngineClient) !void {
        try self.queues.enqueueRequest(Request{ .rewind = {} });
    }

    pub fn move(self: *GameEngineClient, direction: Coord) !void {
        return self.act(Action{ .move = direction });
    }
    pub fn attack(self: *GameEngineClient, direction: Coord) !void {
        return self.act(Action{ .attack = direction });
    }
};

fn queuePut(comptime T: type, queue: *std.atomic.Queue(T), x: T) !void {
    const Node = std.atomic.Queue(T).Node;
    const node: *Node = try allocator.create(Node);
    node.data = x;
    queue.put(node);
}
fn queueGet(comptime T: type, queue: *std.atomic.Queue(T)) ?T {
    const Node = std.atomic.Queue(T).Node;
    const node: *Node = queue.get() orelse return null;
    defer allocator.destroy(node);
    const hack = node.data; // TODO: https://github.com/ziglang/zig/issues/961
    return hack;
}

test "basic interaction" {
    // init
    core.debug.init();
    core.debug.nameThisThread("test main");
    defer core.debug.unnameThisThread();
    core.debug.testing.print("start test");
    defer core.debug.testing.print("exit test");
    var _client: GameEngineClient = undefined;
    var client = &_client;
    try client.startAsThread();
    defer client.stopEngine();

    const startup_response = client.queues.waitAndTakeResponse().?;
    std.testing.expect(startup_response.load_state.self.?.rel_position.equals(makeCoord(0, 0)));
    core.debug.testing.print("startup done");

    // move
    try client.move(makeCoord(1, 0));
    {
        const response = client.queues.waitAndTakeResponse().?;
        const frames = response.stuff_happens.frames;
        std.testing.expect(frames[frames.len - 1].self.?.rel_position.equals(makeCoord(0, 0)));
    }
    core.debug.testing.print("move looks good");

    // rewind
    try client.rewind();
    {
        const response = client.queues.waitAndTakeResponse().?;
        const new_position = response.load_state.self.?.rel_position;

        std.testing.expect(response.load_state.self.?.rel_position.equals(makeCoord(0, 0)));
    }
    core.debug.testing.print("rewind looks good");
}
