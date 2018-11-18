const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const ClientChannel = core.protocol.ClientChannel(std.os.File.ReadError, std.os.File.WriteError);
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const MovedEvent = core.protocol.MovedEvent;

pub const GameEngine = struct {
    child_process: *std.os.ChildProcess,
    in_adapter: std.os.File.InStream,
    out_adapter: std.os.File.OutStream,
    channel: ClientChannel,
    position: Coord,
    send_thread: *std.os.Thread,
    recv_thread: *std.os.Thread,
    action_outbox: std.atomic.Queue(Action),
    event_inbox: std.atomic.Queue(Event),

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
        self.channel = ClientChannel.create(&self.in_adapter.stream, &self.out_adapter.stream);

        self.position = makeCoord(0, 0);
        self.send_thread = try std.os.spawnThread(self, sendMain);
        self.recv_thread = try std.os.spawnThread(self, recvMain);
    }
    pub fn stopEngine(self: *GameEngine) void {
        _ = self.child_process.kill() catch undefined;
    }

    pub fn pollEvents(self: *GameEngine) void {
        while (true) {
            switch (queueGet(Event, &self.event_inbox) orelse return) {
                Event.Moved => |event| {
                    self.position = event.to;
                },
                Event._Unused => unreachable,
            }
        }
    }

    pub fn move(self: *GameEngine, direction: Coord) !void {
        try queuePut(Action, &self.action_outbox, Action{ .Move = direction });
    }

    fn sendMain(self: *GameEngine) void {
        while (true) {
            const action: Action = queueGet(Action, &self.action_outbox) orelse {
                // :ResidentSleeper:
                std.os.time.sleep(17 * std.os.time.millisecond);
                continue;
            };
            self.channel.writeAction(action) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }

    fn recvMain(self: *GameEngine) !void {
        while (true) {
            const event: Event = try self.channel.readEvent();
            queuePut(Event, &self.event_inbox, event) catch |err| {
                @panic("TODO: proper error handling");
            };
        }
    }
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
