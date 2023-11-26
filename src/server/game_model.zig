const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;
const HashMap = std.HashMap;
const core = @import("core");
const Coord = core.geometry.Coord;

const NewGameSettings = core.protocol.NewGameSettings;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const ThingPosition = core.protocol.ThingPosition;
const EquipmentSlot = core.protocol.EquipmentSlot;
const TerrainSpace = core.protocol.TerrainSpace;
const StatusConditions = core.protocol.StatusConditions;

const map_gen = @import("./map_gen.zig");

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return std.AutoArrayHashMap(u32, V);
}

pub fn CoordMap(comptime V: type) type {
    return std.AutoArrayHashMap(Coord, V);
}

pub const oob_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};
pub const Terrain = core.matrix.SparseChunkedMatrix(TerrainSpace, oob_terrain, .{
    .metrics = true, // TODO: map generator should not use metrics
    .track_dirty_after_clone = true,
});

pub const Individual = struct {
    species: Species,
    abs_position: ThingPosition,
    perceived_origin: Coord,
    status_conditions: StatusConditions = 0,

    pub fn clone(self: Individual, allocator: Allocator) !*Individual {
        const other = try allocator.create(Individual);
        other.* = self;
        return other;
    }
};

pub const ItemLocation = union(enum) {
    floor_coord: Coord,
    held: HeldLocation,
};
pub const HeldLocation = struct {
    holder_id: u32,
    equipped_to_slot: EquipmentSlot,
};
pub const Item = struct {
    location: ItemLocation,
    kind: enum {
        shield,
        axe,
        torch,
        dagger,
    },

    pub fn clone(self: @This(), allocator: Allocator) !*@This() {
        const other = try allocator.create(@This());
        other.* = self;
        return other;
    }
};

pub const StateDiff = union(enum) {
    individual_spawn: struct {
        id: u32,
        individual: Individual,
    },
    individual_despawn: struct {
        id: u32,
        individual: Individual,
    },
    individual_reposition: struct {
        id: u32,
        from: ThingPosition,
        to: ThingPosition,
    },
    individual_polymorph: struct {
        id: u32,
        from: Species,
        to: Species,
    },
    individual_status_condition_update: struct {
        id: u32,
        from: StatusConditions,
        to: StatusConditions,
    },

    item_spawn: struct {
        id: u32,
        item: Item,
    },
    item_despawn: struct {
        id: u32,
        item: Item,
    },
    item_relocation: struct {
        id: u32,
        from: ItemLocation,
        to: ItemLocation,
    },

    terrain_update: struct {
        at: Coord,
        from: TerrainSpace,
        to: TerrainSpace,
    },

    transition_to_next_level,
};

pub const GameState = struct {
    allocator: Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),
    items: IdMap(*Item),
    level_number: u32,
    warp_points: []Coord,

    pub fn generate(allocator: Allocator, new_game_settings: NewGameSettings) !*GameState {
        const game_state = try allocator.create(GameState);
        game_state.* = GameState{
            .allocator = allocator,
            .terrain = Terrain.init(allocator),
            .individuals = IdMap(*Individual).init(allocator),
            .items = IdMap(*Item).init(allocator),
            .level_number = 0,
            .warp_points = &[_]Coord{},
        };
        try map_gen.generate(game_state, new_game_settings);
        return game_state;
    }

    pub fn clone(self: *GameState) !*GameState {
        const game_state = try self.allocator.create(GameState);
        game_state.* = GameState{
            .allocator = self.allocator,
            .terrain = try self.terrain.clone(self.allocator),
            .individuals = blk: {
                var ret = IdMap(*Individual).init(self.allocator);
                var iterator = self.individuals.iterator();
                while (iterator.next()) |kv| {
                    try ret.putNoClobber(kv.key_ptr.*, try kv.value_ptr.*.clone(self.allocator));
                }
                break :blk ret;
            },
            .items = blk: {
                var ret = IdMap(*Item).init(self.allocator);
                var iterator = self.items.iterator();
                while (iterator.next()) |kv| {
                    try ret.putNoClobber(kv.key_ptr.*, try kv.value_ptr.*.clone(self.allocator));
                }
                break :blk ret;
            },
            .level_number = self.level_number,
            .warp_points = try self.allocator.dupe(Coord, self.warp_points),
        };
        return game_state;
    }

    pub fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .individual_spawn => |data| {
                    try self.individuals.putNoClobber(data.id, try data.individual.clone(self.allocator));
                },
                .individual_despawn => |data| {
                    assert(self.individuals.swapRemove(data.id));
                },
                .individual_reposition => |data| {
                    self.individuals.get(data.id).?.abs_position = data.to;
                },
                .individual_polymorph => |polymorph| {
                    const individual = self.individuals.get(polymorph.id).?;
                    individual.species = polymorph.to;
                },
                .individual_status_condition_update => |data| {
                    const individual = self.individuals.get(data.id).?;
                    individual.status_conditions = data.to;
                },

                .item_spawn => |data| {
                    try self.items.putNoClobber(data.id, try data.item.clone(self.allocator));
                },
                .item_despawn => |data| {
                    assert(self.items.swapRemove(data.id));
                },
                .item_relocation => |data| {
                    self.items.get(data.id).?.location = data.to;
                },

                .terrain_update => |data| {
                    try self.terrain.putCoord(data.at, data.to);
                },
                .transition_to_next_level => {
                    self.level_number += 1;
                },
            }
        }
    }
    pub fn undoStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes, 0..) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .individual_spawn => |data| {
                    assert(self.individuals.swapRemove(data.id));
                },
                .individual_despawn => |data| {
                    try self.individuals.putNoClobber(data.id, try data.individual.clone(self.allocator));
                },
                .individual_reposition => |data| {
                    self.individuals.get(data.id).?.abs_position = data.from;
                },
                .individual_polymorph => |polymorph| {
                    const individual = self.individuals.get(polymorph.id).?;
                    individual.species = polymorph.from;
                },
                .individual_status_condition_update => |data| {
                    const individual = self.individuals.get(data.id).?;
                    individual.status_conditions = data.from;
                },

                .item_spawn => |data| {
                    assert(self.items.swapRemove(data.id));
                },
                .item_despawn => |data| {
                    try self.items.putNoClobber(data.id, try data.item.clone(self.allocator));
                },
                .item_relocation => |data| {
                    self.items.get(data.id).?.location = data.from;
                },

                .terrain_update => |data| {
                    try self.terrain.putCoord(data.at, data.from);
                },
                .transition_to_next_level => {
                    self.level_number -= 1;
                },
            }
        }
    }
};
