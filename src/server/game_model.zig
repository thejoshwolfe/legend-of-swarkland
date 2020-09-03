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

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return std.AutoHashMap(u32, V);
}

pub fn CoordMap(comptime V: type) type {
    return std.AutoHashMap(Coord, V);
}

pub const Terrain = core.matrix.Matrix(TerrainSpace);
pub const oob_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};

pub const Individual = struct {
    species: Species,
    abs_position: ThingPosition,
    status_conditions: StatusConditions = 0,
    has_shield: bool = false,

    pub fn clone(self: Individual, allocator: *std.mem.Allocator) !*Individual {
        var other = try allocator.create(Individual);
        other.* = self;
        return other;
    }
};

pub const StateDiff = union(enum) {
    spawn: IdAndIndividual,
    despawn: IdAndIndividual,

    small_move: IdAndCoord,

    large_move: IdAndCoords,

    polymorph: Polymorph,

    status_condition_diff: struct {
        id: u32,
        from: StatusConditions,
        to: StatusConditions,
    },

    terrain_update: TerrainDiff,

    pub const IdAndIndividual = struct {
        id: u32,
        individual: Individual,
    };
    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };
    pub const IdAndCoords = struct {
        id: u32,
        coords: [2]Coord,
    };
    pub const Polymorph = struct {
        id: u32,
        from: Species,
        to: Species,
    };
    pub const TerrainDiff = struct {
        at: Coord,
        from: TerrainSpace,
        to: TerrainSpace,
    };
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),

    pub fn generate(allocator: *std.mem.Allocator) !GameState {
        var game_state = GameState{
            .allocator = allocator,
            .terrain = undefined,
            .individuals = IdMap(*Individual).init(allocator),
        };
        try map_gen.generate(allocator, &game_state.terrain, &game_state.individuals);
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
                    try ret.putNoClobber(kv.key, try kv.value.*.clone(self.allocator));
                }
                break :blk ret;
            },
        };
    }

    pub fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |data| {
                    try self.individuals.putNoClobber(data.id, try data.individual.clone(self.allocator));
                },
                .despawn => |data| {
                    self.individuals.removeAssertDiscard(data.id);
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.get(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.plus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.get(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].plus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].plus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.get(polymorph.id).?;
                    individual.species = polymorph.to;
                },
                .status_condition_diff => |data| {
                    const individual = self.individuals.get(data.id).?;
                    individual.status_conditions = data.to;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.to;
                },
            }
        }
    }
    pub fn undoStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |data| {
                    self.individuals.removeAssertDiscard(data.id);
                },
                .despawn => |data| {
                    try self.individuals.putNoClobber(data.id, try data.individual.clone(self.allocator));
                },
                .small_move => |id_and_coord| {
                    const individual = self.individuals.get(id_and_coord.id).?;
                    individual.abs_position.small = individual.abs_position.small.minus(id_and_coord.coord);
                },
                .large_move => |id_and_coords| {
                    const individual = self.individuals.get(id_and_coords.id).?;
                    individual.abs_position.large = .{
                        individual.abs_position.large[0].minus(id_and_coords.coords[0]),
                        individual.abs_position.large[1].minus(id_and_coords.coords[1]),
                    };
                },
                .polymorph => |polymorph| {
                    const individual = self.individuals.get(polymorph.id).?;
                    individual.species = polymorph.from;
                },
                .status_condition_diff => |data| {
                    const individual = self.individuals.get(data.id).?;
                    individual.status_conditions = data.from;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.from;
                },
            }
        }
    }

    pub fn terrainAt(self: GameState, coord: Coord) TerrainSpace {
        return self.terrain.getCoord(coord) orelse oob_terrain;
    }
};
