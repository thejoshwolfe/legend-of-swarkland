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

const Connection = union(enum) {
    child_process: *std.ChildProcess,

    thread: ThreadData,
    const ThreadData = struct {
        core_thread: *std.Thread,
        send_pipe: [2]std.fs.File,
        recv_pipe: [2]std.fs.File,
        server_socket: Socket,
    };

    attach: AttachData,
    const AttachData = struct {
        send_pipe: [2]std.fs.File,
        recv_pipe: [2]std.fs.File,
        server_socket: Socket,
    };
};

pub const GameEngineClient = struct {
    // initialized in startAs*()
    connection: Connection,
    socket: Socket,
    // initialized in startThreads()
    send_thread: *std.Thread,
    recv_thread: *std.Thread,

    // initialized in init()
    request_outbox: std.atomic.Queue(Request),
    response_inbox: std.atomic.Queue(Response),
    stay_alive: std.atomic.Int(u8),
    debug_client_id: u32,

    fn init(self: *GameEngineClient) void {
        self.request_outbox = std.atomic.Queue(Request).init();
        self.response_inbox = std.atomic.Queue(Response).init();
        self.stay_alive = std.atomic.Int(u8).init(1);
        self.debug_client_id = 0;
    }
    fn startThreads(self: *GameEngineClient) !void {
        // start threads last
        self.send_thread = try std.Thread.spawn(self, sendMain);
        self.recv_thread = try std.Thread.spawn(self, recvMain);
    }

    /// returns the socket for the server to use to communicate with this client.
    pub fn startAsAi(self: *GameEngineClient, debug_client_id: u32) !*Socket {
        self.init();
        self.debug_client_id = debug_client_id;

        var result: *Socket = undefined;

        self.connection = Connection{ .attach = undefined };
        var data = &self.connection.attach;
        data.send_pipe = try makePipe();
        data.recv_pipe = try makePipe();
        self.socket = Socket.init(data.recv_pipe[0].inStream(), data.send_pipe[1].outStream());
        data.server_socket = Socket.init(data.send_pipe[0].inStream(), data.recv_pipe[1].outStream());
        result = &data.server_socket;

        try self.startThreads();

        return result;
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
                self.socket = Socket.init(child_process.*.stdout.?.inStream(), child_process.*.stdin.?.outStream());
                // FIXME: workaround for wait() trying to close stdin when it's already closed.
                child_process.stdin = null;
                break :blk child_process;
            },
        };

        try self.startThreads();
    }
    pub fn startAsThread(self: *GameEngineClient) !void {
        self.init();

        self.connection = Connection{ .thread = undefined };
        const data = &self.connection.thread;
        data.send_pipe = try makePipe();
        data.recv_pipe = try makePipe();
        self.socket = Socket.init(data.recv_pipe[0].inStream(), data.send_pipe[1].outStream());
        data.server_socket = Socket.init(data.send_pipe[0].inStream(), data.recv_pipe[1].outStream());
        const LambdaPlease = struct {
            pub fn f(context: *Socket) void {
                game_server.server_main(context) catch |err| {
                    std.debug.warn("error: {}", @errorName(err));
                    if (@errorReturnTrace()) |trace| {
                        std.debug.dumpStackTrace(trace.*);
                    }
                    @panic("");
                };
            }
        };
        data.core_thread = try std.Thread.spawn(&data.server_socket, LambdaPlease.f);

        try self.startThreads();
    }

    pub fn stopEngine(self: *GameEngineClient) void {
        core.debug.thread_lifecycle.print("close");
        self.socket.close(Request.quit);
        switch (self.connection) {
            .child_process => |child_process| {
                _ = child_process.wait() catch undefined;
            },
            .thread => |*data| {
                data.core_thread.wait();
            },
            .attach => @panic("call stopAi()"),
        }
        self.stay_alive.set(0);
        self.send_thread.wait();
        self.recv_thread.wait();
        core.debug.thread_lifecycle.print("all threads done");
    }
    pub fn stopAi(self: *GameEngineClient) void {
        core.debug.thread_lifecycle.print("stop ai");
        self.stay_alive.set(0);
        // The server tells us to shut down
        self.connection.attach.server_socket.close(Response.game_over);
        self.send_thread.wait();
        self.recv_thread.wait();
        core.debug.thread_lifecycle.print("ai threads done");
    }

    fn isAlive(self: *GameEngineClient) bool {
        return self.stay_alive.get() != 0;
    }

    pub fn pollResponse(self: *GameEngineClient) ?Response {
        return queueGet(Response, &self.response_inbox) orelse return null;
    }

    fn sendMain(self: *GameEngineClient) void {
        core.debug.nameThisThreadWithClientId("client send", self.debug_client_id);
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init");
        defer core.debug.thread_lifecycle.print("shutdown");
        while (self.isAlive()) {
            const request = queueGet(Request, &self.request_outbox) orelse {
                // :ResidentSleeper:
                std.time.sleep(17 * std.time.millisecond);
                continue;
            };
            self.socket.out().write(request) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *GameEngineClient) void {
        core.debug.nameThisThreadWithClientId("client recv", self.debug_client_id);
        defer core.debug.unnameThisThread();
        core.debug.thread_lifecycle.print("init");
        defer core.debug.thread_lifecycle.print("shutdown");
        while (self.isAlive()) {
            const response = self.socket.in(allocator).read(Response) catch {
                @panic("TODO: proper error handling");
            };
            switch (response) {
                .game_over => {
                    core.debug.thread_lifecycle.print("clean shutdown");
                    return;
                },
                else => {
                    queuePut(Response, &self.response_inbox, response) catch |err| {
                        @panic("TODO: proper error handling");
                    };
                },
            }
        }
    }

    pub fn act(self: *GameEngineClient, action: Action) !void {
        try queuePut(Request, &self.request_outbox, Request{ .act = action });
    }
    pub fn rewind(self: *GameEngineClient) !void {
        try queuePut(Request, &self.request_outbox, Request{ .rewind = {} });
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
fn makePipe() ![2]std.fs.File {
    var fds: [2]i32 = try std.os.pipe();
    return [_]std.fs.File{
        std.fs.File.openHandle(fds[0]),
        std.fs.File.openHandle(fds[1]),
    };
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

    const startup_response = pollSync(client);
    const starting_position = startup_response.load_state.self.?.abs_position;
    core.debug.testing.print("startup done");

    // move
    try client.move(makeCoord(1, 0));
    {
        const response = pollSync(client);
        const new_position = blk: {
            for (response.stuff_happens.frames[1].movements) |x| {
                if (x.species == .human) break :blk x.abs_position;
            }
            unreachable;
        };

        std.testing.expect(new_position.minus(starting_position).equals(makeCoord(1, 0)));
    }
    core.debug.testing.print("move looks good");

    // rewind
    try client.rewind();
    {
        const response = pollSync(client);
        const new_position = response.load_state.self.?.abs_position;

        std.testing.expect(new_position.equals(starting_position));
    }
    core.debug.testing.print("rewind looks good");
}

fn pollSync(client: *GameEngineClient) Response {
    while (true) {
        if (client.pollResponse()) |response| {
            return response;
        }
        std.time.sleep(17 * std.time.millisecond);
    }
}
