const std = @import("std");
const core = @import("./index.zig");
const Coord = core.geometry.Coord;
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
    history: std.LinkedList(Event),

    const HistoryNode = std.LinkedList(Event).Node;

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
            .game_state = GameState.init(),
            .history = std.LinkedList(Event).init(),
        };
    }

    pub fn startGame(self: *GameEngine) Event {
        var terrain: Terrain = undefined;
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
        const event = Event{
            .init_state = Event.InitState{
                .terrain = terrain,
                .player_positions = []Coord{ makeCoord(3, 3), makeCoord(9, 5) },
            },
        };
        self.game_state.applyEvent(event);
        return event;
    }

    pub fn takeAction(self: *GameEngine, action: Action) !?Event {
        const event = self.validateAction(action) orelse return null;
        try self.recordEvent(event);
        self.game_state.applyEvent(event);
        return event;
    }

    pub fn validateAction(self: *const GameEngine, action: Action) ?Event {
        switch (action) {
            Action.move => |direction| {
                if (direction.x * direction.y != 0) return null;
                if ((direction.x + direction.y) * (direction.x + direction.y) != 1) return null;
                const old_positions = self.game_state.player_positions;
                const new_positions = []Coord{
                    old_positions[0].plus(direction),
                    old_positions[1].plus(direction),
                };
                for (new_positions) |new_position| {
                    if (new_position.x < 0 or new_position.y < 0) return null;
                    if (new_position.x >= 16 or new_position.y >= 16) return null;
                    if (self.game_state.terrain.walls[@intCast(usize, new_position.y)][@intCast(usize, new_position.x)] != Wall.air) return null;
                }
                return Event{
                    .moved = Event.Moved{
                        .from = old_positions,
                        .to = new_positions,
                    },
                };
            },
        }
    }

    pub fn rewind(self: *GameEngine) ?Event {
        const node = self.history.pop() orelse return null;
        const event = node.data;
        self.allocator.destroy(node);
        self.game_state.undoEvent(event);
        return event;
    }

    fn recordEvent(self: *GameEngine, event: Event) !void {
        const history_node: *HistoryNode = try self.allocator.createOne(HistoryNode);
        history_node.data = event;
        self.history.append(history_node);
    }
};

pub const HistoryFrame = struct {
    event: core.protocol.Event,
};
