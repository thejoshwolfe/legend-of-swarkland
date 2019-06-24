const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

pub const Floor = enum {
    unknown,
    dirt,
    marble,
    lava,
};

pub const Wall = enum {
    unknown,
    air,
    dirt,
    stone,
};

pub const Species = enum {
    human,
    orc,
    ogre,
    snake,
    ant,
};

pub const Terrain = struct {
    floor: [16][16]Floor,
    walls: [16][16]Wall,
};

pub const Request = union(enum) {
    act: Action,
    rewind,
    quit,
};

pub const Action = union(enum) {
    move: Coord,
    attack: Coord,
};

pub const Response = union(enum) {
    game_over,
    static_perception: StaticPerception,
    stuff_happens: []PerceivedFrame,
    undo: StaticPerception,
};

pub const StaticPerception = struct {
    terrain: Terrain,
    self: StaticIndividual,
    others: []StaticIndividual,
    pub const StaticIndividual = struct {
        abs_position: Coord, // TODO: when we have scrolling, change abs_position to rel_position
        species: Species,
    };
};

/// Represents what you can observe with your eyes happening simultaneously.
/// There can be multiple of these per turn.
pub const PerceivedFrame = struct {
    perceived_movements: []IndividualWithMotion,

    pub const IndividualWithMotion = struct {
        prior_velocity: Coord,
        abs_position: Coord, // TODO: when we have scrolling, change abs_position to rel_position
        species: Species,
        next_velocity: Coord,
    };
};

/// Despite all the generic elegance of the Channel classes,
/// this is what we use everywhere.
pub const Socket = struct {
    in_stream: std.fs.File.InStream,
    out_stream: std.fs.File.OutStream,

    pub fn init(
        in_stream: std.fs.File.InStream,
        out_stream: std.fs.File.OutStream,
    ) Socket {
        return Socket{
            .in_stream = in_stream,
            .out_stream = out_stream,
        };
    }

    pub const FileInChannel = InChannel(std.fs.File.InStream.Stream);
    pub fn in(self: *Socket, allocator: *std.mem.Allocator) FileInChannel {
        return initInChannel(allocator, &self.in_stream.stream);
    }

    pub const FileOutChannel = OutChannel(std.fs.File.OutStream.Stream);
    pub fn out(self: *Socket) FileOutChannel {
        return initOutChannel(&self.out_stream.stream);
    }

    pub fn close(self: *Socket, final_message: var) void {
        self.out().write(final_message) catch {};
        self.out_stream.file.close();
        // FIXME: revisit closing the in_stream when we have async/await maybe.
    }
};

pub fn initOutChannel(out_stream: var) OutChannel(@typeOf(out_stream.*)) {
    return OutChannel(@typeOf(out_stream.*)).init(out_stream);
}
pub fn OutChannel(comptime OutStream: type) type {
    return struct {
        const Self = @This();

        stream: *OutStream,
        pub fn init(stream: *OutStream) Self {
            return Self{ .stream = stream };
        }

        pub fn write(self: Self, x: var) !void {
            const T = @typeOf(x);
            switch (@typeInfo(T)) {
                .Int => return self.writeInt(x),
                .Bool => return self.writeInt(@boolToInt(x)),
                .Enum => return self.writeInt(@enumToInt(x)),
                .Struct => |info| {
                    inline for (info.fields) |field| {
                        try self.write(@field(x, field.name));
                    }
                },
                .Array => {
                    for (x) |x_i| {
                        try self.write(x_i);
                    }
                },
                .Pointer => |info| {
                    switch (info.size) {
                        .Slice => {
                            try self.writeInt(x.len);
                            for (x) |x_i| {
                                try self.write(x_i);
                            }
                        },
                        else => @compileError("not supported: " ++ @typeName(T)),
                    }
                },
                .Union => |info| {
                    const tag_value = @enumToInt(info.tag_type.?(x));
                    try self.writeInt(tag_value);
                    inline for (info.fields) |u_field| {
                        if (tag_value == u_field.enum_field.?.value) {
                            // FIXME: this `if` is because inferred error sets require at least one error.
                            return if (u_field.field_type != void) {
                                try self.write(@field(x, u_field.name));
                            };
                        }
                    }
                    unreachable;
                },
                else => @compileError("not supported: " ++ @typeName(T)),
            }
        }

        pub fn writeInt(self: Self, x: var) !void {
            const int_info = @typeInfo(@typeOf(x)).Int;
            const T_aligned = @IntType(int_info.is_signed, @divTrunc(int_info.bits + 7, 8) * 8);
            try self.stream.writeIntLittle(T_aligned, x);
        }
    };
}

pub fn initInChannel(allocator: *std.mem.Allocator, in_stream: var) InChannel(@typeOf(in_stream.*)) {
    return InChannel(@typeOf(in_stream.*)).init(allocator, in_stream);
}
pub fn InChannel(comptime InStream: type) type {
    return struct {
        const Self = @This();

        allocator: *std.mem.Allocator,
        stream: *InStream,
        pub fn init(allocator: *std.mem.Allocator, stream: *InStream) Self {
            return Self{
                .allocator = allocator,
                .stream = stream,
            };
        }

        pub fn read(self: Self, comptime T: type) !T {
            switch (@typeInfo(T)) {
                .Int => return self.readInt(T),
                .Bool => return 0 != try self.readInt(u1),
                .Enum => return @intToEnum(T, try self.readInt(@TagType(T))),
                .Struct => |info| {
                    var x: T = undefined;
                    inline for (info.fields) |field| {
                        @field(x, field.name) = try self.read(field.field_type);
                    }
                    return x;
                },
                .Array => {
                    var x: T = undefined;
                    for (x) |*x_i| {
                        x_i.* = try self.read(@typeOf(x_i.*));
                    }
                    return x;
                },
                .Pointer => |info| {
                    switch (info.size) {
                        .Slice => {
                            const len = try self.readInt(usize);
                            var x = try self.allocator.alloc(info.child, len);
                            for (x) |*x_i| {
                                x_i.* = try self.read(info.child);
                            }
                            return x;
                        },
                        else => @compileError("not supported: " ++ @typeName(T)),
                    }
                },
                .Union => |info| {
                    const tag_value = @enumToInt(try self.read(info.tag_type.?));
                    inline for (info.fields) |u_field| {
                        if (tag_value == u_field.enum_field.?.value) {
                            // FIXME: this `if` is because inferred error sets require at least one error.
                            return workaround1315(T, u_field.enum_field.?.value, if (u_field.field_type == void) {} else try self.read(u_field.field_type));
                        }
                    }
                    unreachable;
                },
                else => @compileError("not supported: " ++ @typeName(T)),
            }
        }

        pub fn readInt(self: Self, comptime T: type) !T {
            const int_info = @typeInfo(T).Int;
            const T_aligned = @IntType(int_info.is_signed, @divTrunc(int_info.bits + 7, 8) * 8);
            const x_aligned = try self.stream.readIntLittle(T_aligned);
            return std.math.cast(T, x_aligned);
        }
    };
}

/// FIXME: https://github.com/ziglang/zig/issues/1315
fn workaround1315(comptime UnionType: type, comptime tag_value: comptime_int, value: var) UnionType {
    if (UnionType == Action) {
        if (tag_value == @enumToInt(Action.move)) return Action{ .move = value };
        if (tag_value == @enumToInt(Action.attack)) return Action{ .attack = value };
    }
    if (UnionType == Request) {
        if (tag_value == @enumToInt(Request.act)) return Request{ .act = value };
        if (tag_value == @enumToInt(Request.rewind)) return Request{ .rewind = value };
        if (tag_value == @enumToInt(Request.quit)) return Request{ .quit = value };
    }
    if (UnionType == Response) {
        if (tag_value == @enumToInt(Response.game_over)) return Response{ .game_over = value };
        if (tag_value == @enumToInt(Response.static_perception)) return Response{ .static_perception = value };
        if (tag_value == @enumToInt(Response.stuff_happens)) return Response{ .stuff_happens = value };
        if (tag_value == @enumToInt(Response.undo)) return Response{ .undo = value };
    }
    @compileError("add support for: " ++ @typeName(UnionType));
}

test "channel int" {
    var buffer = [_]u8{0} ** 256;
    var _out_stream = std.io.SliceOutStream.init(buffer[0..]);
    var _in_stream = std.io.SliceInStream.init(buffer[0..]);
    var out_channel = initOutChannel(&_out_stream.stream);
    var in_channel = initInChannel(std.debug.global_allocator, &_in_stream.stream);

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(u0(0)); // zero size
    try out_channel.writeInt(u3(2));
    try out_channel.writeInt(i3(-2));
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{ 2, 0xfe }));
    std.testing.expect(0 == try in_channel.read(u0));
    std.testing.expect(2 == try in_channel.read(u3));
    std.testing.expect(-2 == try in_channel.read(i3));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(u64(3));
    try out_channel.writeInt(i64(4));
    try out_channel.writeInt(i64(-5));
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{
        3,    0,    0,    0,    0,    0,    0,    0,
        4,    0,    0,    0,    0,    0,    0,    0,
        0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    }));
    std.testing.expect(3 == try in_channel.read(u64));
    std.testing.expect(4 == try in_channel.read(i64));
    std.testing.expect(-5 == try in_channel.read(i64));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(u64(0xffffffffffffffff));
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    }));
    std.testing.expect(0xffffffffffffffff == try in_channel.read(u64));

    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.writeInt(i64(-0x8000000000000000));
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{
        0, 0, 0, 0, 0, 0, 0, 0x80,
    }));
    std.testing.expect(-0x8000000000000000 == try in_channel.read(i64));
}

test "channel" {
    var buffer = [_]u8{0} ** 256;
    var _out_stream = std.io.SliceOutStream.init(buffer[0..]);
    var _in_stream = std.io.SliceInStream.init(buffer[0..]);
    var out_channel = initOutChannel(&_out_stream.stream);
    var in_channel = initInChannel(std.debug.global_allocator, &_in_stream.stream);

    // enum
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(Wall.stone);
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{@enumToInt(Wall.stone)}));
    std.testing.expect(Wall.stone == try in_channel.read(Wall));

    // bool
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(false);
    try out_channel.write(true);
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{ 0, 1 }));
    std.testing.expect(false == try in_channel.read(bool));
    std.testing.expect(true == try in_channel.read(bool));

    // struct
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(Coord{ .x = 1, .y = 2 });
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{
        1, 0, 0, 0,
        2, 0, 0, 0,
    }));
    std.testing.expect((Coord{ .x = 1, .y = 2 }).equals(try in_channel.read(Coord)));

    // fixed-size array
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write([_]u8{ 1, 2, 3 });
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{ 1, 2, 3 }));
    std.testing.expect(std.mem.eql(u8, [_]u8{ 1, 2, 3 }, try in_channel.read([3]u8)));

    // slice
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(([_]u8{ 1, 2, 3 })[0..]);
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{
        3, 0, 0, 0, 0, 0, 0, 0,
    } ++ [_]u8{ 1, 2, 3 }));
    std.testing.expect(std.mem.eql(u8, [_]u8{ 1, 2, 3 }, try in_channel.read([]u8)));

    // union(enum)
    _out_stream.reset();
    _in_stream.pos = 0;
    try out_channel.write(Action{ .move = Coord{ .x = 3, .y = -4 } });
    std.testing.expect(std.mem.eql(u8, _out_stream.getWritten(), [_]u8{@enumToInt(Action.move)} ++ [_]u8{
        3,    0,    0,    0,
        0xfc, 0xff, 0xff, 0xff,
    }));
    std.testing.expect((Coord{ .x = 3, .y = -4 }).equals((try in_channel.read(Action)).move));
}
