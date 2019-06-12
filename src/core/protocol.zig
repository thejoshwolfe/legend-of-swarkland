const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

pub const Terrain = struct {
    floor: [16][16]Floor,
    walls: [16][16]Wall,
};

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

pub const Request = union(enum) {
    act: Action,
    rewind,
};

pub const Action = union(enum) {
    move: Coord,
    attack: Coord,
};

pub const Response = union(enum) {
    stuff_happens: []PerceivedFrame,
    undo: PerceivedFrame,

    pub fn deinit(self: Response, allocator: *std.mem.Allocator) void {
        switch (self) {
            stuff_happens => |frames| {
                for (frames) |frame| {
                    frame.deinit(allocator);
                }
                allocator.free(frames);
            },
            undo => {},
        }
    }
};

pub const Species = enum {
    human,
    orc,
};

/// Represents what you can observe with your eyes happening simultaneously.
/// There can be multiple of these per turn.
pub const PerceivedFrame = struct {
    terrain: Terrain,
    individuals_by_location: []IndividualWithMotion,
    pub fn deinit(self: PerceivedFrame, allocator: *std.mem.Allocator) void {
        allocator.free(self.individuals_by_location);
    }

    // the tuple (prior velocity, abs_position) is guaranteed to be unique in this frame
    const IndividualWithMotion = struct {
        prior_velocity: Coord,
        abs_position: Coord, // TODO: when we have scrolling, change abs_position to rel_position
        species: Species,
        next_velocity: Coord,
    };
};

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
            Request.act => |action| {
                try self.writeInt(u8(@enumToInt(@TagType(Action)(action))));
                switch (action) {
                    Action.move => |direction| try self.writeCoord(direction),
                    Action.attack => |direction| try self.writeCoord(direction),
                }
            },
            Request.rewind => {},
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
        switch (Request.act) {
            Request.act => {},
            Request.rewind => {},
        }
    }

    pub fn readResponse(self: *Channel) !?Response {
        switch (self.readInt(u8) catch |err| switch (err) {
            error.EndOfStream => return null,
            else => return err,
        }) {
            @enumToInt(Response.stuff_happens) => {
                var frames = try self.allocator.alloc(PerceivedFrame, try self.readArrayLength());
                for (frames) |*frame| {
                    frame.* = try self.readPerceivedFrame();
                }
                return Response{ .stuff_happens = frames };
            },
            @enumToInt(Response.undo) => {
                return Response{ .undo = try self.readPerceivedFrame() };
            },
            else => return error.MalformedData,
        }
    }
    fn writeResponse(self: *Channel, response: Response) !void {
        try self.writeInt(u8(@enumToInt(@TagType(Response)(response))));
        switch (response) {
            Response.stuff_happens => |perception_frames| {
                try self.writeArrayLength(perception_frames.len);
                for (perception_frames) |frame| {
                    try self.writePerceivedFrame(frame);
                }
            },
            Response.undo => |_| {
                @panic("todo");
            },
        }
    }

    fn readPerceivedFrame(self: *Channel) !PerceivedFrame {
        return PerceivedFrame{
            .terrain = try self.readTerrain(),
            .individuals_by_location = blk: {
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
        try self.writeTerrain(frame.terrain);

        try self.writeArrayLength(frame.individuals_by_location.len);
        for (frame.individuals_by_location) |individual_with_motion| {
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

pub fn deinitResponse(allocator: *std.mem.Allocator, response: Response) void {
    switch (response) {
        Response.events => |events| {
            deinitEvents(allocator, events);
        },
        Response.undo => |events| {
            deinitEvents(allocator, events);
        },
    }
}
pub fn deinitEvents(allocator: *std.mem.Allocator, events: []Event) void {
    for (events) |event| {
        event.deinit(allocator);
    }
    allocator.free(events);
}