const std = @import("std");
const core = @import("../index.zig");
const game_server = @import("../server/game_server.zig");
const debugPrintAction = game_server.debugPrintAction;
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Socket = core.protocol.Socket;
const SomeQueues = core.protocol.SomeQueues;
const Request = core.protocol.Request;
const NewGameSettings = core.protocol.NewGameSettings;
const Action = core.protocol.Action;
const Response = core.protocol.Response;
const Event = core.protocol.Event;

const cheatcodes = @import("cheatcodes.zig");
const the_levels = @import("../server/map_gen.zig").the_levels;

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
        child_process: *std.ChildProcess,
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
    current_level: usize,
    starting_level: usize,
    moves_per_level: []usize,
    pending_forward_actions: usize,
    pending_backward_actions: usize,

    fn init(self: *GameEngineClient) !void {
        self.queues.init(allocator);
        self.starting_level = 0;
        self.current_level = 0;
        self.moves_per_level = try allocator.alloc(usize, the_levels.len);
        std.mem.set(usize, self.moves_per_level, 0);
        // starting a new game looks like undoing.
        self.pending_forward_actions = 0;
        self.moves_per_level[0] = 1;
        self.pending_backward_actions = 1;
    }

    pub const StartAsChildProcessOptions = struct {
        exe_path: []const u8 = "",
    };
    pub fn startAsChildProcess(self: *GameEngineClient, options: StartAsChildProcessOptions) !void {
        try self.init();

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
                var child_process = try std.ChildProcess.init(&args, allocator);
                child_process.stdout_behavior = std.ChildProcess.StdIo.Pipe;
                child_process.stdin_behavior = std.ChildProcess.StdIo.Pipe;
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
    pub fn startAsThread(self: *GameEngineClient) !void {
        try self.init();

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

    pub fn stopEngine(self: *GameEngineClient) void {
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

    pub fn startGame(self: *GameEngineClient, new_game_settings: NewGameSettings) !void {
        try self.enqueueRequest(Request{ .start_game = new_game_settings });
    }

    pub fn act(self: *GameEngineClient, action: Action) !void {
        try self.enqueueRequest(Request{ .act = action });
        debugPrintAction(0, action);
    }
    pub fn rewind(self: *GameEngineClient) !void {
        const data = self.getMoveCounterInfo();
        if (data.moves_into_current_level == 0 and data.current_level <= self.starting_level) {
            core.debug.move_counter.print("can't undo the start of the starting level", .{});
            return;
        }

        try self.enqueueRequest(.rewind);
        core.debug.actions.print("[rewind]", .{});
    }

    pub fn move(self: *GameEngineClient, direction: Coord) !void {
        return self.act(Action{ .move = direction });
    }
    pub fn attack(self: *GameEngineClient, direction: Coord) !void {
        return self.act(Action{ .attack = direction });
    }
    pub fn kick(self: *GameEngineClient, direction: Coord) !void {
        return self.act(Action{ .kick = direction });
    }

    pub fn beatLevelMacro(self: *GameEngineClient, how_many: usize) !void {
        const data = self.getMoveCounterInfo();
        if (data.moves_into_current_level > 0) {
            core.debug.move_counter.print("beat level is restarting first with {} undos.", .{data.moves_into_current_level});
            for (times(data.moves_into_current_level)) |_| {
                try self.rewind();
            }
        }
        for (times(how_many)) |_, i| {
            if (data.current_level + i >= cheatcodes.beat_level_actions.len) {
                core.debug.move_counter.print("beat level is stopping at the end of the levels.", .{});
                return;
            }
            for (cheatcodes.beat_level_actions[data.current_level + i]) |action| {
                try self.enqueueRequest(Request{ .act = action });
            }
        }
    }
    pub fn unbeatLevelMacro(self: *GameEngineClient, how_many: usize) !void {
        for (times(how_many)) |_| {
            const data = self.getMoveCounterInfo();
            if (data.moves_into_current_level > 0) {
                core.debug.move_counter.print("unbeat level is restarting first with {} undos.", .{data.moves_into_current_level});
                for (times(data.moves_into_current_level)) |_| {
                    try self.rewind();
                }
            } else if (data.current_level <= self.starting_level) {
                core.debug.move_counter.print("unbeat level reached the first level.", .{});
                return;
            } else {
                const unbeat_level = data.current_level - 1;
                const moves_counter = self.moves_per_level[unbeat_level];
                core.debug.move_counter.print("unbeat level is undoing level {} with {} undos.", .{ unbeat_level, moves_counter });
                for (times(moves_counter)) |_| {
                    try self.rewind();
                }
            }
        }
    }

    pub fn restartLevel(self: *GameEngineClient) !void {
        const data = self.getMoveCounterInfo();
        core.debug.move_counter.print("restarting is undoing {} times.", .{data.moves_into_current_level});
        for (times(data.moves_into_current_level)) |_| {
            try self.rewind();
        }
    }

    const MoveCounterData = struct {
        current_level: usize,
        moves_into_current_level: usize,
    };
    fn getMoveCounterInfo(self: GameEngineClient) MoveCounterData {
        var remaining_pending_backward_actions = self.pending_backward_actions;
        var remaining_pending_forward_actions = self.pending_forward_actions;
        var pending_current_level = self.current_level;
        while (remaining_pending_backward_actions > self.moves_per_level[pending_current_level] + remaining_pending_forward_actions) {
            core.debug.move_counter.print("before the start of a level: (m[{}]={})+{}-{} < zero,", .{
                pending_current_level,
                self.moves_per_level[pending_current_level],
                remaining_pending_forward_actions,
                remaining_pending_backward_actions,
            });
            remaining_pending_backward_actions -= self.moves_per_level[pending_current_level] + remaining_pending_forward_actions;
            remaining_pending_forward_actions = 0;
            if (pending_current_level == 0) {
                return .{
                    .current_level = 0,
                    .moves_into_current_level = 0,
                };
            }
            pending_current_level -= 1;
            core.debug.move_counter.print("looking at the previous level.", .{});
        }
        const moves_counter = self.moves_per_level[pending_current_level] + remaining_pending_forward_actions - remaining_pending_backward_actions;
        core.debug.move_counter.print("move counter data: (m[{}]={})+{}-{}={}", .{
            pending_current_level,
            self.moves_per_level[pending_current_level],
            remaining_pending_forward_actions,
            remaining_pending_backward_actions,
            moves_counter,
        });
        return .{
            .current_level = pending_current_level,
            .moves_into_current_level = moves_counter,
        };
    }

    pub fn stopUndoPastLevel(self: *GameEngineClient, starting_level: usize) void {
        self.starting_level = starting_level;
    }

    fn enqueueRequest(self: *GameEngineClient, request: Request) !void {
        switch (request) {
            .act => {
                self.pending_forward_actions += 1;
            },
            .rewind => {
                self.pending_backward_actions += 1;
            },
            .start_game => {},
        }
        try self.queues.enqueueRequest(request);
    }
    pub fn takeResponse(self: *GameEngineClient) ?Response {
        const response = self.queues.takeResponse() orelse return null;
        // Monitor the responses to count moves per level.
        switch (response) {
            .stuff_happens => |happening| {
                self.pending_forward_actions -= 1;
                self.moves_per_level[happening.frames[0].completed_levels] += 1;
                self.current_level = lastPtr(happening.frames).completed_levels;
            },
            .load_state => |frame| {
                // This can also happen for the initial game load.
                self.pending_backward_actions -= 1;
                self.moves_per_level[frame.completed_levels] -= 1;
                self.current_level = frame.completed_levels;
            },
            .reject_request => |request| {
                switch (request) {
                    .act => {
                        self.pending_forward_actions -= 1;
                    },
                    .rewind => {
                        self.pending_backward_actions -= 1;
                    },
                    .start_game => {},
                }
            },
        }
        return response;
    }
};

test "basic interaction" {
    // init
    core.debug.init();
    core.debug.nameThisThread("test main");
    defer core.debug.unnameThisThread();
    core.debug.testing.print("start test", .{});
    defer core.debug.testing.print("exit test", .{});
    var _client: GameEngineClient = undefined;
    var client = &_client;
    try client.startAsThread();
    defer client.stopEngine();
    try client.startGame(.puzzle_levels);

    const startup_response = client.queues.waitAndTakeResponse().?;
    try std.testing.expect(startup_response.load_state.self.position.small.equals(makeCoord(0, 0)));
    core.debug.testing.print("startup done", .{});

    // move
    try client.move(makeCoord(1, 0));
    {
        const response = client.queues.waitAndTakeResponse().?;
        const frames = response.stuff_happens.frames;
        try std.testing.expect(frames[frames.len - 1].self.position.small.equals(makeCoord(1, 0)));
    }
    core.debug.testing.print("move looks good", .{});

    // rewind
    try client.rewind();
    {
        const response = client.queues.waitAndTakeResponse().?;

        try std.testing.expect(response.load_state.self.position.small.equals(makeCoord(0, 0)));
    }
    core.debug.testing.print("rewind looks good", .{});
}

test "server child process" {
    // init
    core.debug.init();
    core.debug.nameThisThread("test main");
    defer core.debug.unnameThisThread();
    core.debug.testing.print("start test", .{});
    defer core.debug.testing.print("exit test", .{});
    var _client: GameEngineClient = undefined;
    var client = &_client;
    try client.startAsChildProcess(.{
        .exe_path = "zig-out/bin/legend-of-swarkland_headless",
    });
    defer client.stopEngine();
    try client.startGame(.puzzle_levels);

    const startup_response = client.queues.waitAndTakeResponse().?;
    try std.testing.expect(startup_response.load_state.self.position.small.equals(makeCoord(0, 0)));
    core.debug.testing.print("startup done", .{});
}

fn lastPtr(arr: anytype) @TypeOf(&arr[arr.len - 1]) {
    return &arr[arr.len - 1];
}

fn times(n: usize) []const void {
    return @as([(1 << (@sizeOf(usize) * 8) - 1)]void, undefined)[0..n];
}
