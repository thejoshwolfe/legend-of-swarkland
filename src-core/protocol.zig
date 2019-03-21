const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const Terrain = core.game_state.Terrain;
const Floor = core.game_state.Floor;
const Wall = core.game_state.Wall;

pub const Request = union(enum) {
    act: Action,
    rewind,
};

pub const Action = union(enum) {
    move: Coord,
    attack: Coord,
};

pub const Response = union(enum) {
    events: []Event,
    undo: []Event,
};

pub const Event = union(enum) {
    init_state: InitState,
    pub const InitState = struct {
        terrain: Terrain,
        player_positions: []Coord,
    };

    moved: Moved,
    pub const Moved = struct {
        player_index: usize,
        locations: []Coord,
    };

    attacked: Attacked,
    pub const Attacked = struct {
        player_index: usize,
        origin_position: Coord,
        attack_location: Coord,
    };

    died: usize,

    pub fn deinit(event: Event, allocator: *std.mem.Allocator) void {
        switch (event) {
            Event.init_state => |e| {
                allocator.free(e.player_positions);
            },
            Event.moved => |e| {
                allocator.free(e.locations);
            },
            Event.attacked, Event.died => {},
        }
    }
};

pub const Channel = struct {
    allocator: *std.mem.Allocator,
    in_file: std.os.File,
    out_file: std.os.File,
    in_adapter: std.os.File.InStream,
    out_adapter: std.os.File.OutStream,
    in_stream: *std.io.InStream(std.os.File.ReadError),
    out_stream: *std.io.OutStream(std.os.File.WriteError),

    pub fn init(self: *Channel, allocator: *std.mem.Allocator, in_file: std.os.File, out_file: std.os.File) void {
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
            @enumToInt(Response.events) => {
                return Response{ .events = try self.readEvents() };
            },
            @enumToInt(Response.undo) => {
                return Response{ .undo = try self.readEvents() };
            },
            else => return error.MalformedData,
        }
    }
    fn writeResponse(self: *Channel, response: Response) !void {
        try self.writeInt(u8(@enumToInt(@TagType(Response)(response))));
        switch (response) {
            Response.events => |events| {
                try self.writeEvents(events);
            },
            Response.undo => |events| {
                try self.writeEvents(events);
            },
        }
    }

    fn readEvents(self: *Channel) ![]Event {
        var events: []Event = try self.allocator.alloc(Event, try self.readArrayLength());
        for (events) |*event| {
            event.* = try self.readEvent();
        }
        return events;
    }
    fn writeEvents(self: *Channel, events: []Event) !void {
        try self.writeArrayLength(events.len);
        for (events) |event| {
            try self.writeEvent(event);
        }
    }

    fn readEvent(self: *Channel) !Event {
        switch (Event.moved) {
            Event.init_state => {},
            Event.moved => {},
            Event.attacked => {},
            Event.died => {},
        }
        switch (try self.readInt(u8)) {
            @enumToInt(Event.init_state) => {
                return Event{
                    .init_state = Event.InitState{
                        .terrain = try self.readTerrain(),
                        .player_positions = blk: {
                            const arr = try self.allocator.alloc(Coord, try self.readArrayLength());
                            for (arr) |*x| {
                                x.* = try self.readCoord();
                            }
                            break :blk arr;
                        },
                    },
                };
            },
            @enumToInt(Event.moved) => {
                return Event{
                    .moved = Event.Moved{
                        .player_index = try self.readInt(u8),
                        .locations = blk: {
                            const arr = try self.allocator.alloc(Coord, try self.readArrayLength());
                            for (arr) |*x| {
                                x.* = try self.readCoord();
                            }
                            break :blk arr;
                        },
                    },
                };
            },
            @enumToInt(Event.attacked) => {
                return Event{
                    .attacked = Event.Attacked{
                        .player_index = try self.readInt(u8),
                        .origin_position = try self.readCoord(),
                        .attack_location = try self.readCoord(),
                    },
                };
            },
            @enumToInt(Event.died) => {
                return Event{ .died = try self.readInt(u8) };
            },
            else => return error.MalformedData,
        }
    }
    fn writeEvent(self: *Channel, _event: Event) !void {
        try self.writeInt(u8(@enumToInt(@TagType(Event)(_event))));
        switch (_event) {
            Event.init_state => |event| {
                try self.writeTerrain(event.terrain);
                try self.writeArrayLength(event.player_positions.len);
                for (event.player_positions) |position| {
                    try self.writeCoord(position);
                }
            },
            Event.moved => |event| {
                try self.writeInt(@intCast(u8, event.player_index));
                try self.writeArrayLength(event.locations.len);
                for (event.locations) |location| {
                    try self.writeCoord(location);
                }
            },
            Event.attacked => |event| {
                try self.writeInt(@intCast(u8, event.player_index));
                try self.writeCoord(event.origin_position);
                try self.writeCoord(event.attack_location);
            },
            Event.died => |player_index| {
                try self.writeInt(@intCast(u8, player_index));
            },
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
        // core.debug.warn("readInt: {}\n", x);
        return x;
    }
    fn writeInt(self: *Channel, x: var) !void {
        // core.debug.warn("writeInt: {}\n", x);
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
