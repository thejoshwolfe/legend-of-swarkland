const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const makeCoord = core.geometry.makeCoord;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const GameState = core.game_state.GameState;
const Terrain = core.game_state.Terrain;
const Floor = core.game_state.Floor;
const Wall = core.game_state.Wall;

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,
    game_state: GameState,
    history: HistoryList,

    const HistoryList = std.LinkedList([]Event);
    const HistoryNode = HistoryList.Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = GameState.init(),
            .history = HistoryList.init(),
        };
    }

    pub fn getStartGameEvents(self: *const GameEngine) ![]Event {
        var events = try self.allocator.alloc(Event, 1);
        events[0] = Event{
            .init_state = Event.InitState{
                .terrain = undefined,
                .player_positions = []Coord{ makeCoord(3, 3), makeCoord(8, 5) },
            },
        };

        var terrain: *Terrain = &events[0].init_state.terrain;
        {
            var x: usize = 0;
            while (x < 16) : (x += 1) {
                terrain.floor[0][x] = Floor.unknown;
                terrain.floor[16 - 1][x] = Floor.unknown;
                terrain.walls[0][x] = Wall.stone;
                terrain.walls[16 - 1][x] = Wall.stone;
            }
        }
        {
            var y: usize = 1;
            while (y < 16 - 1) : (y += 1) {
                terrain.floor[y][0] = Floor.unknown;
                terrain.floor[y][16 - 1] = Floor.unknown;
                terrain.walls[y][0] = Wall.stone;
                terrain.walls[y][16 - 1] = Wall.stone;
            }
        }
        {
            var y: usize = 1;
            while (y < 16 - 1) : (y += 1) {
                var x: usize = 1;
                while (x < 16 - 1) : (x += 1) {
                    terrain.floor[y][x] = Floor.dirt;
                    terrain.walls[y][x] = Wall.air;
                }
            }
        }

        return events;
    }

    pub fn actionsToEvents(self: *const GameEngine, actions: []const Action) ![]Event {
        // all game rules are here
        var events = ArrayList(Event).init(self.allocator);

        var coord_to_individual = Multimap(Coord, usize, core.geometry.eqlCoord).init(self.allocator);
        defer coord_to_individual.deinit();

        for (self.game_state.player_positions) |position, i| {
            if (!self.game_state.player_is_alive[i]) continue;
            try coord_to_individual.put(position, i);
        }

        for (actions) |action, i| {
            if (!self.game_state.player_is_alive[i]) continue;

            switch (action) {
                Action.move => |direction| {
                    const old_position = self.game_state.player_positions[i];
                    const new_position = old_position.plus(direction);
                    if (self.isOpenSpace(new_position)) {
                        try events.append(Event{
                            .moved = Event.Moved{
                                .player_index = i,
                                .from = old_position,
                                .to = new_position,
                            },
                        });
                        // they are also here
                        try coord_to_individual.put(new_position, i);
                    }
                },
                Action.attack => |direction| {},
            }
        }
        for (actions) |action, i| {
            if (!self.game_state.player_is_alive[i]) continue;
            switch (action) {
                Action.attack => |direction| {
                    const origin_position = self.game_state.player_positions[i];
                    const attack_location = origin_position.plus(direction);
                    try events.append(Event{
                        .attacked = Event.Attacked{
                            .player_index = i,
                            .origin_position = origin_position,
                            .attack_location = attack_location,
                        },
                    });

                    var iterator = coord_to_individual.find(attack_location);
                    while (iterator.next()) |hit_i| {
                        try events.append(Event{ .died = hit_i });
                    }
                },
                Action.move => |direction| {},
            }
        }
        return events.toOwnedSlice();
    }

    pub fn applyEvents(self: *GameEngine, events: []Event) !void {
        try self.recordEvents(events);
        self.game_state.applyEvents(events);
    }

    pub fn isOpenSpace(self: *const GameEngine, coord: Coord) bool {
        if (coord.x < 0 or coord.y < 0) return false;
        if (coord.x >= 16 or coord.y >= 16) return false;
        return self.game_state.terrain.walls[@intCast(usize, coord.y)][@intCast(usize, coord.x)] == Wall.air;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            Action.move => |direction| return isCardinalDirection(direction),
            Action.attack => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn rewind(self: *GameEngine) ?[]Event {
        if (self.history.len <= 1) {
            // that's enough pal.
            return null;
        }
        const node = self.history.pop() orelse return null;
        const events = node.data;
        self.allocator.destroy(node);
        self.game_state.undoEvents(events);
        return events;
    }

    fn recordEvents(self: *GameEngine, events: []Event) !void {
        const history_node: *HistoryNode = try self.allocator.create(HistoryNode);
        history_node.data = events;
        self.history.append(history_node);
    }
};

pub const HistoryFrame = struct {
    event: []Event,
};

fn Multimap(comptime K: type, comptime V: type, comptime eql: fn (a: K, b: K) bool) type {
    // TODO: use an actual hash multimap instead of this linear search (maybe)
    return struct {
        const Self = @This();

        entries: ArrayList(KV),

        const KV = struct {
            k: K,
            v: V,
        };

        pub fn init(allocator: *std.mem.Allocator) Self {
            return Self{ .entries = ArrayList(KV).init(allocator) };
        }
        pub fn deinit(self: *Self) void {
            self.entries.deinit();
        }

        pub fn put(self: *Self, k: K, v: V) !void {
            try self.entries.append(KV{ .k = k, .v = v });
        }

        pub fn find(self: *Self, k: K) ValueIterator {
            return ValueIterator{ .self = self, .k = k, .i = 0 };
        }
        pub const ValueIterator = struct {
            self: *const Self,
            k: K,
            i: usize,

            pub fn next(self: *ValueIterator) ?V {
                while (self.i < self.self.entries.len) {
                    const kv = &self.self.entries.items[self.i];
                    self.i += 1;
                    if (eql(kv.k, self.k)) return kv.v;
                }
                return null;
            }
        };
    };
}
