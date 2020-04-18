const std = @import("std");
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const ThingPosition = core.protocol.ThingPosition;
const TerrainSpace = core.protocol.TerrainSpace;
const StatusConditions = core.protocol.StatusConditions;

const map_gen = @import("./map_gen.zig");
const generateLevels = map_gen.generateLevels;

/// Shallow copies the argument to a newly allocated pointer.
fn allocClone(allocator: *std.mem.Allocator, obj: var) !*@TypeOf(obj) {
    var x = try allocator.create(@TypeOf(obj));
    x.* = obj;
    return x;
}

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return HashMap(u32, V, core.geometry.hashU32, std.hash_map.getTrivialEqlFn(u32));
}

pub fn CoordMap(comptime V: type) type {
    return HashMap(Coord, V, Coord.hash, Coord.equals);
}

pub const Terrain = core.matrix.Matrix(TerrainSpace);
pub const oob_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};

pub const Level = struct {
    width: u16,
    height: u16,
    terrain: Terrain,
    individuals: []const Individual,
};

pub const Individual = struct {
    id: u32,
    species: Species,
    abs_position: ThingPosition,
    status_conditions: StatusConditions = 0,
};

pub const StateDiff = union(enum) {
    spawn: Individual,
    despawn: Individual,
    small_move: IdAndCoord,
    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };

    large_move: IdAndCoords,
    pub const IdAndCoords = struct {
        id: u32,
        coords: [2]Coord,
    };

    polymorph: Polymorph,
    pub const Polymorph = struct {
        id: u32,
        from: Species,
        to: Species,
    };

    status_condition_diff: struct {
        id: u32,
        from: StatusConditions,
        to: StatusConditions,
    },

    terrain_update: TerrainDiff,
    pub const TerrainDiff = struct {
        at: Coord,
        from: TerrainSpace,
        to: TerrainSpace,
    };

    transition_to_next_level,
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),
    the_levels: []const Level,
    level_number: u16,

    pub fn generate(allocator: *std.mem.Allocator) !GameState {
        var game_state = GameState{
            .allocator = allocator,
            .terrain = undefined,
            .individuals = IdMap(*Individual).init(allocator),
            .the_levels = generateLevels(),
            .level_number = 0,
        };

        game_state.terrain = try buildTheTerrain(allocator, game_state.the_levels);

        // human is always id 1
        var non_human_id_cursor: u32 = 2;
        var found_human = false;
        for (game_state.the_levels[0].individuals) |template_individual, i| {
            var id: u32 = undefined;
            if (template_individual.species == .human) {
                id = 1;
                found_human = true;
            } else {
                id = non_human_id_cursor;
                non_human_id_cursor += 1;
            }
            const individual = try allocClone(allocator, template_individual);
            individual.id = id;
            try game_state.individuals.putNoClobber(id, individual);
        }
        std.debug.assert(found_human);

        return game_state;
    }

    pub fn clone(self: GameState) !GameState {
        return GameState{
            .allocator = self.allocator,
            .terrain = self.terrain,
            .individuals = blk: {
                var ret = IdMap(*Individual).init(self.allocator);
                var iterator = self.individuals.iterator();
                while (iterator.next()) |kv| {
                    try ret.putNoClobber(kv.key, try allocClone(self.allocator, kv.value.*));
                }
                break :blk ret;
            },
            .the_levels = self.the_levels,
            .level_number = self.level_number,
        };
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .despawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.plus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.getValue(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].plus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].plus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.getValue(polymorph.id).?;
                    individual.species = polymorph.to;
                },
                .status_condition_diff => |data| {
                    const individual = self.individuals.getValue(data.id).?;
                    individual.status_conditions = data.to;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.to;
                },
                .transition_to_next_level => {
                    self.level_number += 1;
                },
            }
        }
    }
    fn undoStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .despawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.minus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.getValue(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].minus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].minus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.getValue(polymorph.id).?;
                    individual.species = polymorph.from;
                },
                .status_condition_diff => |data| {
                    const individual = self.individuals.getValue(data.id).?;
                    individual.status_conditions = data.from;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.from;
                },
                .transition_to_next_level => {
                    self.level_number -= 1;
                },
            }
        }
    }

    pub fn terrainAt(self: GameState, coord: Coord) TerrainSpace {
        return self.terrain.getCoord(coord) orelse oob_terrain;
    }
};

fn buildTheTerrain(allocator: *std.mem.Allocator, the_levels: []const Level) !Terrain {
    var width: u16 = 0;
    var height: u16 = 1;
    for (the_levels) |level| {
        width += level.width;
        height = std.math.max(height, level.height);
    }

    const border_wall = TerrainSpace{
        .floor = .unknown,
        .wall = .stone,
    };
    var terrain = try Terrain.initFill(allocator, width, height, border_wall);

    var level_x: u16 = 0;
    for (the_levels) |level| {
        terrain.copy(level.terrain, level_x, 0, 0, 0, level.terrain.width, level.terrain.height);
        level_x += level.width;
    }

    return terrain;
}
