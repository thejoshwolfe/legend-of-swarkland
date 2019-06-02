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
            .game_state = GameState.init(allocator),
            .history = HistoryList.init(),
        };
    }

    pub fn getStartGameEvents(self: *const GameEngine) ![]Event {
        var events = try self.allocator.alloc(Event, 1);
        events[0] = Event{
            .init_state = Event.InitState{
                .terrain = undefined,
                .player_positions = try std.mem.dupe(self.allocator, Coord, []Coord{
                    makeCoord(7, 14),
                    makeCoord(3, 2),
                    // makeCoord(14, 2), // TODO: not ready for a 3rd entity yet.
                }),
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

        // allocate lots of little things with this,
        // but anything returned from this function has to be allocated from the main allocator
        var _arena = std.heap.ArenaAllocator.init(self.allocator);
        defer _arena.deinit();
        const arena = &_arena.allocator;

        // read movement
        var any_movement = false;
        var current_player_positions = try arena.alloc(Coord, self.game_state.player_positions.len);
        var current_player_movement = try arena.alloc(Coord, self.game_state.player_positions.len);
        var current_player_force = try arena.alloc(u4, self.game_state.player_positions.len);
        for (self.game_state.alive_players) |i| {
            const action = actions[i];

            switch (action) {
                Action.move => |direction| {
                    current_player_positions[i] = self.game_state.player_positions[i];
                    current_player_movement[i] = direction;
                    any_movement = true;
                },
                else => {
                    // standing still
                    current_player_positions[i] = self.game_state.player_positions[i];
                    current_player_movement[i] = makeCoord(0, 0);
                },
            }
        }
        // detect and resolve collisions
        while (any_movement) {
            any_movement = false;
            // calculate the forces
            var force_field = [][16]u4{[]u4{0} ** 16} ** 16;
            // walls
            var cursor = makeCoord(0, 0);
            while (cursor.y < 16) : (cursor.y += 1) {
                cursor.x = 0;
                while (cursor.x < 16) : (cursor.x += 1) {
                    if (!self.isOpenSpace(cursor)) {
                        // walls push in all directions
                        force_field[@intCast(u4, cursor.y)][@intCast(u4, cursor.x)] = 15;
                    }
                }
            }
            // do movement
            for (self.game_state.alive_players) |i| {
                const direction = current_player_movement[i];
                if (direction.x == 0 and direction.y == 0) {
                    // standing still means pushing in all directions
                    current_player_force[i] = 15;
                } else {
                    // moving
                    const old_position = current_player_positions[i];
                    const new_position = old_position.plus(direction);
                    current_player_positions[i] = new_position;
                    current_player_force[i] = core.geometry.directionToCardinalBitmask(direction);
                }
                // contribute force
                const position = current_player_positions[i];
                force_field[@intCast(u4, position.y)][@intCast(u4, position.x)] |= current_player_force[i];
            }
            // who's getting pushed
            for (self.game_state.alive_players) |i| {
                const position = current_player_positions[i];
                const my_force = current_player_force[i];
                // look at all the forces except our own
                const ambient_force = force_field[@intCast(u4, position.y)][@intCast(u4, position.x)] & ~my_force;
                if (ambient_force != 0) {
                    // oof
                    const push_direction = core.geometry.cardinalBitmaskToDirection(ambient_force) orelse
                    // if we're walking into something stationary, or walking into a clusterfuck,
                    // just reverse course.
                        core.geometry.cardinalBitmaskToDirection(my_force).?.negated();
                    current_player_movement[i] = push_direction;
                    any_movement = true;
                } else {
                    current_player_movement[i] = makeCoord(0, 0);
                }
            }
        }

        // publish movement as events.
        // TODO: we should give a full history to be animated instead of this digest
        for (self.game_state.alive_players) |i| {
            const old_position = self.game_state.player_positions[i];
            const new_position = current_player_positions[i];
            if (!old_position.equals(new_position)) {
                try events.append(Event{
                    .moved = Event.Moved{
                        .player_index = i,
                        .locations = try std.mem.dupe(self.allocator, Coord, []Coord{ old_position, new_position }),
                    },
                });
            }
        }

        // resolve attacks
        for (self.game_state.alive_players) |i| {
            const action = actions[i];
            switch (action) {
                Action.attack => |direction| {
                    const origin_position = current_player_positions[i];
                    const attack_location = origin_position.plus(direction);
                    try events.append(Event{
                        .attacked = Event.Attacked{
                            .player_index = i,
                            .origin_position = origin_position,
                            .attack_location = attack_location,
                        },
                    });

                    for (self.game_state.alive_players) |j| {
                        if (current_player_positions[j].equals(attack_location)) {
                            try events.append(Event{ .died = j });
                        }
                    }
                },
                Action.move => |direction| {},
            }
        }
        return events.toOwnedSlice();
    }

    pub fn applyEvents(self: *GameEngine, events: []Event) !void {
        try self.recordEvents(events);
        try self.game_state.applyEvents(events);
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
        try self.game_state.undoEvents(events);
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
