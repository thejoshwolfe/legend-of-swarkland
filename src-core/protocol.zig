const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;

pub const Action = union(enum) {
    Move: Coord,
    _Unused, // TODO: workaround for https://github.com/ziglang/zig/issues/1712
};

pub const Event = union(enum) {
    Moved: MovedEvent,
    _Unused, // TODO: workaround for https://github.com/ziglang/zig/issues/1712
};

pub const MovedEvent = struct {
    from: Coord,
    to: Coord,
};

pub fn ClientChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct {
        const Self = @This();
        const Base = BaseChannel(ReadError, WriteError);

        base: Base,

        pub fn create(
            in_stream: *std.io.InStream(ReadError),
            out_stream: *std.io.OutStream(WriteError),
        ) Self {
            return Self{
                .base = Base{
                    .in_stream = in_stream,
                    .out_stream = out_stream,
                },
            };
        }

        pub fn writeAction(self: *Self, action: Action) !void {
            try self.base.writeInt(u8(@enumToInt(@TagType(Action)(action))));
            switch (action) {
                Action.Move => |vector| {
                    try self.base.writeCoord(vector);
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
                    return Event{
                        .Moved = MovedEvent{
                            .from = try self.base.readCoord(),
                            .to = try self.base.readCoord(),
                        },
                    };
                },
                Event._Unused => unreachable,
            }
        }
    };
}

pub fn ServerChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct {
        const Self = @This();
        const Base = BaseChannel(ReadError, WriteError);

        base: Base,

        pub fn create(
            in_stream: *std.io.InStream(ReadError),
            out_stream: *std.io.OutStream(WriteError),
        ) Self {
            return Self{
                .base = Base{
                    .in_stream = in_stream,
                    .out_stream = out_stream,
                },
            };
        }

        pub fn writeEvent(self: *Self, _event: Event) !void {
            try self.base.writeInt(u8(@enumToInt(@TagType(Event)(_event))));
            switch (_event) {
                Event.Moved => |event| {
                    try self.base.writeCoord(event.from);
                    try self.base.writeCoord(event.to);
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
                    return Action{ .Move = try self.base.readCoord() };
                },
                Action._Unused => unreachable,
            }
        }
    };
}

fn BaseChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct {
        const Self = @This();

        in_stream: *std.io.InStream(ReadError),
        out_stream: *std.io.OutStream(WriteError),

        pub fn readInt(self: *Self, comptime T: type) !T {
            const x = self.in_stream.readIntLe(T);
            return x;
        }
        pub fn writeInt(self: *Self, x: var) !void {
            return self.out_stream.writeIntLe(@typeOf(x), x);
        }

        pub fn readCoord(self: *Self) !Coord {
            return Coord{
                .x = try self.readInt(i32),
                .y = try self.readInt(i32),
            };
        }
        pub fn writeCoord(self: *Self, coord: Coord) !void {
            try self.writeInt(coord.x);
            try self.writeInt(coord.y);
        }
    };
}
