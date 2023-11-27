const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;
const ArrayList = std.ArrayList;
const core = @import("core");
const Coord = core.geometry.Coord;
const Rect = core.geometry.Rect;
const sign = core.geometry.sign;
const CardinalDirection = core.geometry.CardinalDirection;
const isOrthogonalUnitVector = core.geometry.isOrthogonalUnitVector;
const isOrthogonalVectorOfMagnitude = core.geometry.isOrthogonalVectorOfMagnitude;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;
const cardinalDirectionToDelta = core.geometry.cardinalDirectionToDelta;
const makeCoord = core.geometry.makeCoord;
const zero_vector = makeCoord(0, 0);

const Action = core.protocol.Action;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const PerceivedFrame = core.protocol.PerceivedFrame;
const ThingPosition = core.protocol.ThingPosition;
const PerceivedThing = core.protocol.PerceivedThing;
const PerceivedActivity = core.protocol.PerceivedActivity;
const TerrainSpace = core.protocol.TerrainSpace;
const TerrainChunk = core.protocol.TerrainChunk;
const Equipment = core.protocol.Equipment;
const EquipmentSlot = core.protocol.EquipmentSlot;
const StatusConditions = core.protocol.StatusConditions;

const PerceivedTerrain = core.game_logic.PerceivedTerrain;
const getViewDistance = core.game_logic.getViewDistance;
const isOpenSpace = core.game_logic.isOpenSpace;
const isWet = core.game_logic.isWet;
const isTransparentSpace = core.game_logic.isTransparentSpace;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const applyMovementToPosition = core.game_logic.applyMovementToPosition;
const offsetPosition = core.game_logic.offsetPosition;
const getPhysicsLayer = core.game_logic.getPhysicsLayer;
const isSlow = core.game_logic.isSlow;
const limpsAfterLunge = core.game_logic.limpsAfterLunge;
const getAttackFunction = core.game_logic.getAttackFunction;
const actionCausesPainWhileMalaised = core.game_logic.actionCausesPainWhileMalaised;

const game_model = @import("./game_model.zig");
const GameState = game_model.GameState;
const Individual = game_model.Individual;
const Item = game_model.Item;
const StateDiff = game_model.StateDiff;
const IdMap = game_model.IdMap;
const CoordMap = game_model.CoordMap;
const Terrain = game_model.Terrain;
const oob_terrain = game_model.oob_terrain;

const the_levels = @import("map_gen.zig").the_levels;

const GameEngine = @This();
allocator: Allocator,
state: *GameState,

pub const Happenings = struct {
    individual_to_perception: IdMap([]PerceivedFrame),
    state_changes: []StateDiff,
};

/// Does not modify the game state.
pub fn getStaticPerception(allocator: Allocator, game_state: *GameState, individual_id: u32) !PerceivedFrame {
    var self = GameEngine{
        .allocator = allocator,
        .state = game_state,
    };
    return self.getPerceivedFrame(
        individual_id,
        Activities.static_state,
    );
}

/// Computes what would happen to the state of the game.
/// Does not modify the game state.
/// This is the entry point for all game rules.
pub fn computeHappenings(allocator: Allocator, pristine_game_state: *GameState, actions: IdMap(Action)) !Happenings {
    var self = GameEngine{
        .allocator = allocator,
        .state = try pristine_game_state.clone(),
    };
    const individual_to_perception = try self.doAllTheThings(actions);

    return Happenings{
        .individual_to_perception = blk: {
            var ret = IdMap([]PerceivedFrame).init(self.allocator);
            for (pristine_game_state.individuals.keys()) |id| {
                var frame_list = individual_to_perception.get(id).?.frames;
                // remove empty frames, except the last one
                var i: usize = 0;
                frameLoop: while (i + 1 < frame_list.items.len) : (i +%= 1) {
                    const frame = frame_list.items[i];
                    if (frame.self.kind.individual.activity != PerceivedActivity.none) continue :frameLoop;
                    for (frame.others) |other| {
                        if (other.kind == .individual and other.kind.individual.activity != PerceivedActivity.none) continue :frameLoop;
                    }
                    // delete this frame
                    _ = frame_list.orderedRemove(i);
                    i -%= 1;
                }
                try ret.putNoClobber(id, try frame_list.toOwnedSlice());
            }
            break :blk ret;
        },
        .state_changes = blk: {
            var ret = ArrayList(StateDiff).init(self.allocator);
            // terrain
            {
                var it = self.state.terrain.chunks.iterator();
                while (it.next()) |entry| {
                    const chunk_coord = entry.key_ptr.*;
                    const new_chunk = entry.value_ptr.*;
                    if (!new_chunk.is_dirty) continue;
                    const old_chunk = pristine_game_state.terrain.chunks.get(chunk_coord).?;
                    for (old_chunk.data, 0..) |old_cell, i| {
                        const new_cell = new_chunk.data[i];
                        if (!std.meta.eql(old_cell, new_cell)) {
                            try ret.append(StateDiff{ .terrain_update = .{
                                .at = Terrain.chunkCoordAndInnerIndexToCoord(chunk_coord, i),
                                .from = old_cell,
                                .to = new_cell,
                            } });
                        }
                    }
                }
            }

            // individual updates and despawns
            {
                var it = pristine_game_state.individuals.iterator();
                while (it.next()) |entry| {
                    const id = entry.key_ptr.*;
                    const old_individual = entry.value_ptr.*;
                    const new_individual = self.state.individuals.get(id) orelse {
                        // despawned
                        try ret.append(StateDiff{ .individual_despawn = .{
                            .id = id,
                            .individual = old_individual.*,
                        } });
                        continue;
                    };
                    if (!std.meta.eql(old_individual.species, new_individual.species)) {
                        try ret.append(StateDiff{ .individual_polymorph = .{
                            .id = id,
                            .from = old_individual.species,
                            .to = new_individual.species,
                        } });
                    }
                    if (!std.meta.eql(old_individual.abs_position, new_individual.abs_position)) {
                        try ret.append(StateDiff{ .individual_reposition = .{
                            .id = id,
                            .from = old_individual.abs_position,
                            .to = new_individual.abs_position,
                        } });
                    }
                    if (!std.meta.eql(old_individual.status_conditions, new_individual.status_conditions)) {
                        try ret.append(StateDiff{ .individual_status_condition_update = .{
                            .id = id,
                            .from = old_individual.status_conditions,
                            .to = new_individual.status_conditions,
                        } });
                    }
                    assert(std.meta.eql(old_individual.perceived_origin, new_individual.perceived_origin));
                }
            }
            // individual spawns
            {
                var it = self.state.individuals.iterator();
                while (it.next()) |entry| {
                    const id = entry.key_ptr.*;
                    const new_individual = entry.value_ptr.*;
                    if (pristine_game_state.individuals.contains(id)) continue;
                    // new individual.
                    try ret.append(StateDiff{ .individual_spawn = .{
                        .id = id,
                        .individual = new_individual.*,
                    } });
                }
            }

            // item updates and despawns
            {
                var it = pristine_game_state.items.iterator();
                while (it.next()) |entry| {
                    const id = entry.key_ptr.*;
                    const old_item = entry.value_ptr.*;
                    const new_item = self.state.items.get(id) orelse {
                        // despawned
                        try ret.append(StateDiff{ .item_despawn = .{
                            .id = id,
                            .item = old_item.*,
                        } });
                        continue;
                    };
                    if (!std.meta.eql(old_item.location, new_item.location)) {
                        try ret.append(StateDiff{ .item_relocation = .{
                            .id = id,
                            .from = old_item.location,
                            .to = new_item.location,
                        } });
                    }
                    switch (new_item.location) {
                        .held => |held| {
                            assert(self.state.individuals.contains(held.holder_id));
                        },
                        else => {},
                    }
                }
            }
            // item spawns
            {
                var it = self.state.items.iterator();
                while (it.next()) |entry| {
                    const id = entry.key_ptr.*;
                    const new_item = entry.value_ptr.*;
                    if (pristine_game_state.items.contains(id)) continue;
                    // new item.
                    try ret.append(StateDiff{ .item_spawn = .{
                        .id = id,
                        .item = new_item.*,
                    } });
                }
            }

            if (pristine_game_state.level_number != self.state.level_number) {
                @panic("TODO");
            }
            break :blk try ret.toOwnedSlice();
        },
    };
}

const HappeningsComputation = struct {
    // this list is updated as individuals die. TODO: do we actually need this copy?
    everybody: []u32,
    // individuals that move.
    budges_at_all: IdMap(void),

    individual_to_perception: IdMap(*MutablePerceivedHappening),
    total_deaths: IdMap(void),
    polymorphers: IdMap(void),
};
fn doAllTheThings(self: *GameEngine, actions: IdMap(Action)) !IdMap(*MutablePerceivedHappening) {
    var hc = HappeningsComputation{
        .everybody = try self.allocator.dupe(u32, self.state.individuals.keys()),
        .budges_at_all = IdMap(void).init(self.allocator),
        .individual_to_perception = IdMap(*MutablePerceivedHappening).init(self.allocator),
        .total_deaths = IdMap(void).init(self.allocator),
        .polymorphers = IdMap(void).init(self.allocator),
    };

    for (hc.everybody) |id| {
        // TODO: why is this on the heap?
        const h = try self.allocator.create(MutablePerceivedHappening);
        h.* = MutablePerceivedHappening.init(self.allocator);
        try hc.individual_to_perception.putNoClobber(id, h);
    }

    // Cheating
    {
        for (hc.everybody) |id| {
            const index = switch (actions.get(id).?) {
                .cheatcode_warp => |index| index,
                else => continue,
            };
            if (index < self.state.warp_points.len) {
                self.state.individuals.get(id).?.abs_position = ThingPosition{ .small = self.state.warp_points[index] };
            }
        }
    }

    // Shrinking
    {
        var shrinks = IdMap(u1).init(self.allocator);
        var next_positions = IdMap(ThingPosition).init(self.allocator);
        for (hc.everybody) |id| {
            const index: u1 = switch (actions.get(id).?) {
                .shrink => |index| index,
                else => continue,
            };
            const old_position = self.state.individuals.get(id).?.abs_position.large;
            try next_positions.putNoClobber(id, ThingPosition{ .small = old_position[index] });
            try shrinks.putNoClobber(id, index);
        }
        for (hc.everybody) |id| {
            try observeFrame(
                self,
                id,
                hc.individual_to_perception.get(id).?,
                Activities{
                    .shrinks = &shrinks,
                },
            );
        }
        for (hc.everybody) |id| {
            const next_position = next_positions.get(id) orelse continue;
            self.state.individuals.get(id).?.abs_position = next_position;
        }
    }

    // Moving normally
    {
        var intended_moves = IdMap(Coord).init(self.allocator);
        for (hc.everybody) |id| {
            switch (actions.get(id).?) {
                .move, .lunge => |direction| {
                    const move_delta = cardinalDirectionToDelta(direction);
                    try intended_moves.putNoClobber(id, move_delta);
                },
                .charge => {
                    const position_coords = self.state.individuals.get(id).?.abs_position.large;
                    const position_delta = position_coords[0].minus(position_coords[1]);
                    const move_delta = position_delta.scaled(2);
                    try intended_moves.putNoClobber(id, move_delta);
                },
                else => continue,
            }
            const individual = self.state.individuals.get(id).?;
            if (0 != individual.status_conditions & core.protocol.StatusCondition_grappling) {
                // also move the grapplee
                const coord = getHeadPosition(individual.abs_position);
                for (hc.everybody) |other_id| {
                    const other = self.state.individuals.get(other_id).?;
                    if (0 == other.status_conditions & core.protocol.StatusCondition_grappled) continue;
                    if (!coord.equals(getHeadPosition(other.abs_position))) continue;
                    try intended_moves.putNoClobber(other_id, intended_moves.get(id).?);
                }
            }
        }

        try self.doMovementAndCollisions(
            &hc.everybody,
            &hc.individual_to_perception,
            &intended_moves,
            &hc.budges_at_all,
            &hc.total_deaths,
        );
    }

    // Growing
    {
        var intended_moves = IdMap(Coord).init(self.allocator);
        var next_positions = IdMap(ThingPosition).init(self.allocator);
        for (hc.everybody) |id| {
            const move_delta: Coord = switch (actions.get(id).?) {
                .grow => |direction| cardinalDirectionToDelta(direction),
                else => continue,
            };
            try intended_moves.putNoClobber(id, move_delta);
            const old_coord = self.state.individuals.get(id).?.abs_position.small;
            const new_head_coord = old_coord.plus(move_delta);
            if (!isOpenSpace(self.state.terrain.getCoord(new_head_coord).wall)) {
                // that's a wall
                continue;
            }
            try next_positions.putNoClobber(id, ThingPosition{ .large = .{ new_head_coord, old_coord } });
        }

        for (hc.everybody) |id| {
            try observeFrame(
                self,
                id,
                hc.individual_to_perception.get(id).?,
                Activities{
                    .growths = .{
                        .intended_moves = &intended_moves,
                        .next_positions = &next_positions,
                    },
                },
            );
        }

        for (hc.everybody) |id| {
            const next_position = next_positions.get(id) orelse continue;
            self.state.individuals.get(id).?.abs_position = next_position;
        }
    }

    // Using doors
    {
        var door_toggles = CoordMap(u2).init(self.allocator);
        for (hc.everybody) |id| {
            const direction = switch (actions.get(id).?) {
                .open_close => |direction| direction,
                else => continue,
            };
            const door_position = getHeadPosition(self.state.individuals.get(id).?.abs_position).plus(cardinalDirectionToDelta(direction));
            const gop = try door_toggles.getOrPut(door_position);
            if (gop.found_existing) {
                gop.value_ptr.* +|= 1;
            } else {
                gop.value_ptr.* = 1;
            }
        }
        for (door_toggles.keys(), 0..) |door_position, i| {
            if (door_toggles.values()[i] > 1) continue;
            const cell = self.state.terrain.getCoord(door_position);
            switch (cell.wall) {
                .door_open => {
                    self.state.terrain.getExistingCoord(door_position).wall = .door_closed;
                },
                .door_closed => {
                    self.state.terrain.getExistingCoord(door_position).wall = .door_open;
                },
                else => {
                    // There's no door here. Nothing happens.
                },
            }
        }
        // Do we need to explicitly observe this? is there going to be an animation for it or something?
    }

    // Kicks
    {
        var kicks = IdMap(Coord).init(self.allocator);
        var intended_moves = IdMap(Coord).init(self.allocator);
        var kicked_too_much = IdMap(void).init(self.allocator);
        for (hc.everybody) |id| {
            const kick_direction: Coord = switch (actions.get(id).?) {
                .kick => |direction| cardinalDirectionToDelta(direction),
                else => continue,
            };
            try kicks.putNoClobber(id, kick_direction);

            var kicked_anybody = false;
            const attacker_coord = getHeadPosition(self.state.individuals.get(id).?.abs_position);
            const kick_coord = attacker_coord.plus(kick_direction);
            for (hc.everybody) |other_id| {
                const other = self.state.individuals.get(other_id).?;
                const position = other.abs_position;
                for (getAllPositions(&position)) |coord| {
                    if (!coord.equals(kick_coord)) continue;
                    // gotchya
                    switch (getPhysicsLayer(other.species)) {
                        3 => {
                            // Your kick is not stronk enough.
                            continue;
                        },
                        0 => {
                            // Instead you get sucked in!
                            if (try intended_moves.fetchPut(id, kick_direction)) |_| {
                                // kicked multiple times at once!
                                _ = try kicked_too_much.put(id, {});
                            }
                            continue;
                        },
                        else => {},
                    }
                    kicked_anybody = true;
                    if (try intended_moves.fetchPut(other_id, kick_direction)) |_| {
                        // kicked multiple times at once!
                        _ = try kicked_too_much.put(other_id, {});
                    }
                }
            }
            if (!kicked_anybody and !isOpenSpace(self.state.terrain.getCoord(kick_coord).wall)) {
                // Shove off the wall.
                if (try intended_moves.fetchPut(id, kick_direction.scaled(-1))) |_| {
                    _ = try kicked_too_much.put(id, {});
                }
            }
        }
        if (kicks.count() > 0) {
            for (hc.everybody) |id| {
                try observeFrame(
                    self,
                    id,
                    hc.individual_to_perception.get(id).?,
                    Activities{
                        .kicks = &kicks,
                    },
                );
            }

            // for now, multiple kicks at once just fail.
            for (kicked_too_much.keys()) |key| {
                assert(intended_moves.swapRemove(key));
            }

            try self.doMovementAndCollisions(
                &hc.everybody,
                &hc.individual_to_perception,
                &intended_moves,
                &hc.budges_at_all,
                &hc.total_deaths,
            );
        }
    }

    // pick up, equip, unequp
    {
        var item_claims = IdMap(?struct { individual_id: u32, to_slot: EquipmentSlot }).init(self.allocator);
        for (hc.everybody) |individual_id| {
            const action = actions.get(individual_id).?;
            switch (action) {
                .pick_up_and_equip, .pick_up_unequipped => {
                    const individual = self.state.individuals.get(individual_id).?;
                    const pick_up_coord = getHeadPosition(individual.abs_position);
                    var it = self.state.items.iterator();
                    while (it.next()) |entry| {
                        const item_id = entry.key_ptr.*;
                        const item = entry.value_ptr.*;
                        switch (item.location) {
                            .floor_coord => |floor_coord| {
                                if (!floor_coord.equals(pick_up_coord)) continue;
                            },
                            .held => continue,
                        }
                        const gop = try item_claims.getOrPut(item_id);
                        if (gop.found_existing) {
                            // Too many claims on this item.
                            gop.value_ptr.* = null;
                        } else {
                            const to_slot: EquipmentSlot = if (action == .pick_up_unequipped) .none else itemToEquipmentSlot(item);
                            gop.value_ptr.* = .{ .individual_id = individual_id, .to_slot = to_slot };
                        }
                    }
                },
                .unequip => |equipped_item| {
                    var it = inventoryIterator(self.state, individual_id);
                    while (it.next()) |entry| {
                        const inventory_item = entry.value_ptr.*;
                        if (itemToEquippedItem(inventory_item) == equipped_item) {
                            inventory_item.location.held.equipped_to_slot = .none;
                        }
                    }
                },
                .equip => |equipped_item| {
                    var bumped_out_of_slot: ?EquipmentSlot = null;
                    {
                        var it = inventoryIterator(self.state, individual_id);
                        while (it.next()) |entry| {
                            const inventory_item = entry.value_ptr.*;
                            if (itemToEquippedItem(inventory_item) == equipped_item and inventory_item.location.held.equipped_to_slot == .none) {
                                const slot = itemToEquipmentSlot(inventory_item);
                                inventory_item.location.held.equipped_to_slot = slot;
                                bumped_out_of_slot = slot;
                            }
                        }
                    }
                    if (bumped_out_of_slot) |slot_to_vacate| {
                        var it = inventoryIterator(self.state, individual_id);
                        while (it.next()) |entry| {
                            const inventory_item = entry.value_ptr.*;
                            if (itemToEquippedItem(inventory_item) != equipped_item and inventory_item.location.held.equipped_to_slot == slot_to_vacate) {
                                // Drop the item in the conflicting slot.
                                inventory_item.location = .{
                                    .floor_coord = getHeadPosition(self.state.individuals.get(individual_id).?.abs_position),
                                };
                            }
                        }
                    }
                },
                else => {},
            }
        }
        var it = item_claims.iterator();
        while (it.next()) |entry| {
            const item_id = entry.key_ptr.*;
            const claim = entry.value_ptr.* orelse continue;
            // the pick up succeeds.
            var other_it = inventoryIterator(self.state, claim.individual_id);
            while (other_it.next()) |other_entry| {
                const other_item = other_entry.value_ptr.*;
                if (other_item.location.held.equipped_to_slot == claim.to_slot) {
                    // Drop the item in the conflicting slot.
                    other_item.location = .{
                        .floor_coord = getHeadPosition(self.state.individuals.get(claim.individual_id).?.abs_position),
                    };
                }
            }
            self.state.items.get(item_id).?.location = .{ .held = .{ .holder_id = claim.individual_id, .equipped_to_slot = claim.to_slot } };
        }
    }

    // Wounds, limping, pain, etc.
    for (hc.everybody) |id| {
        const individual = self.state.individuals.get(id).?;
        const species = individual.species;
        if (self.state.terrain.getCoord(getHeadPosition(individual.abs_position)).floor == .marble) {
            // this tile heals you for some reason.
            const healed_statuses = core.protocol.StatusCondition_wounded_leg | //
                core.protocol.StatusCondition_limping | //
                core.protocol.StatusCondition_malaise | //
                core.protocol.StatusCondition_pain;
            individual.status_conditions &= ~healed_statuses;
        }
        if (hc.budges_at_all.contains(id)) {
            // You moved.
            if (0 != individual.status_conditions & core.protocol.StatusCondition_wounded_leg or //
                isSlow(species) or //
                (actions.get(id).? == .lunge and limpsAfterLunge(species)))
            {
                // now you limp.
                individual.status_conditions |= core.protocol.StatusCondition_limping;
            }
            // A nocked arrow does not survive moving.
            individual.status_conditions &= ~core.protocol.StatusCondition_arrow_nocked;
        } else {
            // you held still, so you are free of any limping status.
            individual.status_conditions &= ~core.protocol.StatusCondition_limping;
        }
        if (0 != individual.status_conditions & core.protocol.StatusCondition_malaise and //
            actionCausesPainWhileMalaised(actions.get(id).?))
        {
            individual.status_conditions |= core.protocol.StatusCondition_pain;
        } else {
            individual.status_conditions &= ~core.protocol.StatusCondition_pain;
        }
    }

    // Attacks
    var attack_deaths = IdMap(void).init(self.allocator);
    {
        var attacks = IdMap(Activities.Attack).init(self.allocator);
        var defends = IdMap(CardinalDirection).init(self.allocator);
        var nibbles = IdMap(void).init(self.allocator);
        var stomps = IdMap(void).init(self.allocator);
        var intended_moves = IdMap(Coord).init(self.allocator);
        var pushed_too_much = IdMap(void).init(self.allocator);
        for (hc.everybody) |id| {
            switch (actions.get(id).?) {
                .defend => |direction| {
                    try defends.putNoClobber(id, direction);
                },
                else => {},
            }
        }
        for (hc.everybody) |id| {
            const action = actions.get(id).?;
            switch (action) {
                .attack, .lunge, .fire_bow => |attack_direction| {
                    const attack_delta = cardinalDirectionToDelta(attack_direction);
                    const attacker = self.state.individuals.get(id).?;
                    attacker.status_conditions &= ~core.protocol.StatusCondition_arrow_nocked;
                    var attacker_coord = getHeadPosition(attacker.abs_position);
                    const attacker_equipment = getEquipment(self.state, id);
                    const attack_function = getAttackFunction(attacker.species, attacker_equipment);
                    var attack_distance: i32 = 1;
                    const range: i32 = if (action == .fire_bow) core.game_logic.bow_range else 1;
                    while (attack_distance <= range) : (attack_distance += 1) {
                        var damage_position = attacker_coord.plus(attack_delta.scaled(attack_distance));
                        var stop_the_attack = false;
                        for (hc.everybody) |other_id| {
                            const other = self.state.individuals.get(other_id).?;
                            const other_position_index = blk: for (getAllPositions(&other.abs_position), 0..) |coord, i| {
                                if (coord.equals(damage_position)) break :blk i;
                            } else {
                                // Not in this position.
                                continue;
                            };

                            const is_shielding = blk: {
                                if (defends.get(other_id)) |direction| {
                                    break :blk (@intFromEnum(direction) +% 2 == @intFromEnum(attack_direction));
                                } else break :blk false;
                            };

                            const effect = core.game_logic.getAttackEffect(
                                attacker.species,
                                attacker_equipment,
                                other.species,
                                other_position_index,
                                other.status_conditions,
                                is_shielding,
                            );
                            if (effect == .miss) {
                                // Don't stop the attack.
                                continue;
                            }
                            stop_the_attack = true;
                            switch (effect) {
                                .miss => unreachable, // Handled above.
                                .no_effect => {},
                                .kill => {
                                    _ = try attack_deaths.put(other_id, {});
                                },
                                .wound => {
                                    other.status_conditions |= core.protocol.StatusCondition_wounded_leg;
                                },
                                .malaise => {
                                    other.status_conditions |= core.protocol.StatusCondition_malaise;
                                },
                                .shield_parry => {
                                    if (range == 1) {
                                        attacker.status_conditions |= (core.protocol.StatusCondition_limping | core.protocol.StatusCondition_pain);
                                    }
                                },
                                .heavy_hit_knocks_away_shield => {
                                    // drop the shield.
                                    var it = inventoryIterator(self.state, other_id);
                                    while (it.next()) |entry| {
                                        const item = entry.value_ptr.*;
                                        if (item.kind == .shield) {
                                            item.location = .{ .floor_coord = damage_position };
                                        }
                                    }
                                    // become pain.
                                    other.status_conditions |= core.protocol.StatusCondition_pain;
                                },
                            }
                        }
                        if (attack_function == .smash) {
                            // whoosh effects push people around.
                            for ([_]core.geometry.CardinalDirection{ .east, .south, .west, .north }) |whoosh_dir| {
                                if (@intFromEnum(whoosh_dir) +% 2 == @intFromEnum(attack_direction)) continue;
                                const whoosh_delta = cardinalDirectionToDelta(whoosh_dir);
                                const whoosh_coord = damage_position.plus(whoosh_delta);
                                for (hc.everybody) |other_id| {
                                    const other = self.state.individuals.get(other_id).?;
                                    switch (getPhysicsLayer(other.species)) {
                                        // unaffected by whoosh.
                                        0, 3 => continue,
                                        1, 2 => {},
                                    }
                                    for (getAllPositions(&other.abs_position)) |coord| {
                                        if (!coord.equals(whoosh_coord)) continue;
                                        if (try intended_moves.fetchPut(other_id, whoosh_delta)) |_| {
                                            // pushed multiple times at once
                                            _ = try pushed_too_much.put(other_id, {});
                                        }
                                    }
                                }
                            }
                        }
                        if (stop_the_attack) break;
                    }
                    try attacks.putNoClobber(id, Activities.Attack{
                        .direction = attack_delta,
                        .distance = attack_distance,
                    });
                },
                .nibble, .stomp => {
                    const is_stomp = action == .stomp;
                    const attacker = self.state.individuals.get(id).?;
                    const attack_function = getAttackFunction(attacker.species, getEquipment(self.state, id));
                    const attacker_coord = getHeadPosition(attacker.abs_position);
                    const attacker_physics_layer = getPhysicsLayer(attacker.species);
                    const damage_position = attacker_coord;
                    for (hc.everybody) |other_id| {
                        if (other_id == id) continue; // Don't hit yourself.
                        const other = self.state.individuals.get(other_id).?;
                        const other_physics_layer = getPhysicsLayer(other.species);

                        if (is_stomp) {
                            // stomping only works downward.
                            if (!(attacker_physics_layer > other_physics_layer)) continue;
                        } else {
                            // nibbling only works upward.
                            if (!(attacker_physics_layer < other_physics_layer)) continue;
                        }
                        for (getAllPositions(&other.abs_position), 0..) |coord, i| {
                            if (!coord.equals(damage_position)) continue;
                            // hit something.
                            const is_effective = blk: {
                                // innate defense
                                if (!core.game_logic.isAffectedByAttacks(other.species, i)) break :blk false;
                                break :blk true;
                            };
                            if (is_effective) {
                                // get wrecked
                                if (is_stomp) {
                                    // stomping is instant deth.
                                    _ = try attack_deaths.put(other_id, {});
                                } else {
                                    // nibbling does attack damage.
                                    try doAttackDamage(attack_function, other_id, other.species, &other.status_conditions, &attack_deaths);
                                }
                            }
                        }
                    }
                    if (is_stomp) {
                        try stomps.putNoClobber(id, {});
                    } else {
                        try nibbles.putNoClobber(id, {});
                    }
                },
                .nock_arrow => {
                    const attacker = self.state.individuals.get(id).?;
                    attacker.status_conditions |= core.protocol.StatusCondition_arrow_nocked;
                },
                else => continue,
            }
        }

        // observe
        if (attacks.count() + defends.count() + nibbles.count() + stomps.count() > 0) {
            for (hc.everybody) |id| {
                try observeFrame(
                    self,
                    id,
                    hc.individual_to_perception.get(id).?,
                    Activities{ .attacks = .{
                        .attacks = &attacks,
                        .defends = &defends,
                        .nibbles = &nibbles,
                        .stomps = &stomps,
                    } },
                );
            }
        }

        // resolve pushes
        if (intended_moves.count() > 0) {
            for (pushed_too_much.keys()) |key| {
                assert(intended_moves.swapRemove(key));
            }

            try self.doMovementAndCollisions(
                &hc.everybody,
                &hc.individual_to_perception,
                &intended_moves,
                &hc.budges_at_all,
                &hc.total_deaths,
            );
        }
    }
    // Lava (and deaths from attacks for some reason)
    {
        for (hc.everybody) |id| {
            const position = self.state.individuals.get(id).?.abs_position;
            for (getAllPositions(&position)) |coord| {
                if (self.state.terrain.getCoord(coord).floor == .lava) {
                    _ = try attack_deaths.put(id, {});
                }
            }
        }
        if (attack_deaths.count() > 0) {
            for (hc.everybody) |id| {
                try observeFrame(
                    self,
                    id,
                    hc.individual_to_perception.get(id).?,
                    Activities{ .deaths = &attack_deaths },
                );
            }
        }
        try self.flushDeaths(&hc.total_deaths, &attack_deaths, &hc.everybody);
    }

    // Grapple and digestion
    {
        var grappler_to_victim_count = IdMap(u32).init(self.allocator);
        var victim_to_has_multiple_attackers = IdMap(bool).init(self.allocator);
        var victim_to_unique_attacker = IdMap(u32).init(self.allocator);
        for (hc.everybody) |id| {
            const individual = self.state.individuals.get(id).?;
            switch (individual.species) {
                .blob, .ant => {},
                else => continue,
            }
            // grappler
            const grappler_physics_layer = getPhysicsLayer(individual.species);
            for (hc.everybody) |other_id| {
                if (other_id == id) continue;
                const other = self.state.individuals.get(other_id).?;
                switch (getPhysicsLayer(other.species)) {
                    0 => continue, // blobs are ungrapplable.
                    1 => if (grappler_physics_layer == 1) continue, // how'd you get there.
                    2 => {}, // always grapplable.
                    3 => if (grappler_physics_layer == 1) continue, // too small to grapple large bois.
                }
                find_collision: for (getAllPositions(&individual.abs_position)) |coord| {
                    const other_position = other.abs_position;
                    for (getAllPositions(&other_position)) |other_coord| {
                        // any overlap means you get grappled.
                        if (other_coord.equals(coord)) break :find_collision;
                    }
                } else {
                    continue;
                }

                // Grapple is happening.
                {
                    const gop = try victim_to_has_multiple_attackers.getOrPut(other_id);
                    if (gop.found_existing) {
                        // multiple attackers.
                        assert(victim_to_unique_attacker.swapRemove(other_id));
                        gop.value_ptr.* = true;
                    } else {
                        // we're the first attacker.
                        try victim_to_unique_attacker.putNoClobber(other_id, id);
                        gop.value_ptr.* = false;
                    }
                }
                const gop = try grappler_to_victim_count.getOrPut(id);
                if (gop.found_existing) {
                    gop.value_ptr.* += 1;
                } else {
                    gop.value_ptr.* = 1;
                }
            }
        }

        var digestion_deaths = IdMap(void).init(self.allocator);
        var digesters = IdMap(void).init(self.allocator);
        for (hc.everybody) |id| {
            const victim = self.state.individuals.get(id).?;
            if (!victim_to_has_multiple_attackers.contains(id)) {
                // Not grappled.
                victim.status_conditions &= ~core.protocol.StatusCondition_grappled;
                if (0 != victim.status_conditions & core.protocol.StatusCondition_being_digested) {
                    // you've escaped digestion. the status effect turns into a leg wound i guess.
                    victim.status_conditions &= ~core.protocol.StatusCondition_being_digested;
                    victim.status_conditions |= core.protocol.StatusCondition_wounded_leg;
                }
            } else {
                // Grappled.
                victim.status_conditions |= core.protocol.StatusCondition_grappled;
                if (victim_to_unique_attacker.get(id)) |attacker_id| {
                    const attacker_individual = self.state.individuals.get(attacker_id).?;
                    const blob_subspecies = switch (attacker_individual.species) {
                        .blob => |subspecies| subspecies,
                        else => continue,
                    };
                    // Blobs digest their prey.

                    // Is the victim vulnerable to digestion attacks?
                    if (!core.game_logic.isAffectedByAttacks(self.state.individuals.get(id).?.species, 0)) {
                        // Too strong for the blob's attacks.
                        continue;
                    }

                    // Need a sufficient density of blob on this space to do a digestion.
                    const can_digest = switch (blob_subspecies) {
                        .large_blob => true,
                        .small_blob => attacker_individual.abs_position == .small,
                    };
                    if (!can_digest) continue;
                    if (0 == victim.status_conditions & core.protocol.StatusCondition_being_digested) {
                        // Start getting digested.
                        victim.status_conditions |= core.protocol.StatusCondition_being_digested;
                        _ = try digesters.put(attacker_id, {});
                    } else {
                        // Complete the digestion
                        try digestion_deaths.put(id, {});
                        grappler_to_victim_count.getEntry(attacker_id).?.value_ptr.* -= 1;

                        if (blob_subspecies == .small_blob) {
                            // Grow up to be large.
                            self.state.individuals.get(attacker_id).?.species = .{ .blob = .large_blob };
                            try hc.polymorphers.put(attacker_id, {});
                        }
                    }
                }
            }
        }
        // attacker-side statuses.
        for (hc.everybody) |id| {
            const individual = self.state.individuals.get(id).?;
            if ((grappler_to_victim_count.get(id) orelse 0) > 0) {
                individual.status_conditions |= core.protocol.StatusCondition_grappling;
            } else {
                individual.status_conditions &= ~core.protocol.StatusCondition_grappling;
            }
            if (digesters.contains(id)) {
                individual.status_conditions |= core.protocol.StatusCondition_digesting;
            } else {
                individual.status_conditions &= ~core.protocol.StatusCondition_digesting;
            }
        }

        for (hc.everybody) |id| {
            if (digestion_deaths.count() != 0) {
                try observeFrame(
                    self,
                    id,
                    hc.individual_to_perception.get(id).?,
                    Activities{ .deaths = &digestion_deaths },
                );
            }
        }
        try self.flushDeaths(&hc.total_deaths, &digestion_deaths, &hc.everybody);
    }

    // Environmental death triggers
    {
        var check_bloody_water_near: ?Coord = null;
        for (hc.total_deaths.keys()) |id| {
            const coord = getHeadPosition(self.state.individuals.get(id).?.abs_position);
            const cell = self.state.terrain.getCoord(coord);
            switch (cell.floor) {
                .water => {
                    self.state.terrain.getExistingCoord(coord).floor = .water_bloody;
                    check_bloody_water_near = coord;
                },
                else => {
                    // There's no death trigger here.
                    continue;
                },
            }
        }
        if (check_bloody_water_near) |center| {
            var bloody_spots = ArrayList(Coord).init(self.allocator);
            var clean_spots = ArrayList(Coord).init(self.allocator);
            var it = (Rect{
                .x = center.x - 2,
                .y = center.y - 2,
                .width = 5,
                .height = 5,
            }).rowMajorIterator();
            while (it.next()) |coord| {
                const cell = self.state.terrain.getCoord(coord);
                switch (cell.floor) {
                    .water => {
                        try clean_spots.append(coord);
                    },
                    .water_bloody => {
                        try bloody_spots.append(coord);
                    },
                    else => continue,
                }
            }
            if (bloody_spots.items.len >= 3) {
                // siren attack.
                for (clean_spots.items) |coord| {
                    // spawn siren
                    try self.state.individuals.putNoClobber(
                        self.findAvailableId(),
                        try (Individual{
                            .species = .{ .siren = .water },
                            .abs_position = .{ .small = coord },
                            .perceived_origin = coord,
                        }).clone(self.allocator),
                    );
                }
                // corrupt the water.
                it = (Rect{
                    .x = center.x - 2,
                    .y = center.y - 2,
                    .width = 5,
                    .height = 5,
                }).rowMajorIterator();
                while (it.next()) |coord| {
                    const cell = self.state.terrain.getCoord(coord);
                    switch (cell.floor) {
                        .water, .water_bloody => {
                            self.state.terrain.getExistingCoord(coord).floor = .water_deep;
                        },
                        else => {},
                    }
                }
            }
        }
    }

    // Traps
    var intended_polymorphs = IdMap(Species).init(self.allocator);
    for (hc.everybody) |id| {
        const individual = self.state.individuals.get(id).?;
        const position = individual.abs_position;
        const from_species = individual.species;
        var dest_species: ?Species = null;
        switch (from_species) {
            .siren => |subspecies| switch (subspecies) {
                .water => {
                    if (!isWet(self.state.terrain.getCoord(position.small).floor)) {
                        dest_species = .{ .siren = .land };
                        core.debug.engine.print("transforming siren into land form.", .{});
                    }
                },
                .land => {
                    if (isWet(self.state.terrain.getCoord(position.small).floor)) {
                        dest_species = .{ .siren = .water };
                        core.debug.engine.print("transforming siren into water form.", .{});
                    }
                },
            },
            else => {},
        }
        switch (position) {
            .small => |coord| {
                switch (self.state.terrain.getCoord(coord).wall) {
                    .polymorph_trap_centaur => {
                        dest_species = .{ .centaur = .archer };
                    },
                    .polymorph_trap_kangaroo => {
                        dest_species = .kangaroo;
                    },
                    .polymorph_trap_turtle => {
                        dest_species = .turtle;
                    },
                    .polymorph_trap_blob => {
                        dest_species = Species{ .blob = .small_blob };
                    },
                    .polymorph_trap_human => {
                        dest_species = .human;
                    },
                    else => {},
                }
            },
            .large => |coords| {
                if (coords[0].y == coords[1].y) {
                    // west-east aligned
                    var ordered_coords: [2]Coord = undefined;
                    if (coords[0].x < coords[1].x) {
                        ordered_coords[0] = coords[0];
                        ordered_coords[1] = coords[1];
                    } else {
                        ordered_coords[0] = coords[1];
                        ordered_coords[1] = coords[0];
                    }
                    const ordered_walls = [_]Wall{
                        self.state.terrain.getCoord(ordered_coords[0]).wall,
                        self.state.terrain.getCoord(ordered_coords[1]).wall,
                    };

                    if (ordered_walls[0] == .polymorph_trap_rhino_west and //
                        ordered_walls[1] == .polymorph_trap_rhino_east)
                    {
                        dest_species = .rhino;
                    }
                    if (ordered_walls[0] == .polymorph_trap_blob_west and //
                        ordered_walls[1] == .polymorph_trap_blob_east)
                    {
                        dest_species = Species{ .blob = .small_blob };
                    }
                } else {
                    // No north-south aligned traps exist.
                }
            },
        }
        if (dest_species) |species| {
            try intended_polymorphs.putNoClobber(id, species);
        }
    }
    if (intended_polymorphs.count() > 0) {
        var non_blob_individuals_to_be_present = CoordMap(u2).init(self.allocator);

        for (hc.everybody) |id| {
            const individual = self.state.individuals.get(id).?;
            const from_species = individual.species;
            const position = individual.abs_position;
            if (from_species != .blob) {
                for (getAllPositions(&position)) |coord| {
                    const gop = try non_blob_individuals_to_be_present.getOrPut(coord);
                    if (gop.found_existing) {
                        gop.value_ptr.* +|= 1;
                    } else {
                        gop.value_ptr.* = 1;
                    }
                }
            }
            const dest_species = intended_polymorphs.get(id) orelse continue;
            if (from_species == .blob and dest_species != .blob) {
                for (getAllPositions(&position)) |coord| {
                    const gop = try non_blob_individuals_to_be_present.getOrPut(coord);
                    if (gop.found_existing) {
                        gop.value_ptr.* +|= 1;
                    } else {
                        gop.value_ptr.* = 1;
                    }
                }
            }
        }
        for (hc.everybody) |id| {
            const individual = self.state.individuals.get(id).?;
            const dest_species = intended_polymorphs.get(id) orelse continue;
            const from_species = individual.species;
            var same_species = from_species == @as(std.meta.Tag(Species), dest_species);
            if (same_species) {
                switch (from_species) {
                    .siren => |subspecies| {
                        if (subspecies != dest_species.siren) {
                            same_species = false;
                        }
                    },
                    else => {},
                }
                if (same_species) {
                    core.debug.engine.print("polymorph fails: already that species.", .{});
                    continue;
                }
            }
            for (getAllPositions(&individual.abs_position)) |coord| {
                if (non_blob_individuals_to_be_present.get(coord).? > 1) {
                    core.debug.engine.print("polymorph fails: too croweded.", .{});
                    break;
                }
            } else {
                // The polymoph succeeds.
                self.state.individuals.get(id).?.species = dest_species;
                try hc.polymorphers.put(id, {});
                const blob_only_statuses = core.protocol.StatusCondition_grappling | //
                    core.protocol.StatusCondition_digesting;
                const blob_immune_statuses = core.protocol.StatusCondition_grappled | //
                    core.protocol.StatusCondition_being_digested | //
                    core.protocol.StatusCondition_wounded_leg | //
                    core.protocol.StatusCondition_limping | //
                    core.protocol.StatusCondition_malaise | //
                    core.protocol.StatusCondition_pain;
                if (dest_species == .blob) {
                    individual.status_conditions &= ~blob_immune_statuses;
                } else {
                    individual.status_conditions &= ~blob_only_statuses;
                }
            }
        }
    }
    if (hc.polymorphers.count() != 0) {
        for (hc.everybody) |id| {
            try observeFrame(
                self,
                id,
                hc.individual_to_perception.get(id).?,
                Activities{ .polymorphs = &hc.polymorphers },
            );
        }
    }

    // Check for completing and starting the next level.
    // We push these changes directly into the state diff after all the above is resolved,
    // because these changes don't affect any of the above logic.
    var open_the_way = false;
    var button_getting_pressed: ?Coord = null;
    if (self.state.individuals.count() - hc.total_deaths.count() == 1) {
        // Only one person left.
        if (hc.total_deaths.count() > 0) {
            // Freshly completed level.
            open_the_way = true;
        }
        // check for someone on the button
        for (hc.everybody) |id| {
            const position = self.state.individuals.get(id).?.abs_position;
            for (getAllPositions(&position)) |coord| {
                if (self.state.terrain.getCoord(coord).floor == .hatch) {
                    button_getting_pressed = coord;
                    break;
                }
            }
        }
    }
    if (open_the_way) {
        if (getLevelTransitionBoundingBox(self.state.level_number)) |level_transition_bounding_box| {
            // open the way
            var x: i32 = level_transition_bounding_box.x;
            find_the_doors: while (x < level_transition_bounding_box.x + level_transition_bounding_box.width) : (x += 1) {
                var y: i32 = level_transition_bounding_box.y;
                while (y < level_transition_bounding_box.y + level_transition_bounding_box.height) : (y += 1) {
                    const cell = try self.state.terrain.getOrPut(x, y);
                    if (cell.wall == .dirt) {
                        cell.* = .{
                            .floor = .marble,
                            .wall = .air,
                        };
                    } else if (cell.floor == .hatch) {
                        // only open doors until the next hatch
                        break :find_the_doors;
                    }
                }
            }
        }
    }

    if (button_getting_pressed) |button_coord| button_blk: {
        if (self.state.level_number + 1 < the_levels.len) {
            self.state.level_number += 1;
        } else {
            // Can't advance.
            break :button_blk;
        }
        // close any open paths
        if (getLevelTransitionBoundingBox(self.state.level_number - 1)) |level_transition_bounding_box| {
            var x: i32 = level_transition_bounding_box.x;
            while (x < level_transition_bounding_box.x + level_transition_bounding_box.width) : (x += 1) {
                var y: i32 = level_transition_bounding_box.y;
                while (y < level_transition_bounding_box.y + level_transition_bounding_box.height) : (y += 1) {
                    const cell = try self.state.terrain.getOrPut(x, y);
                    if (cell.floor == .marble) {
                        cell.* = .{
                            .floor = .unknown,
                            .wall = .stone,
                        };
                    }
                }
            }
        }

        // destroy the button
        try self.state.terrain.putCoord(button_coord, TerrainSpace{
            .floor = .dirt,
            .wall = .air,
        });

        // spawn enemies
        var level_x: u16 = 0;
        for (the_levels[0..self.state.level_number]) |level| {
            level_x += level.width;
        }
        for (the_levels[self.state.level_number].individuals) |_individual| {
            var individual = _individual;
            individual.abs_position = offsetPosition(individual.abs_position, makeCoord(level_x, 0));
            try self.state.individuals.putNoClobber(self.findAvailableId(), try individual.clone(self.allocator));
        }
    }

    // final observations
    for (hc.total_deaths.keys()) |id| {
        assert(self.state.individuals.swapRemove(id));
    }

    for (hc.everybody) |id| {
        try observeFrame(
            self,
            id,
            hc.individual_to_perception.get(id).?,
            Activities.static_state,
        );
    }

    return hc.individual_to_perception;
}

fn doMovementAndCollisions(
    self: *GameEngine,
    everybody: *[]u32,
    individual_to_perception: *IdMap(*MutablePerceivedHappening),
    intended_moves: *IdMap(Coord),
    budges_at_all: *IdMap(void),
    total_deaths: *IdMap(void),
) !void {
    var next_positions = IdMap(ThingPosition).init(self.allocator);

    for (everybody.*) |id| {
        // seek forward and stop at any wall.
        const position = self.state.individuals.get(id).?.abs_position;
        const initial_head_coord = getHeadPosition(position);
        const move_delta = intended_moves.get(id) orelse continue;
        const move_unit = move_delta.signumed();
        const move_magnitude = move_delta.magnitudeDiag();
        var distance: i32 = 1;
        while (true) : (distance += 1) {
            const new_head_coord = initial_head_coord.plus(move_unit.scaled(distance));
            if (!isOpenSpace(self.state.terrain.getCoord(new_head_coord).wall)) {
                // bonk
                distance -= 1;
                break;
            }
            if (distance >= move_magnitude)
                break;
        }
        if (distance == 0) {
            // no move for you
            continue;
        }
        // found an open space
        const adjusted_move_delta = move_unit.scaled(distance);
        _ = intended_moves.put(id, adjusted_move_delta) catch unreachable;
        try next_positions.putNoClobber(id, applyMovementToPosition(position, adjusted_move_delta));
    }

    var trample_deaths = IdMap(void).init(self.allocator);

    // Collision detection and resolution.
    {
        const Collision = struct {
            /// this has at most one entry
            stationary_id: ?u32 = null,
            cardinal_index_to_enterer: [4]?u32 = [_]?u32{null} ** 4,
            cardinal_index_to_fast_enterer: [4]?u32 = [_]?u32{null} ** 4,
            winner_id: ?u32 = null,
        };

        // Collect collision information.
        var coord_to_collision = [4]CoordMap(Collision){
            undefined, // no physics in layer 0.
            CoordMap(Collision).init(self.allocator),
            CoordMap(Collision).init(self.allocator),
            CoordMap(Collision).init(self.allocator),
        };
        for (everybody.*) |id| {
            const individual = self.state.individuals.get(id).?;
            const physics_layer = getPhysicsLayer(individual.species);
            if (physics_layer == 0) {
                // no collisions at all.
                continue;
            }
            const old_position = individual.abs_position;
            const new_position = next_positions.get(id) orelse old_position;
            for (getAllPositions(&new_position), 0..) |new_coord, i| {
                const old_coord = getAllPositions(&old_position)[i];
                const delta = new_coord.minus(old_coord);
                const gop = try coord_to_collision[physics_layer].getOrPut(new_coord);
                if (!gop.found_existing) {
                    gop.value_ptr.* = Collision{};
                }
                var collision = gop.value_ptr;
                if (delta.equals(zero_vector)) {
                    collision.stationary_id = id;
                } else if (isOrthogonalUnitVector(delta)) {
                    collision.cardinal_index_to_enterer[@intFromEnum(deltaToCardinalDirection(delta))] = id;
                } else if (isOrthogonalVectorOfMagnitude(delta, 2)) {
                    collision.cardinal_index_to_fast_enterer[@intFromEnum(deltaToCardinalDirection(delta.scaledDivTrunc(2)))] = id;
                } else unreachable;
            }
        }

        // Determine who wins each collision in each layer.
        for ([_]u2{ 3, 2, 1 }) |physics_layer| {
            for (coord_to_collision[physics_layer].values()) |*collision| {
                var incoming_vector_set: u9 = 0;
                if (collision.stationary_id != null) incoming_vector_set |= 1 << 0;
                for (collision.cardinal_index_to_enterer, 0..) |maybe_id, i| {
                    if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (1 + @as(u4, @as(u2, @intCast(i))));
                }
                for (collision.cardinal_index_to_fast_enterer, 0..) |maybe_id, i| {
                    if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (5 + @as(u4, @as(u2, @intCast(i))));
                }
                if (incoming_vector_set & 1 != 0) {
                    // Stationary entities always win.
                    collision.winner_id = collision.stationary_id;
                } else if (cardinalIndexBitSetToCollisionWinnerIndex(@as(u4, @intCast((incoming_vector_set & 0b111100000) >> 5)))) |index| {
                    // fast bois beat slow bois.
                    collision.winner_id = collision.cardinal_index_to_fast_enterer[index].?;
                } else if (cardinalIndexBitSetToCollisionWinnerIndex(@as(u4, @intCast((incoming_vector_set & 0b11110) >> 1)))) |index| {
                    // a slow boi wins.
                    collision.winner_id = collision.cardinal_index_to_enterer[index].?;
                } else {
                    // nobody wins. winner stays null.
                }
            }
        }
        id_loop: for (everybody.*) |id| {
            const next_position = next_positions.get(id) orelse continue;
            const physics_layer = getPhysicsLayer(self.state.individuals.get(id).?.species);
            if (physics_layer == 0) continue;
            for (getAllPositions(&next_position)) |coord| {
                const collision = coord_to_collision[physics_layer].get(coord).?;
                if (collision.winner_id != id) {
                    // i lose.
                    assert(next_positions.swapRemove(id));
                    continue :id_loop;
                }
            }
        }

        // Check for unsuccessful conga lines.
        // for example:
        //   🐕 -> 🐕
        //         V
        //   🐕 <- 🐕
        // This conga line would be unsuccessful because the head of the line isn't successfully moving.
        for (everybody.*) |id| {
            // anyone who fails to move could be the head of a sad conga line.
            if (next_positions.contains(id)) continue;
            // You are not moving (successfully).
            var head_id = id;
            conga_loop: while (true) {
                const head_individual = self.state.individuals.get(head_id).?;
                const head_physics_layer = getPhysicsLayer(head_individual.species);
                if (head_physics_layer == 0) {
                    // Don't let me stop the party.
                    break :conga_loop;
                }
                for (getAllPositions(&head_individual.abs_position)) |coord| {
                    const follower_id = (coord_to_collision[head_physics_layer].fetchSwapRemove(coord) orelse continue).value.winner_id orelse continue;
                    if (follower_id == head_id) continue; // your tail is always allowed to follow your head.
                    // conga line is botched.
                    _ = next_positions.swapRemove(follower_id);
                    // and now you get to pass on the bad news.
                    head_id = follower_id;
                    continue :conga_loop;
                }
                // no one wants to move into this space.
                break;
            }
        }
    }

    // Observe movement.
    for (everybody.*) |id| {
        try observeFrame(
            self,
            id,
            individual_to_perception.get(id).?,
            Activities{
                .movement = .{
                    .intended_moves = intended_moves,
                    .next_positions = &next_positions,
                },
            },
        );
    }

    // Update positions from movement.
    for (everybody.*) |id| {
        const next_position = next_positions.get(id) orelse continue;
        self.state.individuals.get(id).?.abs_position = next_position;
    }

    for (everybody.*) |id| {
        // TODO: re-implement trample deaths.
        // trample deaths got removed when physics layers were introduced.
        try observeFrame(
            self,
            id,
            individual_to_perception.get(id).?,
            Activities{ .deaths = &trample_deaths },
        );
    }
    try self.flushDeaths(total_deaths, &trample_deaths, everybody);

    for (intended_moves.keys()) |id| {
        _ = try budges_at_all.put(id, {});
    }
}

const Activities = union(enum) {
    static_state,
    movement: Movement,
    growths: Movement,
    shrinks: *const IdMap(u1),
    kicks: *const IdMap(Coord),
    attacks: AttackishActivities,
    polymorphs: *const IdMap(void),

    deaths: *const IdMap(void),

    const Movement = struct {
        intended_moves: *const IdMap(Coord),
        next_positions: *const IdMap(ThingPosition),
    };
    const AttackishActivities = struct {
        attacks: *const IdMap(Attack),
        defends: *const IdMap(CardinalDirection),
        nibbles: *const IdMap(void),
        stomps: *const IdMap(void),
    };
    const Attack = struct {
        direction: Coord,
        distance: i32,
    };
};
fn observeFrame(
    self: *GameEngine,
    my_id: u32,
    perception: *MutablePerceivedHappening,
    activities: Activities,
) !void {
    try perception.frames.append(try self.getPerceivedFrame(
        my_id,
        activities,
    ));
}

fn getPerceivedFrame(
    self: *GameEngine,
    my_id: u32,
    activities: Activities,
) !PerceivedFrame {
    const actual_me = self.state.individuals.get(my_id).?;
    const my_abs_position = actual_me.abs_position;
    const my_head_coord = getHeadPosition(my_abs_position);

    const view_distance = getViewDistance(actual_me.species);
    var view_bounding_box = Rect{
        .x = my_head_coord.x - view_distance,
        .y = my_head_coord.y - view_distance,
        .width = view_distance * 2 + 1,
        .height = view_distance * 2 + 1,
    };
    if (actual_me.species == .blob) {
        switch (actual_me.abs_position) {
            .large => |data| {
                // Blobs can also see with their butts.
                // Grow the bounding box by 1 in the tail direction.
                if (data[1].x < data[0].x) {
                    view_bounding_box.x -= 1;
                    view_bounding_box.width += 1;
                } else if (data[1].x > data[0].x) {
                    view_bounding_box.width += 1;
                } else if (data[1].y < data[0].y) {
                    view_bounding_box.y -= 1;
                    view_bounding_box.height += 1;
                } else if (data[1].y > data[0].y) {
                    view_bounding_box.height += 1;
                } else unreachable;
            },
            else => {},
        }
    }

    var seen_terrain = PerceivedTerrain.init(self.allocator);
    var in_view_matrix = core.matrix.SparseChunkedMatrix(bool, false, .{}).init(self.allocator);

    {
        var it = view_bounding_box.rowMajorIterator();
        while (it.next()) |cursor| {
            if (!isClearLineOfSight(&self.state.terrain, my_head_coord, cursor)) continue;
            try in_view_matrix.putCoord(cursor, true);

            var seen_cell: TerrainSpace = self.state.terrain.getCoord(cursor);
            if (actual_me.species == .blob) {
                // blobs are blind.
                if (seen_cell.floor == .lava and cursor.equals(my_head_coord)) {
                    // Blobs can see lava if they're right on top of it. Also RIP.
                } else {
                    seen_cell = if (isOpenSpace(seen_cell.wall))
                        TerrainSpace{ .floor = .unknown_floor, .wall = .air }
                    else
                        TerrainSpace{ .floor = .unknown, .wall = .unknown_wall };
                }
            }
            // Don't spoil trap behavior.
            switch (seen_cell.wall) {
                .polymorph_trap_centaur, .polymorph_trap_kangaroo, .polymorph_trap_turtle, .polymorph_trap_blob => {
                    seen_cell.wall = .unknown_polymorph_trap;
                },
                .polymorph_trap_rhino_west, .polymorph_trap_blob_west => {
                    seen_cell.wall = .unknown_polymorph_trap_west;
                },
                .polymorph_trap_rhino_east, .polymorph_trap_blob_east => {
                    seen_cell.wall = .unknown_polymorph_trap_east;
                },
                else => {},
            }
            try seen_terrain.putCoord(cursor, seen_cell);
        }
    }

    const perceived_origin = actual_me.perceived_origin;
    var perceived_self: ?PerceivedThing = null;
    var others = ArrayList(PerceivedThing).init(self.allocator);

    for (self.state.individuals.keys()) |id| {
        const activity = switch (activities) {
            .movement => |data| if (data.intended_moves.get(id)) |move_delta|
                if (data.next_positions.contains(id))
                    PerceivedActivity{ .movement = move_delta }
                else
                    PerceivedActivity{ .failed_movement = move_delta }
            else
                PerceivedActivity{ .none = {} },

            .shrinks => |data| if (data.get(id)) |index|
                PerceivedActivity{ .shrink = index }
            else
                PerceivedActivity{ .none = {} },

            .growths => |data| if (data.intended_moves.get(id)) |move_delta|
                if (data.next_positions.contains(id))
                    PerceivedActivity{ .growth = move_delta }
                else
                    PerceivedActivity{ .failed_growth = move_delta }
            else
                PerceivedActivity{ .none = {} },

            .attacks => |data| if (data.attacks.get(id)) |attack|
                PerceivedActivity{
                    .attack = PerceivedActivity.Attack{
                        .direction = attack.direction,
                        .distance = attack.distance,
                    },
                }
            else if (data.defends.get(id)) |direction|
                PerceivedActivity{ .defend = direction }
            else if (data.nibbles.contains(id))
                PerceivedActivity{ .nibble = {} }
            else if (data.stomps.contains(id))
                PerceivedActivity{ .stomp = {} }
            else
                PerceivedActivity{ .none = {} },

            .kicks => |data| if (data.get(id)) |coord|
                PerceivedActivity{ .kick = coord }
            else
                PerceivedActivity{ .none = {} },

            .polymorphs => |data| if (data.contains(id))
                PerceivedActivity{ .polymorph = {} }
            else
                PerceivedActivity{ .none = {} },

            .deaths => |data| if (data.get(id)) |_|
                PerceivedActivity{ .death = {} }
            else
                PerceivedActivity{ .none = {} },

            .static_state => PerceivedActivity{ .none = {} },
        };

        const actual_thing = self.state.individuals.get(id).?;
        const abs_position = actual_thing.abs_position;
        // if any position is within view, we can see all of it.
        var within_view = false;
        for (getAllPositions(&abs_position)) |coord| {
            if (in_view_matrix.getCoord(coord)) {
                within_view = true;
                break;
            }
        }
        if (!within_view) continue;

        const rel_position = offsetPosition(abs_position, perceived_origin.scaled(-1));
        const thing = PerceivedThing{
            .position = rel_position,
            .kind = .{
                .individual = .{
                    .species = actual_thing.species,
                    .status_conditions = actual_thing.status_conditions,
                    .equipment = getEquipment(self.state, id),
                    .activity = activity,
                },
            },
        };
        if (id == my_id) {
            perceived_self = thing;
        } else {
            try others.append(thing);
        }
    }

    for (self.state.items.values()) |item| {
        switch (item.location) {
            .floor_coord => |coord| {
                if (!in_view_matrix.getCoord(coord)) continue;
                try others.append(PerceivedThing{
                    .position = .{ .small = coord.minus(perceived_origin) },
                    .kind = .{ .item = itemToFloorItem(item) },
                });
            },
            .held => {}, // handled above
        }
    }

    var terrain_chunk = try self.exportTerrain(&seen_terrain);
    terrain_chunk.position = terrain_chunk.position.minus(perceived_origin);

    return PerceivedFrame{
        .self = perceived_self.?,
        .others = try others.toOwnedSlice(),
        .terrain = terrain_chunk,
        .completed_levels = self.state.level_number,
    };
}

fn exportTerrain(self: *GameEngine, terrain: *PerceivedTerrain) !TerrainChunk {
    const width = @as(u16, @intCast(terrain.metrics.max_x - terrain.metrics.min_x + 1));
    const height = @as(u16, @intCast(terrain.metrics.max_y - terrain.metrics.min_y + 1));
    var terrain_chunk = core.protocol.TerrainChunk{
        .position = makeCoord(terrain.metrics.min_x, terrain.metrics.min_y),
        .width = width,
        .height = height,
        .matrix = try self.allocator.alloc(TerrainSpace, width * height),
    };

    var y = terrain.metrics.min_y;
    while (y <= terrain.metrics.max_y) : (y += 1) {
        const inner_y = @as(u16, @intCast(y - terrain.metrics.min_y));
        var x = terrain.metrics.min_x;
        while (x <= terrain.metrics.max_x) : (x += 1) {
            const inner_x = @as(u16, @intCast(x - terrain.metrics.min_x));
            terrain_chunk.matrix[inner_y * width + inner_x] = terrain.get(x, y);
        }
    }

    return terrain_chunk;
}

const MutablePerceivedHappening = struct {
    frames: ArrayList(PerceivedFrame),
    pub fn init(allocator: Allocator) MutablePerceivedHappening {
        return MutablePerceivedHappening{
            .frames = ArrayList(PerceivedFrame).init(allocator),
        };
    }
};

/// write local deaths to total deaths, and drop items.
fn flushDeaths(self: *GameEngine, total_deaths: *IdMap(void), local_deaths: *IdMap(void), everybody: *[]u32) !void {
    for (local_deaths.keys()) |id| {
        if (null != try total_deaths.fetchPut(id, {})) continue;
        // remove from everybody
        {
            const index = std.mem.indexOfScalar(u32, everybody.*, id).?;
            everybody.*[index] = everybody.*[everybody.len - 1];
            everybody.* = everybody.*[0 .. everybody.len - 1];
        }
        // drop items
        const coord = getHeadPosition(self.state.individuals.get(id).?.abs_position);
        var it = inventoryIterator(self.state, id);
        while (it.next()) |entry| {
            const item = entry.value_ptr.*;
            item.location = .{ .floor_coord = coord };
        }
    }
    local_deaths.clearRetainingCapacity();
}

fn doAttackDamage(attack_function: core.game_logic.AttackFunction, other_id: u32, other_species: Species, other_status_conditions: *StatusConditions, attack_deaths: *IdMap(void)) !void {
    switch (attack_function) {
        .wound_then_kill => {
            if (other_status_conditions.* & core.protocol.StatusCondition_wounded_leg == 0 and //
                !core.game_logic.woundThenKillGoesRightToKill(other_species))
            {
                // first hit is a wound
                other_status_conditions.* |= core.protocol.StatusCondition_wounded_leg;
            } else {
                // second hit. you ded.
                _ = try attack_deaths.put(other_id, {});
            }
        },
        .just_wound => {
            other_status_conditions.* |= core.protocol.StatusCondition_wounded_leg;
        },
        .malaise => {
            other_status_conditions.* |= core.protocol.StatusCondition_malaise;
        },
        .smash => {
            _ = try attack_deaths.put(other_id, {});
        },
        .chop => {
            _ = try attack_deaths.put(other_id, {});
        },
        .burn => unreachable, // Handled in getAttackEffect().
    }
}

/// See geometry for cardinal index definition.
/// e.g. 0b0001: just right, 0b1100: left and up.
/// Note that the directions are the movement directions, not the "from" directions.
/// Returning null means no winner.
fn cardinalIndexBitSetToCollisionWinnerIndex(cardinal_index_bit_set: u4) ?u2 {
    switch (cardinal_index_bit_set) {
        // nobody here.
        0b0000 => return null,
        // only one person
        0b0001 => return 0,
        0b0010 => return 1,
        0b0100 => return 2,
        0b1000 => return 3,
        // 2-way head-on collision
        0b0101 => return null,
        0b1010 => return null,
        // 2-way right-angle collision.
        // give right of way to the one on the right.
        0b0011 => return 0,
        0b0110 => return 1,
        0b1100 => return 2,
        0b1001 => return 3,
        // 3-way collision. the middle one wins.
        0b1011 => return 0,
        0b0111 => return 1,
        0b1110 => return 2,
        0b1101 => return 3,
        // 4-way collision
        0b1111 => return null,
    }
}

fn getLevelTransitionBoundingBox(current_level_number: usize) ?Rect {
    if (current_level_number + 1 >= the_levels.len) return null;

    var x: u16 = 0;
    for (the_levels[0..current_level_number]) |level| {
        x += level.width;
    }
    const width = //
        the_levels[current_level_number].width + //
        the_levels[current_level_number + 1].width;
    const height = @max(
        the_levels[current_level_number].width,
        the_levels[current_level_number + 1].width,
    );

    return Rect{
        .x = x,
        .y = 0,
        .width = width,
        .height = height,
    };
}

fn isClearLineOfSight(terrain: *Terrain, a: Coord, b: Coord) bool {
    return isClearLineOfSightOneSided(terrain, a, b) or isClearLineOfSightOneSided(terrain, b, a);
}

fn isClearLineOfSightOneSided(terrain: *Terrain, a: Coord, b: Coord) bool {
    const delta = b.minus(a);
    const should_print = false;
    if (should_print) core.debug.testing.print("los: {},{} => {},{}", .{ a.x, a.y, b.x, b.y });
    const abs_delta = delta.abs();

    if (abs_delta.x > abs_delta.y) {
        // Iterate along the x axis.
        const step_x = sign(delta.x);
        var cursor_x = a.x + step_x;
        while (cursor_x != b.x) : (cursor_x += step_x) {
            const y = @divTrunc((cursor_x - a.x) * delta.y, delta.x) + a.y;
            const is_open = isTransparentSpace(terrain.get(cursor_x, y).wall);
            if (should_print) core.debug.testing.print("x,y: {},{}: {}", .{ cursor_x, y, is_open });
            if (!is_open) return false;
        }
    } else {
        // Iterate along the y axis.
        const step_y = sign(delta.y);
        var cursor_y = a.y + step_y;
        while (cursor_y != b.y) : (cursor_y += step_y) {
            const x = @divTrunc((cursor_y - a.y) * delta.x, delta.y) + a.x;
            const is_open = isTransparentSpace(terrain.get(x, cursor_y).wall);
            if (should_print) core.debug.testing.print("x,y: {},{}: {}", .{ x, cursor_y, is_open });
            if (!is_open) return false;
        }
    }
    return true;
}

fn findAvailableId(self: *GameEngine) u32 {
    var cursor: u32 = @as(u32, @intCast(self.state.individuals.count()));
    while (self.state.individuals.contains(cursor)) {
        cursor += 1;
    }
    return cursor;
}

pub fn getEquipment(game_state: *GameState, id: u32) Equipment {
    var equipment: Equipment = .{};
    var it = inventoryIterator(game_state, id);
    while (it.next()) |entry| {
        const item = entry.value_ptr.*;
        const is_equipped = item.location.held.equipped_to_slot != .none;
        switch (item.kind) {
            .shield => {
                equipment.set(.shield, true, is_equipped);
            },
            .axe => {
                equipment.set(.axe, true, is_equipped);
            },
            .torch => {
                equipment.set(.torch, true, is_equipped);
            },
            .dagger => {
                equipment.set(.dagger, true, is_equipped);
            },
        }
    }
    return equipment;
}

const InventoryIterator = struct {
    sub_it: IdMap(*Item).Iterator,
    holder_id: u32,
    pub fn next(self: *@This()) ?IdMap(*Item).Entry {
        while (self.sub_it.next()) |entry| {
            const item = entry.value_ptr.*;
            switch (item.location) {
                .floor_coord => {},
                .held => |held| {
                    if (held.holder_id == self.holder_id) return entry;
                },
            }
        }
        return null;
    }
};
fn inventoryIterator(game_state: *GameState, holder_id: u32) InventoryIterator {
    return .{
        .sub_it = game_state.items.iterator(),
        .holder_id = holder_id,
    };
}

fn itemToEquippedItem(item: *const Item) core.protocol.EquippedItem {
    return switch (item.kind) {
        .shield => .shield,
        .axe => .axe,
        .torch => .torch,
        .dagger => .dagger,
    };
}
fn itemToFloorItem(item: *const Item) core.protocol.FloorItem {
    return switch (item.kind) {
        .shield => .shield,
        .axe => .axe,
        .torch => .torch,
        .dagger => .dagger,
    };
}
fn itemToEquipmentSlot(item: *const Item) EquipmentSlot {
    return switch (item.kind) {
        .dagger, .axe, .torch => .right_hand,
        .shield => .left_hand,
    };
}
