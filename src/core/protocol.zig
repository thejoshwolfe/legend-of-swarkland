const std = @import("std");
const Allocator = std.mem.Allocator;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const CardinalDirection = core.geometry.CardinalDirection;
const AtomicQueue = @import("./old_std_atomic_queue.zig").Queue;

pub const Floor = enum {
    unknown,
    dirt,
    grass,
    sand,
    sandstone,
    marble,
    lava,
    hatch,
    stairs_down,
    water,
    water_bloody,
    water_deep,
    grass_and_water_edge_east,
    grass_and_water_edge_southeast,
    grass_and_water_edge_south,
    grass_and_water_edge_southwest,
    grass_and_water_edge_west,
    grass_and_water_edge_northwest,
    grass_and_water_edge_north,
    grass_and_water_edge_northeast,
    unknown_floor,
};

pub const Wall = enum {
    unknown,
    air,
    dirt,
    stone,
    sandstone,
    sandstone_cracked,
    tree_northwest,
    tree_northeast,
    tree_southwest,
    tree_southeast,
    bush,
    door_open,
    door_closed,
    angel_statue,
    chest,
    polymorph_trap_centaur,
    polymorph_trap_kangaroo,
    polymorph_trap_turtle,
    polymorph_trap_blob,
    polymorph_trap_human,
    unknown_polymorph_trap,
    polymorph_trap_rhino_west,
    polymorph_trap_rhino_east,
    polymorph_trap_blob_west,
    polymorph_trap_blob_east,
    unknown_polymorph_trap_west,
    unknown_polymorph_trap_east,
    unknown_wall,
};

pub const Species = union(enum) {
    human,
    orc,
    centaur: enum {
        archer,
        warrior,
    },
    turtle,
    rhino,
    kangaroo,
    blob: enum {
        small_blob,
        large_blob,
    },
    wolf,
    rat,
    wood_golem,
    scorpion,
    brown_snake,
    ant: enum {
        worker,
        queen,
    },
    minotaur,
    siren: enum {
        water,
        land,
    },
    ogre,
};

pub const TerrainChunk = struct {
    position: Coord,
    width: u16,
    height: u16,
    matrix: []TerrainSpace,
};
pub const TerrainSpace = struct {
    floor: Floor,
    wall: Wall,
};

pub const Request = union(enum) {
    act: Action,
    rewind,
    start_game: NewGameSettings,
};

pub const NewGameSettings = union(enum) {
    regular: struct {
        seed: ?u64 = null,
    },
    puzzle_levels,
};

pub const Action = union(enum) {
    wait,
    move: CardinalDirection,
    charge,
    grow: CardinalDirection,
    shrink: u1,
    attack: CardinalDirection,
    kick: CardinalDirection,
    nibble,
    stomp,
    lunge: CardinalDirection,
    open_close: CardinalDirection,
    pick_up_unequipped,
    pick_up_and_equip,
    nock_arrow,
    fire_bow: CardinalDirection,
    defend: CardinalDirection,
    unequip: EquippedItem,
    equip: EquippedItem,

    cheatcode_warp: u3,
};

pub const Response = union(enum) {
    /// this happens on startup (from your perspective) and on rewind
    load_state: PerceivedFrame,

    /// a typical turn happens
    stuff_happens: PerceivedHappening,

    /// ur doin it rong, and nothing happened. try again.
    reject_request: Request,
};

/// what you see each turn
pub const PerceivedHappening = struct {
    /// Sequential order of simultaneous events.
    /// The last frame will always have all things with .none PerceivedActivity,
    /// unless you're are dead, in which case, the last frame includes your death.
    frames: []PerceivedFrame,
};

/// Represents what you can observe with your eyes happening simultaneously.
/// There can be multiple of these per turn.
pub const PerceivedFrame = struct {
    terrain: TerrainChunk,

    self: PerceivedThing,
    others: []PerceivedThing,
    completed_levels: u32,
};

pub const ThingPosition = union(enum) {
    small: Coord,

    /// 0: head, 1: tail
    large: [2]Coord,
};

pub const PerceivedThing = struct {
    position: ThingPosition,
    kind: union(enum) {
        individual: struct {
            species: Species,

            status_conditions: StatusConditions,
            equipment: Equipment,

            activity: PerceivedActivity,
        },
        item: FloorItem,
    },
};

pub const Equipment = struct {
    held: DataInt = 0,
    /// equipped is a subset of held.
    equipped: DataInt = 0,

    const DataInt = std.meta.Int(.unsigned, std.meta.fields(EquippedItem).len);

    pub fn is_held(self: Equipment, item: EquippedItem) bool {
        const bit = @as(DataInt, 1) << @intFromEnum(item);
        return self.held & bit != 0;
    }
    pub fn is_equipped(self: Equipment, item: EquippedItem) bool {
        const bit = @as(DataInt, 1) << @intFromEnum(item);
        return self.equipped & bit != 0;
    }
    pub fn set(self: *Equipment, item: EquippedItem, held: bool, equipped: bool) void {
        std.debug.assert(!(equipped and !held)); // equipped is a subset of held.
        const bit = @as(DataInt, 1) << @intFromEnum(item);
        if (held) {
            self.held |= bit;
        } else {
            self.held &= ~bit;
        }
        if (equipped) {
            self.equipped |= bit;
        } else {
            self.equipped &= ~bit;
        }
    }
};
/// TODO: move out of this file. (game logic?)
pub const EquipmentSlot = enum {
    none,
    right_hand,
    left_hand,
};
pub const EquippedItem = enum {
    shield,
    axe,
    torch,
    dagger,
};
pub const FloorItem = enum {
    shield,
    axe,
    torch,
    dagger,
};

pub const StatusConditions = u9;
pub const StatusCondition_wounded_leg: StatusConditions = 0x1;
pub const StatusCondition_limping: StatusConditions = 0x2;
pub const StatusCondition_grappling: StatusConditions = 0x4;
pub const StatusCondition_grappled: StatusConditions = 0x8;
pub const StatusCondition_digesting: StatusConditions = 0x10;
pub const StatusCondition_being_digested: StatusConditions = 0x20;
pub const StatusCondition_malaise: StatusConditions = 0x40;
pub const StatusCondition_pain: StatusConditions = 0x80;
pub const StatusCondition_arrow_nocked: StatusConditions = 0x100;

pub const PerceivedActivity = union(enum) {
    none,

    movement: Coord,
    failed_movement: Coord,
    growth: Coord,
    failed_growth: Coord,
    shrink: u1,

    attack: Attack,
    nibble,
    stomp,
    defend: CardinalDirection,

    kick: Coord,
    polymorph,

    death,

    pub const Attack = struct {
        direction: Coord,
        distance: i32,
    };
};

/// Despite all the generic elegance of the Channel classes,
/// this is what we use everywhere.
pub const Socket = struct {
    in_stream: std.fs.File.Reader,
    out_stream: std.fs.File.Writer,

    pub fn init(
        in_stream: std.fs.File.Reader,
        out_stream: std.fs.File.Writer,
    ) Socket {
        return Socket{
            .in_stream = in_stream,
            .out_stream = out_stream,
        };
    }

    pub const FileInChannel = InChannel(std.fs.File.Reader);
    pub fn in(self: *Socket, allocator: Allocator) FileInChannel {
        return initInChannel(allocator, self.in_stream);
    }

    pub const FileOutChannel = OutChannel(std.fs.File.Writer);
    pub fn out(self: *Socket) FileOutChannel {
        return initOutChannel(self.out_stream);
    }

    pub fn close(self: *Socket, final_message: anytype) void {
        self.out().write(final_message) catch {};
        self.out_stream.file.close();
        // FIXME: revisit closing the in_stream when we have async/await maybe.
    }
};

pub fn initOutChannel(out_stream: anytype) OutChannel(@TypeOf(out_stream)) {
    return OutChannel(@TypeOf(out_stream)).init(out_stream);
}
pub fn OutChannel(comptime Writer: type) type {
    return struct {
        const Self = @This();

        stream: Writer,
        pub fn init(stream: Writer) Self {
            return Self{ .stream = stream };
        }

        pub fn write(self: Self, x: anytype) !void {
            const T = @TypeOf(x);
            switch (@typeInfo(T)) {
                .int => return self.writeInt(x),
                .bool => return self.writeInt(@intFromBool(x)),
                .@"enum" => return self.writeInt(@intFromEnum(x)),
                .@"struct" => |info| {
                    inline for (info.fields) |field| {
                        try self.write(@field(x, field.name));
                    }
                },
                .array => {
                    for (x) |x_i| {
                        try self.write(x_i);
                    }
                },
                .pointer => |info| {
                    switch (info.size) {
                        .Slice => {
                            // []T
                            try self.writeInt(x.len);
                            for (x) |x_i| {
                                try self.write(x_i);
                            }
                        },
                        .One => {
                            if (info.is_const and @typeInfo(info.child) == .Array) {
                                // *const [N]T
                                try self.writeInt(x.len);
                                for (x) |x_i| {
                                    try self.write(x_i);
                                }
                            }
                        },
                        else => @compileError("not supported: " ++ @typeName(T)),
                    }
                },
                .@"union" => |info| {
                    const tag = @as(info.tag_type.?, x);
                    try self.writeInt(@intFromEnum(tag));
                    inline for (info.fields) |u_field| {
                        if (tag == @field(T, u_field.name)) {
                            // FIXME: this `if` is because inferred error sets require at least one error.
                            return if (u_field.type != void) {
                                try self.write(@field(x, u_field.name));
                            };
                        }
                    }
                    unreachable;
                },
                .optional => {
                    if (x) |child| {
                        try self.write(true);
                        try self.write(child);
                    } else {
                        try self.write(false);
                    }
                },
                else => @compileError("not supported: " ++ @typeName(T)),
            }
        }

        pub fn writeInt(self: Self, x: anytype) !void {
            const int_info = @typeInfo(@TypeOf(x)).int;
            const T_aligned = @Type(std.builtin.Type{
                .int = .{
                    .signedness = int_info.signedness,
                    .bits = @divTrunc(int_info.bits + 7, 8) * 8,
                },
            });
            try self.stream.writeInt(T_aligned, x, .little);
        }
    };
}

pub fn initInChannel(allocator: Allocator, in_stream: anytype) InChannel(@TypeOf(in_stream)) {
    return InChannel(@TypeOf(in_stream)).init(allocator, in_stream);
}
pub fn InChannel(comptime Reader: type) type {
    return struct {
        const Self = @This();

        allocator: Allocator,
        stream: Reader,
        pub fn init(allocator: Allocator, stream: Reader) Self {
            return Self{
                .allocator = allocator,
                .stream = stream,
            };
        }

        pub fn read(self: Self, comptime T: type) !T {
            switch (@typeInfo(T)) {
                .int => return self.readInt(T),
                .bool => return 0 != try self.readInt(u1),
                .@"enum" => return @as(T, @enumFromInt(try self.readInt(std.meta.Tag(T)))),
                .@"struct" => |info| {
                    var x: T = undefined;
                    inline for (info.fields) |field| {
                        @field(x, field.name) = try self.read(field.type);
                    }
                    return x;
                },
                .array => {
                    var x: T = undefined;
                    for (&x) |*x_i| {
                        x_i.* = try self.read(@TypeOf(x_i.*));
                    }
                    return x;
                },
                .pointer => |info| {
                    switch (info.size) {
                        .Slice => {
                            const len = try self.readInt(usize);
                            var x = try self.allocator.alloc(info.child, len);
                            for (&x) |*x_i| {
                                x_i.* = try self.read(info.child);
                            }
                            return x;
                        },
                        else => @compileError("not supported: " ++ @typeName(T)),
                    }
                },
                .@"union" => |info| {
                    const tag = try self.read(info.tag_type.?);
                    inline for (info.fields) |u_field| {
                        if (tag == @field(T, u_field.name)) {
                            // FIXME: this `if` is because inferred error sets require at least one error.
                            return @unionInit(T, u_field.name, if (u_field.type == void) {} else try self.read(u_field.type));
                        }
                    }
                    unreachable;
                },
                .optional => |info| {
                    const non_null = try self.read(bool);
                    if (!non_null) return null;
                    return try self.read(info.child);
                },
                else => @compileError("not supported: " ++ @typeName(T)),
            }
        }

        pub fn readInt(self: Self, comptime T: type) !T {
            const int_info = @typeInfo(T).int;
            const T_aligned = @Type(std.builtin.Type{
                .int = .{
                    .signedness = int_info.signedness,
                    .bits = @divTrunc(int_info.bits + 7, 8) * 8,
                },
            });
            const x_aligned = try self.stream.readInt(T_aligned, .little);
            return std.math.cast(T, x_aligned) orelse return error.Overflow;
        }
    };
}

pub const SomeQueues = struct {
    allocator: Allocator,
    requests_alive: std.atomic.Value(bool),
    requests: AtomicQueue(Request),
    responses_alive: std.atomic.Value(bool),
    responses: AtomicQueue(Response),

    pub fn init(allocator: Allocator) @This() {
        return .{
            .allocator = allocator,
            .requests_alive = std.atomic.Value(bool).init(true),
            .requests = AtomicQueue(Request).init(),
            .responses_alive = std.atomic.Value(bool).init(true),
            .responses = AtomicQueue(Response).init(),
        };
    }

    pub fn closeRequests(self: *SomeQueues) void {
        self.requests_alive.store(false, .monotonic);
    }
    pub fn enqueueRequest(self: *SomeQueues, request: Request) !void {
        try self.queuePut(Request, &self.requests, request);
    }

    /// null means requests have been closed
    pub fn waitAndTakeRequest(self: *SomeQueues) ?Request {
        while (self.requests_alive.load(.monotonic)) {
            if (self.queueGet(Request, &self.requests)) |response| {
                return response;
            }
            // :ResidentSleeper:
            std.time.sleep(17 * std.time.ns_per_ms);
        }
        return null;
    }

    pub fn closeResponses(self: *SomeQueues) void {
        self.responses_alive.store(false, .monotonic);
    }
    pub fn enqueueResponse(self: *SomeQueues, response: Response) !void {
        try self.queuePut(Response, &self.responses, response);
    }
    pub fn takeResponse(self: *SomeQueues) ?Response {
        return self.queueGet(Response, &self.responses);
    }

    /// null means responses have been closed
    pub fn waitAndTakeResponse(self: *SomeQueues) ?Response {
        while (self.responses_alive.load(.monotonic)) {
            if (self.takeResponse()) |response| {
                return response;
            }
            // :ResidentSleeper:
            std.time.sleep(17 * std.time.ns_per_ms);
        }
        return null;
    }

    fn queuePut(self: *SomeQueues, comptime T: type, queue: *AtomicQueue(T), x: T) !void {
        const Node = AtomicQueue(T).Node;
        const node: *Node = try self.allocator.create(Node);
        node.data = x;
        queue.put(node);
    }
    fn queueGet(self: *SomeQueues, comptime T: type, queue: *AtomicQueue(T)) ?T {
        const Node = AtomicQueue(T).Node;
        const node: *Node = queue.get() orelse return null;
        defer self.allocator.destroy(node);
        const hack = node.data; // TODO: https://github.com/ziglang/zig/issues/961
        return hack;
    }
};

test "channel int" {
    var buffer = [_]u8{0} ** 256;
    var _out_stream = std.io.fixedBufferStream(&buffer);
    var _in_stream = std.io.fixedBufferStream(&buffer);
    var out_channel = initOutChannel(&_out_stream.writer());
    var in_channel = initInChannel(std.testing.allocator, &_in_stream.reader());

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(@as(u0, 0)); // zero size
    try out_channel.writeInt(@as(u3, 2));
    try out_channel.writeInt(@as(i3, -2));
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{ 2, 0xfe }));
    try std.testing.expect(0 == try in_channel.read(u0));
    try std.testing.expect(2 == try in_channel.read(u3));
    try std.testing.expect(-2 == try in_channel.read(i3));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(@as(u64, 3));
    try out_channel.writeInt(@as(i64, 4));
    try out_channel.writeInt(@as(i64, -5));
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{
        3,    0,    0,    0,    0,    0,    0,    0,
        4,    0,    0,    0,    0,    0,    0,    0,
        0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    }));
    try std.testing.expect(3 == try in_channel.read(u64));
    try std.testing.expect(4 == try in_channel.read(i64));
    try std.testing.expect(-5 == try in_channel.read(i64));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(@as(u64, 0xffffffffffffffff));
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    }));
    try std.testing.expect(0xffffffffffffffff == try in_channel.read(u64));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(@as(i64, -0x8000000000000000));
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{
        0, 0, 0, 0, 0, 0, 0, 0x80,
    }));
    try std.testing.expect(-0x8000000000000000 == try in_channel.read(i64));
}

test "channel" {
    var buffer = [_]u8{0} ** 256;
    var _out_stream = std.io.fixedBufferStream(&buffer);
    var _in_stream = std.io.fixedBufferStream(&buffer);
    var out_channel = initOutChannel(&_out_stream.writer());
    var in_channel = initInChannel(std.testing.allocator, &_in_stream.reader());

    // enum
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(Wall.stone);
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{@intFromEnum(Wall.stone)}));
    try std.testing.expect(Wall.stone == try in_channel.read(Wall));

    // bool
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(false);
    try out_channel.write(true);
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{ 0, 1 }));
    try std.testing.expect(false == try in_channel.read(bool));
    try std.testing.expect(true == try in_channel.read(bool));

    // struct
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(Coord{ .x = 1, .y = 2 });
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{
        1, 0, 0, 0,
        2, 0, 0, 0,
    }));
    try std.testing.expect((Coord{ .x = 1, .y = 2 }).equals(try in_channel.read(Coord)));

    // fixed-size array
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write([_]u8{ 1, 2, 3 });
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{ 1, 2, 3 }));
    try std.testing.expect(std.mem.eql(u8, &[_]u8{ 1, 2, 3 }, &(try in_channel.read([3]u8))));

    // slice
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(([_]u8{ 1, 2, 3 })[0..]);
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{
        3, 0, 0, 0, 0, 0, 0, 0,
    } ++ &[_]u8{ 1, 2, 3 }));
    {
        const slice = try in_channel.read([]u8);
        defer std.testing.allocator.free(slice);
        try std.testing.expect(std.mem.eql(u8, &[_]u8{ 1, 2, 3 }, slice));
    }

    // union(enum)
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(PerceivedActivity{ .movement = Coord{ .x = 3, .y = -4 } });
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{@intFromEnum(PerceivedActivity.movement)} ++ &[_]u8{
        3,    0,    0,    0,
        0xfc, 0xff, 0xff, 0xff,
    }));
    try std.testing.expect((Coord{ .x = 3, .y = -4 }).equals((try in_channel.read(PerceivedActivity)).movement));

    // nullable
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(@as(?u8, null));
    try out_channel.write(@as(?u8, 5));
    try std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), &[_]u8{ 0, 1, 5 }));
    try std.testing.expect(null == try in_channel.read(?u8));
    try std.testing.expect(5 == (try in_channel.read(?u8)).?);
}
