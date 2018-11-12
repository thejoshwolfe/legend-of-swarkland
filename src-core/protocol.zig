const std = @import("std");
const debug = @import("./debug.zig");

pub const Action = union(enum).{
    Move: i8,
    _Unused, // TODO: workaround for https://github.com/ziglang/zig/issues/1712
};

pub const Event = union(enum).{
    Moved: i32,
    _Unused, // TODO: workaround for https://github.com/ziglang/zig/issues/1712
};

pub fn ClientChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct.{
        const Self = @This();
        const Base = BaseChannel(ReadError, WriteError);

        base: Base,

        pub fn create(
            in_stream: *std.io.InStream(ReadError),
            out_stream: *std.io.OutStream(WriteError),
        ) Self {
            return Self.{
                .base = Base.{
                    .in_stream = in_stream,
                    .out_stream = out_stream,
                },
            };
        }

        pub fn writeAction(self: *Self, action: Action) !void {
            try self.base.writeInt(u8(@enumToInt(@TagType(Action)(action))));
            switch (action) {
                Action.Move => |direction| {
                    try self.base.writeInt(direction);
                },
                Action._Unused => unreachable,
            }
        }
        pub fn readEvent(self: *Self) !Event {
            const tag_int = try self.base.readInt(u8);
            if (tag_int >= @memberCount(Event)) {
                @panic("malformed data"); // TODO
            }
            switch (@intToEnum(@TagType(Event), @intCast(@TagType(@TagType(Event)), tag_int))) {
                Event.Moved => {
                    return Event.{ .Moved = try self.base.readInt(i32) };
                },
                Event._Unused => unreachable,
            }
        }
    };
}

pub fn ServerChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct.{
        const Self = @This();
        const Base = BaseChannel(ReadError, WriteError);

        base: Base,

        pub fn create(
            in_stream: *std.io.InStream(ReadError),
            out_stream: *std.io.OutStream(WriteError),
        ) Self {
            return Self.{
                .base = Base.{
                    .in_stream = in_stream,
                    .out_stream = out_stream,
                },
            };
        }

        pub fn writeEvent(self: *Self, event: Event) !void {
            try self.base.writeInt(u8(@enumToInt(@TagType(Event)(event))));
            switch (event) {
                Event.Moved => |position| {
                    try self.base.writeInt(position);
                },
                Event._Unused => unreachable,
            }
        }
        pub fn readAction(self: *Self) !Action {
            const tag_int = try self.base.readInt(u8);
            if (tag_int >= @memberCount(Action)) {
                @panic("malformed data"); // TODO
            }
            switch (@intToEnum(@TagType(Action), @intCast(@TagType(@TagType(Action)), tag_int))) {
                Action.Move => {
                    return Action.{ .Move = try self.base.readInt(i8) };
                },
                Action._Unused => unreachable,
            }
        }
    };
}

fn BaseChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct.{
        const Self = @This();

        in_stream: *std.io.InStream(ReadError),
        out_stream: *std.io.OutStream(WriteError),

        pub fn readInt(self: *Self, comptime T: type) !T {
            const x = self.in_stream.readIntLe(T);
            //debug.warn("read: {}\n", x);
            return x;
        }
        pub fn writeInt(self: *Self, x: var) !void {
            //debug.warn("write: {}\n", x);
            return self.out_stream.writeIntLe(@typeOf(x), x);
        }
    };
}

