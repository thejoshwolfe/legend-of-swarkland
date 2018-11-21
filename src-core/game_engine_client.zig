const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const BaseChannel = core.protocol.BaseChannel(std.os.File.ReadError, std.os.File.WriteError);
const Request = core.protocol.Request;
const Action = core.protocol.Action;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const GameState = core.game_state.GameState;

pub const GameEngine = struct {
    child_process: *std.os.ChildProcess,
    in_adapter: std.os.File.InStream,
    out_adapter: std.os.File.OutStream,
    channel: BaseChannel,
    send_thread: *std.os.Thread,
    recv_thread: *std.os.Thread,
    request_outbox: std.atomic.Queue(Request),
    response_inbox: std.atomic.Queue(Response),
    stay_alive: std.atomic.Int(u8),

    game_state: GameState,
    position_animation: ?MoveAnimation,

    pub fn startEngine(self: *GameEngine) !void {
        var dir = try std.os.selfExeDirPathAlloc(std.heap.c_allocator);
        defer std.heap.c_allocator.free(dir);
        var path = try std.os.path.join(std.heap.c_allocator, dir, "legend-of-swarkland_headless");
        defer std.heap.c_allocator.free(path);

        const args = []const []const u8{path};
        self.child_process = try std.os.ChildProcess.init(args, std.heap.c_allocator);
        self.child_process.stdout_behavior = std.os.ChildProcess.StdIo.Pipe;
        self.child_process.stdin_behavior = std.os.ChildProcess.StdIo.Pipe;
        try self.child_process.spawn();
        self.in_adapter = self.child_process.stdout.?.inStream();
        self.out_adapter = self.child_process.stdin.?.outStream();
        self.channel = BaseChannel.create(&self.in_adapter.stream, &self.out_adapter.stream);
        self.stay_alive = std.atomic.Int(u8).init(1);

        self.game_state = GameState.init();
        self.position_animation = null;

        // start threads last
        self.send_thread = try std.os.spawnThread(self, sendMain);
        self.recv_thread = try std.os.spawnThread(self, recvMain);
    }
    pub fn stopEngine(self: *GameEngine) void {
        self.stay_alive.set(0);
        _ = self.child_process.kill() catch undefined;
        // io with the child should now produce errors and clean up the threads
        self.send_thread.wait();
        self.recv_thread.wait();
        core.debug.warn("all threads done\n");
    }
    fn isAlive(self: *GameEngine) bool {
        return self.stay_alive.get() != 0;
    }

    pub fn pollEvents(self: *GameEngine, now: u32) void {
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

    fn sendMain(self: *GameEngine) void {
        defer core.debug.warn("send shutdown\n");
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

    fn recvMain(self: *GameEngine) void {
        defer core.debug.warn("recv shutdown\n");
        while (self.isAlive()) {
            const response: Response = self.channel.readResponse() catch |err| {
                switch (err) {
                    error.EndOfStream => {
                        if (self.isAlive()) {
                            @panic("unexpected EOF");
                        }
                        // but this is expected when we're trying to shutdown
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

    pub fn rewind(self: *GameEngine) !void {
        try queuePut(Request, &self.request_outbox, Request{ .rewind = {} });
    }
    pub fn move(self: *GameEngine, direction: Coord) !void {
        try queuePut(Request, &self.request_outbox, Request{ .act = Action{ .move = direction } });
    }
};

pub const MoveAnimation = struct {
    from: Coord,
    to: Coord,
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
