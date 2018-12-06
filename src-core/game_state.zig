const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Event = core.protocol.Event;

pub const GameState = struct {
    player_positions: [2]Coord,
    player_is_alive: [2]bool,
    terrain: Terrain,

    pub fn init() GameState {
        return GameState{
            .player_positions = []Coord{ makeCoord(0, 0), makeCoord(0, 0) },
            .player_is_alive = []bool{ true, true },
            .terrain = Terrain{
            // @_@
                .floor = [][16]Floor{[]Floor{Floor.unknown} ** 16} ** 16,
                .walls = [][16]Wall{[]Wall{Wall.unknown} ** 16} ** 16,
            },
        };
    }

    fn applyEvents(self: *GameState, events: []const Event) void {
        for (events) |event| {
            switch (event) {
                Event.init_state => |e| {
                    self.terrain = e.terrain;
                    self.player_positions = e.player_positions;
                },
                Event.moved => |e| {
                    self.player_positions[e.player_index] = e.to;
                },
                Event.attacked => {},
                Event.died => |player_index| {
                    self.player_is_alive[player_index] = false;
                },
            }
        }
    }
    fn undoEvents(self: *GameState, events: []const Event) void {
        for (events) |event| {
            switch (event) {
                Event.moved => |e| {
                    self.player_positions[e.player_index] = e.from;
                },
                Event.attacked => {},
                Event.died => |player_index| {
                    self.player_is_alive[player_index] = true;
                },
                Event.init_state => @panic("can't undo the beginning of time"),
            }
        }
    }
};

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
