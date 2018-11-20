const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;

pub const Request = union(enum) {
    Act: Action,
    Rewind,
};

pub const Action = union(enum) {
    Move: Coord,
};

pub const Response = union(enum) {
    Event: Event,
    Undo: Event,
};

pub const Event = union(enum) {
    Moved: MovedEvent,
};

pub const MovedEvent = struct {
    from: Coord,
    to: Coord,
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
                Request.Act => |action| {
                    try self.writeInt(u8(@enumToInt(@TagType(Action)(action))));
                    switch (action) {
                        Action.Move => |vector| {
                            try self.writeCoord(vector);
                        },
                    }
                },
                Request.Rewind => {},
            }
        }

        pub fn readRequest(self: *Self) !Request {
            switch (try self.readInt(u8)) {
                @enumToInt(Request.Act) => {
                    switch (try self.readInt(u8)) {
                        @enumToInt(Action.Move) => {
                            return Request{ .Act = Action{ .Move = try self.readCoord() } };
                        },
                        else => return error.MalformedData,
                    }
                    switch (Action.Move) {
                        Action.Move => {},
                    }
                },
                @enumToInt(Request.Rewind) => return Request{ .Rewind = {} },
                else => return error.MalformedData,
            }
            switch (Request.Act) {
                Request.Act => {},
                Request.Rewind => {},
            }
        }

        pub fn readResponse(self: *Self) !Response {
            switch (try self.readInt(u8)) {
                @enumToInt(Response.Event) => {
                    return Response{ .Event = try self.readEvent() };
                },
                @enumToInt(Response.Undo) => {
                    return Response{ .Undo = try self.readEvent() };
                },
                else => return error.MalformedData,
            }
            switch (Event.Moved) {
                Response.Event => {},
                Response.Undo => {},
            }
        }

        fn readEvent(self: *Self) !Event {
            switch (try self.readInt(u8)) {
                @enumToInt(Event.Moved) => {
                    return Event{
                        .Moved = MovedEvent{
                            .from = try self.readCoord(),
                            .to = try self.readCoord(),
                        },
                    };
                },
                else => return error.MalformedData,
            }
            switch (Event.Moved) {
                Event.Moved => {},
            }
        }

        fn writeResponse(self: *Self, response: Response) !void {
            try self.writeInt(u8(@enumToInt(@TagType(Response)(response))));
            switch (response) {
                Response.Event => |event| {
                    try self.writeEvent(event);
                },
                Response.Undo => |event| {
                    try self.writeEvent(event);
                },
            }
        }
        fn writeEvent(self: *Self, _event: Event) !void {
            try self.writeInt(u8(@enumToInt(@TagType(Event)(_event))));
            switch (_event) {
                Event.Moved => |event| {
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
