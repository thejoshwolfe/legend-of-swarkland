const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Channel = core.protocol.Channel;
const Request = core.protocol.Request;
const Action = core.protocol.Action;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const GameState = core.game_state.GameState;

const Connection = union(enum) {
    child_process: *std.os.ChildProcess,
    thread: ThreadData,
    const ThreadData = struct {
        core_thread: *std.os.Thread,
        send_pipe: [2]std.os.File,
        recv_pipe: [2]std.os.File,
        server_channel: Channel,
    };
};

pub const GameEngineClient = struct {
    connection: Connection,
    channel: Channel,
    send_thread: *std.os.Thread,
    recv_thread: *std.os.Thread,
    request_outbox: std.atomic.Queue(Request),
    response_inbox: std.atomic.Queue(Response),
    stay_alive: std.atomic.Int(u8),

    game_state: GameState,
    position_animation: ?MoveAnimation,

    fn init(self: *GameEngineClient) void {
        self.stay_alive = std.atomic.Int(u8).init(1);

        self.game_state = GameState.init();
        self.position_animation = null;
    }
    fn startThreads(self: *GameEngineClient) !void {
        // start threads last
        self.send_thread = try std.os.spawnThread(self, sendMain);
        self.recv_thread = try std.os.spawnThread(self, recvMain);
    }

    pub fn startAsChildProcess(self: *GameEngineClient) !void {
        self.init();

        var dir = try std.os.selfExeDirPathAlloc(std.heap.c_allocator);
        defer std.heap.c_allocator.free(dir);
        var path = try std.os.path.join(std.heap.c_allocator, dir, "legend-of-swarkland_headless");
        defer std.heap.c_allocator.free(path);

        self.connection = Connection{ .child_process = undefined };
        switch (self.connection) {
            // TODO: This switch is a workaround for lack of guaranteed copy elision.
            Connection.child_process => |*child_process| {
                const args = []const []const u8{path};
                child_process.* = try std.os.ChildProcess.init(args, std.heap.c_allocator);
                child_process.*.stdout_behavior = std.os.ChildProcess.StdIo.Pipe;
                child_process.*.stdin_behavior = std.os.ChildProcess.StdIo.Pipe;
                try child_process.*.spawn();
                self.channel.init(child_process.*.stdout.?, child_process.*.stdin.?);
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
                self.channel.init(data.recv_pipe[0], data.send_pipe[1]);
                data.server_channel.init(data.send_pipe[0], data.recv_pipe[1]);
                data.core_thread = try std.os.spawnThread(&data.server_channel, core.game_server.server_main);
            },
            else => unreachable,
        }

        try self.startThreads();
    }

    pub fn stopEngine(self: *GameEngineClient) void {
        core.debug.warn("close\n");
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
        core.debug.warn("all threads done\n");
    }
    fn isAlive(self: *GameEngineClient) bool {
        return self.stay_alive.get() != 0;
    }

    pub fn pollEvents(self: *GameEngineClient, now: u32) void {
        while (true) {
            switch (queueGet(Response, &self.response_inbox) orelse return) {
                Response.event => |event| {
                    self.game_state.applyEvent(event);
                    // animations
                    switch (event) {
                        Event.moved => |e| {
                            self.position_animation = MoveAnimation{
                                .from = e.from,
                                .to = e.to,
                                .start_time = now,
                                .end_time = now + 200,
                            };
                        },
                        Event.init_state => {},
                    }
                },
                Response.undo => |event| {
                    self.game_state.undoEvent(event);
                    self.position_animation = null;
                },
            }
        }
    }

    fn sendMain(self: *GameEngineClient) void {
        core.debug.nameThisThread("client send");
        core.debug.warn("init\n");
        defer core.debug.warn("shutdown\n");
        while (self.isAlive()) {
            const request = queueGet(Request, &self.request_outbox) orelse {
                // :ResidentSleeper:
                std.os.time.sleep(17 * std.os.time.millisecond);
                continue;
            };
            self.channel.writeRequest(request) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *GameEngineClient) void {
        core.debug.nameThisThread("client recv");
        core.debug.warn("init\n");
        defer core.debug.warn("shutdown\n");
        while (self.isAlive()) {
            const response: Response = self.channel.readResponse() catch |err| {
                switch (err) {
                    error.CleanShutdown => {
                        core.debug.warn("clean shutdown\n");
                        break;
                    },
                    else => @panic("TODO: proper error handling"),
                }
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
};

pub const MoveAnimation = struct {
    from: [2]Coord,
    to: [2]Coord,
    start_time: u32,
    end_time: u32,
};

fn queuePut(comptime T: type, queue: *std.atomic.Queue(T), x: T) !void {
    const Node = std.atomic.Queue(T).Node;
    const node: *Node = try std.heap.c_allocator.createOne(Node);
    node.data = x;
    queue.put(node);
}
fn queueGet(comptime T: type, queue: *std.atomic.Queue(T)) ?T {
    const Node = std.atomic.Queue(T).Node;
    const node: *Node = queue.get() orelse return null;
    defer std.heap.c_allocator.destroy(node);
    return node.data;
}
fn makePipe() ![2]std.os.File {
    // copied from std child_process.zig
    var fds: [2]i32 = undefined;
    const err = std.os.posix.getErrno(std.os.posix.pipe(&fds));
    if (err > 0) {
        return switch (err) {
            std.os.posix.EMFILE, std.os.posix.ENFILE => error.SystemResources,
            else => std.os.unexpectedErrorPosix(err),
        };
    }
    return []std.os.File{
        std.os.File.openHandle(fds[0]),
        std.os.File.openHandle(fds[1]),
    };
}
