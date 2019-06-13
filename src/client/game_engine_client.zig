const std = @import("std");
const core = @import("../index.zig");
const game_server = @import("../server/game_server.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Channel = core.protocol.Channel;
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
        server_channel: Channel,
    };
};

pub const GameEngineClient = struct {
    // initialized in startAs*()
    connection: Connection,
    channel: Channel,
    // initialized in startThreads()
    send_thread: *std.Thread,
    recv_thread: *std.Thread,

    // initialized in init()
    request_outbox: std.atomic.Queue(Request),
    response_inbox: std.atomic.Queue(Response),
    stay_alive: std.atomic.Int(u8),

    fn init(self: *GameEngineClient) void {
        self.request_outbox = std.atomic.Queue(Request).init();
        self.response_inbox = std.atomic.Queue(Response).init();
        self.stay_alive = std.atomic.Int(u8).init(1);
    }
    fn startThreads(self: *GameEngineClient) !void {
        // start threads last
        self.send_thread = try std.Thread.spawn(self, sendMain);
        self.recv_thread = try std.Thread.spawn(self, recvMain);
    }

    pub fn startAsChildProcess(self: *GameEngineClient) !void {
        self.init();

        const dir = try std.fs.selfExeDirPathAlloc(allocator);
        defer allocator.free(dir);
        var path = try std.fs.path.join(allocator, [_][]const u8{ dir, "legend-of-swarkland_headless" });
        defer allocator.free(path);

        self.connection = Connection{ .child_process = undefined };
        switch (self.connection) {
            // TODO: This switch is a workaround for lack of guaranteed copy elision.
            Connection.child_process => |*child_process| {
                const args = [_][]const u8{path};
                child_process.* = try std.ChildProcess.init(args, allocator);
                child_process.*.stdout_behavior = std.ChildProcess.StdIo.Pipe;
                child_process.*.stdin_behavior = std.ChildProcess.StdIo.Pipe;
                try child_process.*.spawn();
                self.channel.init(allocator, child_process.*.stdout.?, child_process.*.stdin.?);
                // FIXME: workaround for wait() trying to close stdin when it's already closed.
                child_process.*.stdin = null;
            },
            else => unreachable,
        }

        try self.startThreads();
    }
    pub fn startAsThread(self: *GameEngineClient) !void {
        self.init();

        self.connection = Connection{ .thread = undefined };
        switch (self.connection) {
            // TODO: This switch is a workaround for lack of guaranteed copy elision.
            Connection.thread => |*data| {
                data.send_pipe = try makePipe();
                data.recv_pipe = try makePipe();
                self.channel.init(allocator, data.recv_pipe[0], data.send_pipe[1]);
                data.server_channel.init(allocator, data.send_pipe[0], data.recv_pipe[1]);
                const LambdaPlease = struct {
                    pub fn f(context: *Channel) void {
                        game_server.server_main(context) catch |err| {
                            std.debug.warn("error: {}", @errorName(err));
                            if (@errorReturnTrace()) |trace| {
                                std.debug.dumpStackTrace(trace.*);
                            }
                            @panic("");
                        };
                    }
                };
                data.core_thread = try std.Thread.spawn(&data.server_channel, LambdaPlease.f);
            },
            else => unreachable,
        }

        try self.startThreads();
    }

    pub fn stopEngine(self: *GameEngineClient) void {
        core.debug.warn("close");
        self.channel.close();
        switch (self.connection) {
            Connection.child_process => |child_process| {
                _ = child_process.wait() catch undefined;
            },
            Connection.thread => |*data| {
                //data.send_pipe[1].close();
                //data.recv_pipe[1].close();
                data.core_thread.wait();
            },
        }
        self.stay_alive.set(0);
        self.send_thread.wait();
        self.recv_thread.wait();
        core.debug.warn("all threads done");
    }
    fn isAlive(self: *GameEngineClient) bool {
        return self.stay_alive.get() != 0;
    }

    pub fn pollResponse(self: *GameEngineClient) ?Response {
        return queueGet(Response, &self.response_inbox) orelse return null;
    }

    fn sendMain(self: *GameEngineClient) void {
        core.debug.nameThisThread("client send");
        core.debug.warn("init");
        defer core.debug.warn("shutdown");
        while (self.isAlive()) {
            const request = queueGet(Request, &self.request_outbox) orelse {
                // :ResidentSleeper:
                std.time.sleep(17 * std.time.millisecond);
                continue;
            };
            self.channel.writeRequest(request) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *GameEngineClient) void {
        core.debug.nameThisThread("client recv");
        core.debug.warn("init");
        defer core.debug.warn("shutdown");
        while (self.isAlive()) {
            const response: Response = self.channel.readResponse() catch {
                @panic("TODO: proper error handling");
            } orelse {
                core.debug.warn("clean shutdown");
                break;
            };
            queuePut(Response, &self.response_inbox, response) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    pub fn rewind(self: *GameEngineClient) !void {
        try queuePut(Request, &self.request_outbox, Request{ .rewind = {} });
    }
    pub fn move(self: *GameEngineClient, direction: Coord) !void {
        try queuePut(Request, &self.request_outbox, Request{ .act = Action{ .move = direction } });
    }
    pub fn attack(self: *GameEngineClient, direction: Coord) !void {
        try queuePut(Request, &self.request_outbox, Request{ .act = Action{ .attack = direction } });
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
    core.debug.init();
    core.debug.nameThisThread("test main");
    core.debug.warn("start test");
    defer core.debug.warn("exit test");
    var _client: GameEngineClient = undefined;
    var client = &_client;
    try client.startAsThread();
    defer client.stopEngine();

    const startup_response = pollSync(client);
    core.debug.warn("got response");
    const starting_position = startup_response.static_perception.self.abs_position;

    try client.move(makeCoord(1, 0));
    const post_move_response = pollSync(client);
    const new_position = post_move_response.stuff_happens[1].individuals_by_location[0].abs_position;

    std.testing.expect(new_position.minus(starting_position).equals(makeCoord(1, 0)));
}

fn pollSync(client: *GameEngineClient) Response {
    while (true) {
        if (client.pollResponse()) |response| {
            return response;
        }
        std.time.sleep(17 * std.time.millisecond);
    }
}
