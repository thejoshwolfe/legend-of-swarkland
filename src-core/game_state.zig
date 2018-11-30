const core = @import("./index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Event = core.protocol.Event;

pub const GameState = struct {
    player_positions: [2]Coord,
    terrain: Terrain,

    pub fn init() GameState {
        return GameState{
            .player_positions = []Coord{ makeCoord(0, 0), makeCoord(0, 0) },
            .terrain = Terrain{
            // @_@
                .floor = [][16]Floor{[]Floor{Floor.unknown} ** 16} ** 16,
                .walls = [][16]Wall{[]Wall{Wall.unknown} ** 16} ** 16,
            },
        };
    }

    fn applyEvent(self: *GameState, event: Event) void {
        switch (event) {
            Event.moved => |e| {
                self.player_positions = e.to;
            },
            Event.init_state => |e| {
                self.terrain = e.terrain;
                self.player_positions = e.player_positions;
            },
        }
    }
    fn undoEvent(self: *GameState, event: Event) void {
        switch (event) {
            Event.moved => |e| {
                self.player_positions = e.from;
            },
            Event.init_state => @panic("can't undo the beginning of time"),
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
