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
};

pub const Action = union(enum) {
    move: Coord,
    attack: Coord,
};

pub const Response = union(enum) {
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

/// TODO: sort all arrays to hide iteration order from the server
pub const Channel = struct {
    allocator: *std.mem.Allocator,
    in_file: std.fs.File,
    out_file: std.fs.File,
    in_adapter: std.fs.File.InStream,
    out_adapter: std.fs.File.OutStream,
    in_stream: *std.io.InStream(std.fs.File.ReadError),
    out_stream: *std.io.OutStream(std.fs.File.WriteError),

    pub fn init(self: *Channel, allocator: *std.mem.Allocator, in_file: std.fs.File, out_file: std.fs.File) void {
        self.allocator = allocator;
        self.in_file = in_file;
        self.out_file = out_file;
        self.in_adapter = in_file.inStream();
        self.out_adapter = out_file.outStream();
        self.in_stream = &self.in_adapter.stream;
        self.out_stream = &self.out_adapter.stream;
    }
    pub fn close(self: *Channel) void {
        self.out_file.close();
    }

    pub fn writeRequest(self: *Channel, request: Request) !void {
        try self.writeInt(u8(@enumToInt(@TagType(Request)(request))));
        switch (request) {
            .act => |action| {
                try self.writeInt(u8(@enumToInt(@TagType(Action)(action))));
                switch (action) {
                    Action.move => |direction| try self.writeCoord(direction),
                    Action.attack => |direction| try self.writeCoord(direction),
                }
            },
            .rewind => {},
        }
    }

    pub fn readRequest(self: *Channel) !?Request {
        switch (self.readInt(u8) catch |err| switch (err) {
            error.EndOfStream => return null,
            else => return err,
        }) {
            @enumToInt(Request.act) => {
                switch (Action.move) {
                    Action.move => {},
                    Action.attack => {},
                }
                switch (try self.readInt(u8)) {
                    @enumToInt(Action.move) => {
                        return Request{ .act = Action{ .move = try self.readCoord() } };
                    },
                    @enumToInt(Action.attack) => {
                        return Request{ .act = Action{ .attack = try self.readCoord() } };
                    },
                    else => return error.MalformedData,
                }
            },
            @enumToInt(Request.rewind) => return Request{ .rewind = {} },
            else => return error.MalformedData,
        }
    }

    pub fn readResponse(self: *Channel) !?Response {
        switch (self.readInt(u8) catch |err| switch (err) {
            error.EndOfStream => return null,
            else => return err,
        }) {
            @enumToInt(Response.static_perception) => {
                return Response{ .static_perception = try self.readStaticPerception() };
            },
            @enumToInt(Response.stuff_happens) => {
                var frames = try self.allocator.alloc(PerceivedFrame, try self.readArrayLength());
                for (frames) |*frame| {
                    frame.* = try self.readPerceivedFrame();
                }
                return Response{ .stuff_happens = frames };
            },
            @enumToInt(Response.undo) => {
                return Response{ .undo = try self.readStaticPerception() };
            },
            else => return error.MalformedData,
        }
    }
    fn writeResponse(self: *Channel, response: Response) !void {
        try self.writeInt(u8(@enumToInt(@TagType(Response)(response))));
        switch (response) {
            .static_perception => |static_perception| {
                try self.writeStaticPerception(static_perception);
            },
            .stuff_happens => |perception_frames| {
                try self.writeArrayLength(perception_frames.len);
                for (perception_frames) |frame| {
                    try self.writePerceivedFrame(frame);
                }
            },
            .undo => |static_perception| {
                try self.writeStaticPerception(static_perception);
            },
        }
    }

    fn readStaticPerception(self: *Channel) !StaticPerception {
        return StaticPerception{
            .terrain = try self.readTerrain(),
            .self = try self.readStaticIndividual(),
            .others = blk: {
                var arr = try self.allocator.alloc(StaticPerception.StaticIndividual, try self.readArrayLength());
                for (arr) |*x| {
                    x.* = try self.readStaticIndividual();
                }
                break :blk arr;
            },
        };
    }
    fn writeStaticPerception(self: *Channel, static_perception: StaticPerception) !void {
        try self.writeTerrain(static_perception.terrain);
        try self.writeStaticIndividual(static_perception.self);

        try self.writeArrayLength(static_perception.others.len);
        for (static_perception.others) |static_individual| {
            try self.writeStaticIndividual(static_individual);
        }
    }

    fn readStaticIndividual(self: *Channel) !StaticPerception.StaticIndividual {
        return StaticPerception.StaticIndividual{
            .abs_position = try self.readCoord(),
            .species = try self.readEnum(Species),
        };
    }
    fn writeStaticIndividual(self: *Channel, static_individual: StaticPerception.StaticIndividual) !void {
        try self.writeCoord(static_individual.abs_position);
        try self.writeEnum(static_individual.species);
    }

    fn readPerceivedFrame(self: *Channel) !PerceivedFrame {
        return PerceivedFrame{
            .perceived_movements = blk: {
                const arr = try self.allocator.alloc(PerceivedFrame.IndividualWithMotion, try self.readArrayLength());
                for (arr) |*x| {
                    x.* = PerceivedFrame.IndividualWithMotion{
                        .prior_velocity = try self.readCoord(),
                        .abs_position = try self.readCoord(),
                        .species = try self.readEnum(Species),
                        .next_velocity = try self.readCoord(),
                    };
                }
                break :blk arr;
            },
        };
    }
    fn writePerceivedFrame(self: *Channel, frame: PerceivedFrame) !void {
        try self.writeArrayLength(frame.perceived_movements.len);
        for (frame.perceived_movements) |individual_with_motion| {
            try self.writeCoord(individual_with_motion.prior_velocity);
            try self.writeCoord(individual_with_motion.abs_position);
            try self.writeEnum(individual_with_motion.species);
            try self.writeCoord(individual_with_motion.next_velocity);
        }
    }

    fn readTerrain(self: *Channel) !Terrain {
        var result: Terrain = undefined;
        for (result.floor) |*row| {
            for (row) |*cell| {
                cell.* = try self.readEnum(Floor);
            }
        }
        for (result.walls) |*row| {
            for (row) |*cell| {
                cell.* = try self.readEnum(Wall);
            }
        }
        return result;
    }
    fn writeTerrain(self: *Channel, terrain: Terrain) !void {
        for (terrain.floor) |row| {
            for (row) |cell| {
                try self.writeEnum(cell);
            }
        }
        for (terrain.walls) |row| {
            for (row) |cell| {
                try self.writeEnum(cell);
            }
        }
    }

    fn readArrayLength(self: *Channel) !usize {
        return usize(try self.readInt(u8));
    }
    fn writeArrayLength(self: *Channel, len: usize) !void {
        try self.writeInt(@intCast(u8, len));
    }

    fn readEnum(self: *Channel, comptime T: type) !T {
        const x = try self.readInt(u8);
        if (x >= @memberCount(T)) return error.MalformedData;
        return @intToEnum(T, @intCast(@TagType(T), x));
    }
    fn writeEnum(self: *Channel, x: var) !void {
        try self.writeInt(u8(@enumToInt(x)));
    }

    fn readInt(self: *Channel, comptime T: type) !T {
        const x = try self.in_stream.readIntLittle(T);
        // core.debug.warn("readInt: {}", x);
        return x;
    }

    /// TODO: switch to varint so we don't have to downcast to u8 unsafely
    fn writeInt(self: *Channel, x: var) !void {
        // core.debug.warn("writeInt: {}", x);
        return self.out_stream.writeIntLittle(@typeOf(x), x);
    }

    fn readCoord(self: *Channel) !Coord {
        return Coord{
            .x = try self.readInt(i32),
            .y = try self.readInt(i32),
        };
    }
    fn writeCoord(self: *Channel, coord: Coord) !void {
        try self.writeInt(coord.x);
        try self.writeInt(coord.y);
    }
};

pub fn initOutChannel(out_stream: var) OutChannel(@typeOf(out_stream.*)) {
    return OutChannel(@typeOf(out_stream.*)).init(out_stream);
}
pub fn OutChannel(comptime OutStream: type) type {
    return struct {
        const Self = @This();
        const Error = @typeInfo(@typeInfo(@typeOf(OutStream(undefined).writeFn)).Fn.return_type.?).ErrorUnion.error_set;

        stream: *OutStream,
        pub fn init(stream: *OutStream) Self {
            return Self{ .stream = stream };
        }

        pub fn write(self: Self, x: var) Error!void {
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
                            try self.write(@field(x, u_field.name));
                        }
                    }
                },
                else => @compileError("not supported: " ++ @typeName(T)),
            }
        }

        pub fn writeInt(self: Self, x: var) Error!void {
            const T = @typeOf(x);
            const int_info = @typeInfo(T).Int;
            if (int_info.bits == 0) {
                // nope
                return;
            } else if (int_info.bits <= 8) {
                // small enough to fit into a single byte
                const T8 = @IntType(int_info.is_signed, 8);
                return self.stream.writeIntLittle(T8, x);
            }
            // variable length little-endian encoding

            const terminal_value = if (x < 0) T(-1) else T(0);
            var remaining = x;
            while (true) {
                var byte = @intCast(u8, remaining & 0x7f);
                remaining >>= 7;

                const done = remaining == terminal_value;
                if (!done) {
                    byte |= 0x80;
                }
                try self.stream.writeIntLittle(u8, byte);
                if (done) return;
            }
        }
    };
}

test "OutChannel.writeInt" {
    var buffer = [_]u8{0} ** 256;
    var _stream = std.io.SliceOutStream.init(buffer[0..]);
    var channel = initOutChannel(&_stream.stream);

    _stream.reset();
    try channel.writeInt(u0(0)); // zero size
    try channel.writeInt(u3(2));
    try channel.writeInt(i3(-2));
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 2, 0xfe }));

    _stream.reset();
    try channel.writeInt(u64(3));
    try channel.writeInt(i64(4));
    try channel.writeInt(i64(-5));
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 3, 4, 0x7b }));

    _stream.reset();
    try channel.writeInt(u64(0xffffffffffffffff));
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{0xff} ** 9 ++ [_]u8{0x01}));

    _stream.reset();
    try channel.writeInt(i64(-0x8000000000000000));
    // this might be wrong
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{0x80} ** 8 ++ [_]u8{0x00}));
}

test "OutChannel.write" {
    var buffer = [_]u8{0} ** 256;
    var _stream = std.io.SliceOutStream.init(buffer[0..]);
    var channel = initOutChannel(&_stream.stream);

    // enum
    _stream.reset();
    try channel.write(Wall.stone);
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{u7(@enumToInt(Wall.stone))}));

    // bool
    _stream.reset();
    try channel.write(false);
    try channel.write(true);
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 0, 1 }));

    // struct
    _stream.reset();
    try channel.write(Coord{ .x = 1, .y = 2 });
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 1, 2 }));

    // fixed-size array
    _stream.reset();
    try channel.write([_]u8{ 1, 2, 3 });
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 1, 2, 3 }));

    // slice
    _stream.reset();
    try channel.write(([_]u8{ 1, 2, 3 })[0..]);
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ 3, 1, 2, 3 }));

    // union(enum)
    _stream.reset();
    try channel.write(Action{ .move = Coord{ .x = 3, .y = 4 } });
    std.testing.expect(std.mem.eql(u8, _stream.getWritten(), [_]u8{ u7(@enumToInt(Action.move)), 3, 4 }));
}
