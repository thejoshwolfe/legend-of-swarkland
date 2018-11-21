const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;

pub const Request = union(enum) {
    act: Action,
    rewind,
};

pub const Action = union(enum) {
    move: Coord,
};

pub const Response = union(enum) {
    event: Event,
    undo: Event,
};

pub const Event = union(enum) {
    moved: Moved,
    const Moved = struct {
        from: Coord,
        to: Coord,
    };
};

pub fn BaseChannel(comptime ReadError: type, comptime WriteError: type) type {
    return struct {
        const Self = @This();

        in_stream: *std.io.InStream(ReadError),
        out_stream: *std.io.OutStream(WriteError),

        pub fn create(
            in_stream: *std.io.InStream(ReadError),
            out_stream: *std.io.OutStream(WriteError),
        ) Self {
            return Self{
                .in_stream = in_stream,
                .out_stream = out_stream,
            };
        }

        pub fn writeRequest(self: *Self, request: Request) !void {
            try self.writeInt(u8(@enumToInt(@TagType(Request)(request))));
            switch (request) {
                Request.act => |action| {
                    try self.writeInt(u8(@enumToInt(@TagType(Action)(action))));
                    switch (action) {
                        Action.move => |vector| {
                            try self.writeCoord(vector);
                        },
                    }
                },
                Request.rewind => {},
            }
        }

        pub fn readRequest(self: *Self) !Request {
            switch (try self.readInt(u8)) {
                @enumToInt(Request.act) => {
                    switch (try self.readInt(u8)) {
                        @enumToInt(Action.move) => {
                            return Request{ .act = Action{ .move = try self.readCoord() } };
                        },
                        else => return error.MalformedData,
                    }
                    switch (Action.move) {
                        Action.Move => {},
                    }
                },
                @enumToInt(Request.rewind) => return Request{ .rewind = {} },
                else => return error.MalformedData,
            }
            switch (Request.act) {
                Request.act => {},
                Request.rewind => {},
            }
        }

        pub fn readResponse(self: *Self) !Response {
            switch (try self.readInt(u8)) {
                @enumToInt(Response.event) => {
                    return Response{ .event = try self.readEvent() };
                },
                @enumToInt(Response.undo) => {
                    return Response{ .undo = try self.readEvent() };
                },
                else => return error.MalformedData,
            }
            switch (Event.moved) {
                Response.event => {},
                Response.undo => {},
            }
        }

        fn readEvent(self: *Self) !Event {
            switch (try self.readInt(u8)) {
                @enumToInt(Event.moved) => {
                    return Event{
                        .moved = Event.Moved{
                            .from = try self.readCoord(),
                            .to = try self.readCoord(),
                        },
                    };
                },
                else => return error.MalformedData,
            }
            switch (Event.moved) {
                Event.moved => {},
            }
        }

        fn writeResponse(self: *Self, response: Response) !void {
            try self.writeInt(u8(@enumToInt(@TagType(Response)(response))));
            switch (response) {
                Response.event => |event| {
                    try self.writeEvent(event);
                },
                Response.undo => |event| {
                    try self.writeEvent(event);
                },
            }
        }
        fn writeEvent(self: *Self, _event: Event) !void {
            try self.writeInt(u8(@enumToInt(@TagType(Event)(_event))));
            switch (_event) {
                Event.moved => |event| {
                    try self.writeCoord(event.from);
                    try self.writeCoord(event.to);
                },
            }
        }

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
