const std = @import("std");
const assert = std.debug.assert;
const ArrayList = std.ArrayList;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const isScaledCardinalDirection = core.geometry.isScaledCardinalDirection;
const directionToCardinalIndex = core.geometry.directionToCardinalIndex;
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
const StatusConditions = core.protocol.StatusConditions;

const getViewDistance = core.game_logic.getViewDistance;
const isOpenSpace = core.game_logic.isOpenSpace;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const applyMovementToPosition = core.game_logic.applyMovementToPosition;
const getInertiaIndex = core.game_logic.getInertiaIndex;

const game_model = @import("./game_model.zig");
const GameState = game_model.GameState;
const Individual = game_model.Individual;
const StateDiff = game_model.StateDiff;
const IdMap = game_model.IdMap;
const CoordMap = game_model.CoordMap;
const Terrain = game_model.Terrain;
const oob_terrain = game_model.oob_terrain;

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

fn findAvailableId(cursor: *u32, usedIds: IdMap(*Individual)) u32 {
    while (usedIds.contains(cursor.*)) {
        cursor.* += 1;
    }
    defer cursor.* += 1;
    return cursor.*;
}

pub const GameEngine = struct {
    allocator: *std.mem.Allocator,

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{
            .allocator = allocator,
        };
    }

    pub const Happenings = struct {
        individual_to_perception: IdMap([]PerceivedFrame),
        state_changes: []StateDiff,
    };

    /// Computes what would happen to the state of the game.
    /// This is the entry point for all game rules.
    pub fn computeHappenings(self: *const GameEngine, game_state: *GameState, actions: IdMap(Action)) !Happenings {
        // cache the set of keys so iterator is easier.
        var everybody = try self.allocator.alloc(u32, game_state.individuals.count());
        for (everybody) |*x, i| {
            x.* = game_state.individuals.keys()[i];
        }
        const everybody_including_dead = try std.mem.dupe(self.allocator, u32, everybody);

        var budges_at_all = IdMap(void).init(self.allocator);

        var individual_to_perception = IdMap(*MutablePerceivedHappening).init(self.allocator);
        for (everybody_including_dead) |id| {
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, MutablePerceivedHappening));
        }

        var current_positions = IdMap(ThingPosition).init(self.allocator);
        for (everybody) |id| {
            try current_positions.putNoClobber(id, game_state.individuals.get(id).?.abs_position);
        }

        var current_status_conditions = IdMap(StatusConditions).init(self.allocator);
        for (everybody) |id| {
            try current_status_conditions.putNoClobber(id, game_state.individuals.get(id).?.status_conditions);
        }

        var total_deaths = IdMap(void).init(self.allocator);

        var polymorphs = IdMap(Species).init(self.allocator);

        // Shrinking
        {
            var shrinks = IdMap(u1).init(self.allocator);
            var next_positions = IdMap(ThingPosition).init(self.allocator);
            for (everybody) |id| {
                const index: u1 = switch (actions.get(id).?) {
                    .shrink => |index| index,
                    else => continue,
                };
                const old_position = current_positions.get(id).?.large;
                try next_positions.putNoClobber(id, ThingPosition{ .small = old_position[index] });
                try shrinks.putNoClobber(id, index);
            }
            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.get(id).?,
                    &current_positions,
                    Activities{
                        .shrinks = &shrinks,
                    },
                );
            }
            for (everybody) |id| {
                const next_position = next_positions.get(id) orelse continue;
                current_positions.getEntry(id).?.value_ptr.* = next_position;
            }
        }

        // Moving normally
        {
            var intended_moves = IdMap(Coord).init(self.allocator);
            for (everybody) |id| {
                const move_delta: Coord = switch (actions.get(id).?) {
                    .move, .fast_move => |move_delta| move_delta,
                    else => continue,
                };
                if (0 != current_status_conditions.get(id).? & (core.protocol.StatusCondition_limping | core.protocol.StatusCondition_grappled)) {
                    // nope.avi
                    continue;
                }
                try intended_moves.putNoClobber(id, move_delta);
            }

            try self.doMovementAndCollisions(
                game_state,
                &everybody,
                &individual_to_perception,
                &current_positions,
                &intended_moves,
                &budges_at_all,
                &total_deaths,
            );
        }

        // Growing
        {
            var intended_moves = IdMap(Coord).init(self.allocator);
            var next_positions = IdMap(ThingPosition).init(self.allocator);
            for (everybody) |id| {
                const move_delta: Coord = switch (actions.get(id).?) {
                    .grow => |move_delta| move_delta,
                    else => continue,
                };
                try intended_moves.putNoClobber(id, move_delta);
                const old_coord = current_positions.get(id).?.small;
                const new_head_coord = old_coord.plus(move_delta);
                if (!isOpenSpace(game_state.terrainAt(new_head_coord).wall)) {
                    // that's a wall
                    continue;
                }
                try next_positions.putNoClobber(id, ThingPosition{ .large = .{ new_head_coord, old_coord } });
            }

            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.get(id).?,
                    &current_positions,
                    Activities{
                        .growths = .{
                            .intended_moves = &intended_moves,
                            .next_positions = &next_positions,
                        },
                    },
                );
            }

            for (everybody) |id| {
                const next_position = next_positions.get(id) orelse continue;
                current_positions.getEntry(id).?.value_ptr.* = next_position;
            }
        }

        // Kicks
        {
            var kicks = IdMap(Coord).init(self.allocator);
            var intended_moves = IdMap(Coord).init(self.allocator);
            var kicked_too_much = IdMap(void).init(self.allocator);
            for (everybody) |id| {
                const kick_direction: Coord = switch (actions.get(id).?) {
                    .kick => |direction| direction,
                    else => continue,
                };
                const attacker_coord = getHeadPosition(current_positions.get(id).?);
                const kick_position = attacker_coord.plus(kick_direction);
                for (everybody) |other_id| {
                    const position = current_positions.get(other_id).?;
                    for (getAllPositions(&position)) |coord| {
                        if (!coord.equals(kick_position)) continue;
                        // gotchya
                        if (getInertiaIndex(game_state.individuals.get(other_id).?.species) > 0) {
                            // Your kick is not stronk enough.
                            continue;
                        }
                        if (try intended_moves.fetchPut(other_id, kick_direction)) |_| {
                            // kicked multiple times at once!
                            _ = try kicked_too_much.put(other_id, {});
                        }
                    }
                }
                try kicks.putNoClobber(id, kick_direction);
            }
            if (kicks.count() > 0) {
                for (everybody) |id| {
                    try self.observeFrame(
                        game_state,
                        id,
                        individual_to_perception.get(id).?,
                        &current_positions,
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
                    game_state,
                    &everybody,
                    &individual_to_perception,
                    &current_positions,
                    &intended_moves,
                    &budges_at_all,
                    &total_deaths,
                );
            }
        }

        // Wounds and limping
        for (everybody) |id| {
            const status_conditions = current_status_conditions.getEntry(id).?.value_ptr;
            if (game_state.terrainAt(getHeadPosition(current_positions.get(id).?)).floor == .marble) {
                // this tile heals wounds for some reason.
                status_conditions.* &= ~(core.protocol.StatusCondition_wounded_leg | core.protocol.StatusCondition_limping);
            } else if (!budges_at_all.contains(id)) {
                // you held still, so you are free of any limping status.
                status_conditions.* &= ~core.protocol.StatusCondition_limping;
            } else if (0 != status_conditions.* & core.protocol.StatusCondition_wounded_leg) {
                // you moved while wounded. now you limp.
                status_conditions.* |= core.protocol.StatusCondition_limping;
            }
        }

        // Grapple and digestion
        {
            var grappler_to_victim_count = IdMap(u2).init(self.allocator);
            var victim_to_has_multiple_attackers = IdMap(bool).init(self.allocator);
            var victim_to_unique_attacker = IdMap(u32).init(self.allocator);
            for (everybody) |id| {
                const blob_individual = game_state.individuals.get(id).?;
                if (blob_individual.species != .blob) continue;
                const position = current_positions.get(id).?;
                for (everybody) |other_id| {
                    var position_index: usize = find_collision: for (getAllPositions(&position)) |coord| {
                        const other_position = current_positions.get(other_id).?;
                        for (getAllPositions(&other_position)) |other_coord, i| {
                            if (other_coord.equals(coord)) break :find_collision i;
                        }
                    } else continue;

                    // there's some overlap. But is the other target even affected?
                    const other_individual = game_state.individuals.get(other_id).?;
                    if (!core.game_logic.isAffectedByAttacks(other_individual.species, position_index)) {
                        // Too strong for the blob's attacks.
                        continue;
                    }

                    // I have you now.
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
            for (everybody) |id| {
                const status_conditions = current_status_conditions.getEntry(id).?.value_ptr;
                if (!victim_to_has_multiple_attackers.contains(id)) {
                    // Not grappled.
                    status_conditions.* &= ~core.protocol.StatusCondition_grappled;
                    if (0 != status_conditions.* & core.protocol.StatusCondition_being_digested) {
                        // you've escaped digestion. the status effect turns into a leg wound i guess.
                        status_conditions.* &= ~core.protocol.StatusCondition_being_digested;
                        status_conditions.* |= core.protocol.StatusCondition_wounded_leg;
                    }
                } else {
                    // All victims are grappled at least.
                    status_conditions.* |= core.protocol.StatusCondition_grappled;
                    if (victim_to_unique_attacker.get(id)) |attacker_id| {
                        // Need a sufficient density of blob on this space to do a digestion.
                        const blob_subpecies = game_state.individuals.get(attacker_id).?.species.blob;
                        const can_digest = switch (blob_subpecies) {
                            .large_blob => true,
                            .small_blob => current_positions.get(attacker_id).? == .small,
                        };
                        if (!can_digest) continue;
                        if (0 == status_conditions.* & core.protocol.StatusCondition_being_digested) {
                            // Start getting digested.
                            status_conditions.* |= core.protocol.StatusCondition_being_digested;
                            _ = try digesters.put(attacker_id, {});
                        } else {
                            // Complete the digestion
                            try digestion_deaths.put(id, {});
                            grappler_to_victim_count.getEntry(attacker_id).?.value_ptr.* -= 1;

                            if (blob_subpecies == .small_blob) {
                                // Grow up to be large.
                                try polymorphs.putNoClobber(attacker_id, Species{ .blob = .large_blob });
                            }
                        }
                    }
                }
            }
            for (everybody) |id| {
                const status_conditions = current_status_conditions.getEntry(id).?.value_ptr;
                if ((grappler_to_victim_count.get(id) orelse 0) > 0) {
                    status_conditions.* |= core.protocol.StatusCondition_grappling;
                } else {
                    status_conditions.* &= ~core.protocol.StatusCondition_grappling;
                }
                if (digesters.contains(id)) {
                    status_conditions.* |= core.protocol.StatusCondition_digesting;
                } else {
                    status_conditions.* &= ~core.protocol.StatusCondition_digesting;
                }
            }

            for (everybody) |id| {
                if (digestion_deaths.count() != 0) {
                    try self.observeFrame(
                        game_state,
                        id,
                        individual_to_perception.get(id).?,
                        &current_positions,
                        Activities{ .deaths = &digestion_deaths },
                    );
                }
            }
            try flushDeaths(&total_deaths, &digestion_deaths, &everybody);
        }

        // Attacks
        var attacks = IdMap(Activities.Attack).init(self.allocator);
        var attack_deaths = IdMap(void).init(self.allocator);
        for (everybody) |id| {
            var attack_direction: Coord = switch (actions.get(id).?) {
                .attack => |direction| direction,
                else => continue,
            };
            var attacker_coord = getHeadPosition(current_positions.get(id).?);
            var attack_distance: i32 = 1;
            const range = core.game_logic.getAttackRange(game_state.individuals.get(id).?.species);
            range_loop: while (attack_distance <= range) : (attack_distance += 1) {
                var damage_position = attacker_coord.plus(attack_direction.scaled(attack_distance));
                for (everybody) |other_id| {
                    const position = current_positions.get(other_id).?;
                    for (getAllPositions(&position)) |coord, i| {
                        if (!coord.equals(damage_position)) continue;
                        // hit something.
                        const other = game_state.individuals.get(other_id).?;
                        const is_effective = blk: {
                            // innate defense
                            if (!core.game_logic.isAffectedByAttacks(other.species, i)) break :blk false;
                            // shield blocks arrows
                            if (range > 1 and other.has_shield) break :blk false;
                            break :blk true;
                        };
                        if (is_effective) {
                            // get wrecked
                            const other_status_conditions = current_status_conditions.getEntry(other_id).?.value_ptr;
                            if (other_status_conditions.* & core.protocol.StatusCondition_wounded_leg == 0) {
                                // first hit is a wound
                                other_status_conditions.* |= core.protocol.StatusCondition_wounded_leg;
                            } else {
                                // second hit. you ded.
                                _ = try attack_deaths.put(other_id, {});
                            }
                        }
                        break :range_loop;
                    }
                }
            }
            try attacks.putNoClobber(id, Activities.Attack{
                .direction = attack_direction,
                .distance = attack_distance,
            });
        }
        // Lava
        for (everybody) |id| {
            const position = current_positions.get(id).?;
            for (getAllPositions(&position)) |coord| {
                if (game_state.terrainAt(coord).floor == .lava) {
                    _ = try attack_deaths.put(id, {});
                }
            }
        }
        // Perception of Attacks and Death
        for (everybody) |id| {
            if (attacks.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.get(id).?,
                    &current_positions,
                    Activities{ .attacks = &attacks },
                );
            }
            if (attack_deaths.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.get(id).?,
                    &current_positions,
                    Activities{ .deaths = &attack_deaths },
                );
            }
        }
        try flushDeaths(&total_deaths, &attack_deaths, &everybody);

        // Traps
        for (everybody) |id| {
            const position = current_positions.get(id).?;
            for (getAllPositions(&position)) |coord| {
                if (game_state.terrainAt(coord).wall == .centaur_transformer and
                    game_state.individuals.get(id).?.species != .blob)
                {
                    try polymorphs.putNoClobber(id, Species{ .blob = .small_blob });
                }
            }
        }
        if (polymorphs.count() != 0) {
            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.get(id).?,
                    &current_positions,
                    Activities{ .polymorphs = &polymorphs },
                );
            }
        }

        // build state changes
        var state_changes = ArrayList(StateDiff).init(self.allocator);
        for (everybody_including_dead) |id| {
            switch (game_state.individuals.get(id).?.abs_position) {
                .small => |coord| {
                    const from = coord;
                    switch (current_positions.get(id).?) {
                        .small => |to| {
                            if (to.equals(from)) continue;
                            const delta = to.minus(from);
                            try state_changes.append(StateDiff{
                                .small_move = .{
                                    .id = id,
                                    .coord = delta,
                                },
                            });
                        },
                        .large => |to| {
                            const delta = to[0].minus(from);
                            try state_changes.append(StateDiff{
                                .growth = .{
                                    .id = id,
                                    .coord = delta,
                                },
                            });
                        },
                    }
                },
                .large => |coords| {
                    const from = coords;
                    switch (current_positions.get(id).?) {
                        .large => |to| {
                            if (to[0].equals(from[0]) and to[1].equals(from[1])) continue;
                            try state_changes.append(StateDiff{
                                .large_move = .{
                                    .id = id,
                                    .coords = .{
                                        to[0].minus(from[0]),
                                        to[1].minus(from[1]),
                                    },
                                },
                            });
                        },
                        .small => |to| {
                            if (to.equals(from[0])) {
                                try state_changes.append(StateDiff{
                                    .shrink_forward = .{
                                        .id = id,
                                        .coord = to.minus(from[1]),
                                    },
                                });
                            } else {
                                try state_changes.append(StateDiff{
                                    .shrink_backward = .{
                                        .id = id,
                                        .coord = to.minus(from[0]),
                                    },
                                });
                            }
                        },
                    }
                },
            }
        }
        {
            var iterator = polymorphs.iterator();
            while (iterator.next()) |kv| {
                try state_changes.append(StateDiff{
                    .polymorph = .{
                        .id = kv.key_ptr.*,
                        .from = game_state.individuals.get(kv.key_ptr.*).?.species,
                        .to = kv.value_ptr.*,
                    },
                });
            }
        }
        for (everybody_including_dead) |id| {
            const old = game_state.individuals.get(id).?.status_conditions;
            const new = current_status_conditions.get(id).?;
            if (old != new) {
                try state_changes.append(StateDiff{
                    .status_condition_diff = .{
                        .id = id,
                        .from = old,
                        .to = new,
                    },
                });
            }
        }
        for (total_deaths.keys()) |id| {
            try state_changes.append(StateDiff{
                .despawn = blk: {
                    var individual = game_state.individuals.get(id).?.*;
                    individual.abs_position = current_positions.get(id).?;
                    break :blk .{
                        .id = id,
                        .individual = individual,
                    };
                },
            });
        }

        // final observations
        try game_state.applyStateChanges(state_changes.items);
        current_positions.clearRetainingCapacity();
        for (everybody) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.get(id).?,
                null,
                Activities.static_state,
            );
        }

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap([]PerceivedFrame).init(self.allocator);
                for (everybody_including_dead) |id| {
                    var frame_list = individual_to_perception.get(id).?.frames;
                    // remove empty frames, except the last one
                    var i: usize = 0;
                    frameLoop: while (i + 1 < frame_list.items.len) : (i +%= 1) {
                        const frame = frame_list.items[i];
                        if (frame.self.activity != PerceivedActivity.none) continue :frameLoop;
                        for (frame.others) |other| {
                            if (other.activity != PerceivedActivity.none) continue :frameLoop;
                        }
                        // delete this frame
                        _ = frame_list.orderedRemove(i);
                        i -%= 1;
                    }
                    try ret.putNoClobber(id, frame_list.toOwnedSlice());
                }
                break :blk ret;
            },
            .state_changes = state_changes.items,
        };
    }

    fn doMovementAndCollisions(
        self: *const GameEngine,
        game_state: *GameState,
        everybody: *[]u32,
        individual_to_perception: *IdMap(*MutablePerceivedHappening),
        current_positions: *IdMap(ThingPosition),
        intended_moves: *IdMap(Coord),
        budges_at_all: *IdMap(void),
        total_deaths: *IdMap(void),
    ) !void {
        var next_positions = IdMap(ThingPosition).init(self.allocator);

        for (everybody.*) |id| {
            // seek forward and stop at any wall.
            const initial_head_coord = getHeadPosition(current_positions.get(id).?);
            const move_delta = intended_moves.get(id) orelse continue;
            const move_unit = move_delta.signumed();
            const move_magnitude = move_delta.magnitudeDiag();
            var distance: i32 = 1;
            while (true) : (distance += 1) {
                const new_head_coord = initial_head_coord.plus(move_unit.scaled(distance));
                if (!isOpenSpace(game_state.terrainAt(new_head_coord).wall)) {
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
            try next_positions.putNoClobber(id, applyMovementToPosition(current_positions.get(id).?, adjusted_move_delta));
        }

        var trample_deaths = IdMap(void).init(self.allocator);

        // Collision detection and resolution.
        {
            const Collision = struct {
                /// this has at most one entry
                inertia_index_to_stationary_id: [2]?u32 = [_]?u32{null} ** 2,
                inertia_index_to_cardinal_index_to_enterer: [2][4]?u32 = [_][4]?u32{[_]?u32{null} ** 4} ** 2,
                inertia_index_to_cardinal_index_to_fast_enterer: [2][4]?u32 = [_][4]?u32{[_]?u32{null} ** 4} ** 2,
                winner_id: ?u32 = null,
            };

            // Collect collision information.
            var coord_to_collision = CoordMap(Collision).init(self.allocator);
            for (everybody.*) |id| {
                const inertia_index = getInertiaIndex(game_state.individuals.get(id).?.species);
                const old_position = current_positions.get(id).?;
                const new_position = next_positions.get(id) orelse old_position;
                for (getAllPositions(&new_position)) |new_coord, i| {
                    const old_coord = getAllPositions(&old_position)[i];
                    const delta = new_coord.minus(old_coord);
                    var collision = coord_to_collision.get(new_coord) orelse Collision{};
                    if (delta.equals(zero_vector)) {
                        collision.inertia_index_to_stationary_id[inertia_index] = id;
                    } else if (isCardinalDirection(delta)) {
                        collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][directionToCardinalIndex(delta)] = id;
                    } else if (isScaledCardinalDirection(delta, 2)) {
                        collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][directionToCardinalIndex(delta.scaledDivTrunc(2))] = id;
                    } else unreachable;
                    _ = try coord_to_collision.put(new_coord, collision);
                }
            }

            // Determine who wins each collision
            for (coord_to_collision.values()) |*collision| {
                var is_trample = false;
                // higher inertia trumps any lower inertia
                for ([_]u1{ 1, 0 }) |inertia_index| {
                    if (is_trample) {
                        // You can't win. You can only get trampled.
                        for ([_]?u32{
                            collision.inertia_index_to_stationary_id[inertia_index],
                            collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][0],
                            collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][1],
                            collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][2],
                            collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][3],
                            collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][0],
                            collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][1],
                            collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][2],
                            collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][3],
                        }) |maybe_id| {
                            if (maybe_id) |trampled_id| {
                                try trample_deaths.putNoClobber(trampled_id, {});
                            }
                        }
                    } else {
                        var incoming_vector_set: u9 = 0;
                        if (collision.inertia_index_to_stationary_id[inertia_index] != null) incoming_vector_set |= 1 << 0;
                        for (collision.inertia_index_to_cardinal_index_to_enterer[inertia_index]) |maybe_id, i| {
                            if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (1 + @as(u4, @intCast(u2, i)));
                        }
                        for (collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index]) |maybe_id, i| {
                            if (maybe_id != null) incoming_vector_set |= @as(u9, 1) << (5 + @as(u4, @intCast(u2, i)));
                        }
                        if (incoming_vector_set & 1 != 0) {
                            // Stationary entities always win.
                            collision.winner_id = collision.inertia_index_to_stationary_id[inertia_index];
                            // Standing still doesn't trample.
                        } else if (cardinalIndexBitSetToCollisionWinnerIndex(@intCast(u4, (incoming_vector_set & 0b111100000) >> 5))) |index| {
                            // fast bois beat slow bois.
                            collision.winner_id = collision.inertia_index_to_cardinal_index_to_fast_enterer[inertia_index][index].?;
                            is_trample = inertia_index > 0;
                        } else if (cardinalIndexBitSetToCollisionWinnerIndex(@intCast(u4, (incoming_vector_set & 0b11110) >> 1))) |index| {
                            // a slow boi wins.
                            collision.winner_id = collision.inertia_index_to_cardinal_index_to_enterer[inertia_index][index].?;
                            is_trample = inertia_index > 0;
                        } else {
                            // nobody wins. winner stays null.
                        }
                    }
                }
            }
            id_loop: for (everybody.*) |id| {
                if (trample_deaths.contains(id)) {
                    // The move succeeds so that we see where the squashing happens.
                    continue;
                }
                const next_position = next_positions.get(id) orelse continue;
                for (getAllPositions(&next_position)) |coord| {
                    var collision = coord_to_collision.get(coord).?;
                    // TODO: https://github.com/ziglang/zig/issues/1332 if (collision.winner_id != id)
                    if (!(collision.winner_id != null and collision.winner_id.? == id)) {
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
            for (everybody.*) |head_id| {
                // anyone who fails to move could be the head of a sad conga line.
                if (next_positions.contains(head_id)) continue;
                if (trample_deaths.contains(head_id)) {
                    // You're dead to me.
                    continue;
                }
                var id = head_id;
                conga_loop: while (true) {
                    const position = current_positions.get(id).?;
                    for (getAllPositions(&position)) |coord| {
                        const follower_id = (coord_to_collision.fetchSwapRemove(coord) orelse continue).value.winner_id orelse continue;
                        if (follower_id == id) continue; // your tail is always allowed to follow your head.
                        // conga line is botched.
                        if (getInertiaIndex(game_state.individuals.get(follower_id).?.species) >
                            getInertiaIndex(game_state.individuals.get(head_id).?.species))
                        {
                            // Outta my way!
                            try trample_deaths.putNoClobber(head_id, {});
                            // Party don't stop for this pushover's failure! Viva la conga!
                            break :conga_loop;
                        }
                        _ = next_positions.swapRemove(follower_id);
                        // and now you get to pass on the bad news.
                        id = follower_id;
                        continue :conga_loop;
                    }
                    // no one wants to move into this space.
                    break;
                }
            }
        }

        // Observe movement.
        for (everybody.*) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.get(id).?,
                current_positions,
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
            current_positions.getEntry(id).?.value_ptr.* = next_position;
        }

        for (everybody.*) |id| {
            try self.observeFrame(
                game_state,
                id,
                individual_to_perception.get(id).?,
                current_positions,
                Activities{ .deaths = &trample_deaths },
            );
        }
        try flushDeaths(total_deaths, &trample_deaths, everybody);

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
        attacks: *const IdMap(Attack),
        polymorphs: *const IdMap(Species),

        deaths: *const IdMap(void),

        const Movement = struct {
            intended_moves: *const IdMap(Coord),
            next_positions: *const IdMap(ThingPosition),
        };
        const Attack = struct {
            direction: Coord,
            distance: i32,
        };
    };
    fn observeFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        perception: *MutablePerceivedHappening,
        maybe_current_positions: ?*const IdMap(ThingPosition),
        activities: Activities,
    ) !void {
        try perception.frames.append(try getPerceivedFrame(
            self,
            game_state,
            my_id,
            maybe_current_positions,
            activities,
        ));
    }

    pub fn getStaticPerception(self: *const GameEngine, game_state: GameState, individual_id: u32) !PerceivedFrame {
        return getPerceivedFrame(
            self,
            &game_state,
            individual_id,
            null,
            Activities.static_state,
        );
    }

    fn getPerceivedFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        maybe_current_positions: ?*const IdMap(ThingPosition),
        activities: Activities,
    ) !PerceivedFrame {
        const actual_me = game_state.individuals.get(my_id).?;
        const your_abs_position = if (maybe_current_positions) |current_positions|
            current_positions.get(my_id).?
        else
            actual_me.abs_position;
        const your_coord = getHeadPosition(your_abs_position);
        var yourself: ?PerceivedThing = null;
        var others = ArrayList(PerceivedThing).init(self.allocator);
        const view_distance = getViewDistance(actual_me.species);

        for (game_state.individuals.keys()) |id| {
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

                .attacks => |data| if (data.get(id)) |attack|
                    PerceivedActivity{
                        .attack = PerceivedActivity.Attack{
                            .direction = attack.direction,
                            .distance = attack.distance,
                        },
                    }
                else
                    PerceivedActivity{ .none = {} },

                .kicks => |data| if (data.get(id)) |coord|
                    PerceivedActivity{ .kick = coord }
                else
                    PerceivedActivity{ .none = {} },

                .polymorphs => |data| if (data.get(id)) |species|
                    PerceivedActivity{ .polymorph = species }
                else
                    PerceivedActivity{ .none = {} },

                .deaths => |data| if (data.get(id)) |_|
                    PerceivedActivity{ .death = {} }
                else
                    PerceivedActivity{ .none = {} },

                .static_state => PerceivedActivity{ .none = {} },
            };

            var abs_position = if (maybe_current_positions) |current_positions|
                current_positions.get(id).?
            else
                game_state.individuals.get(id).?.abs_position;
            var rel_position: ThingPosition = undefined;
            switch (abs_position) {
                .small => |coord| {
                    rel_position = .{ .small = coord.minus(your_coord) };
                },
                .large => |coords| {
                    rel_position = .{
                        .large = .{
                            coords[0].minus(your_coord),
                            coords[1].minus(your_coord),
                        },
                    };
                },
            }
            // if any position is within view, we can see all of it.
            var within_view = false;
            for (getAllPositions(&rel_position)) |delta| {
                if (delta.magnitudeDiag() <= view_distance) {
                    within_view = true;
                    break;
                }
            } else if (actual_me.species == .blob) {
                // Blobs can also see with their butts.
                switch (if (maybe_current_positions) |current_positions|
                    current_positions.get(my_id).?
                else
                    game_state.individuals.get(my_id).?.abs_position) {
                    .large => |data| {
                        for (getAllPositions(&abs_position)) |other_abs_coord| {
                            if (other_abs_coord.minus(data[1]).magnitudeDiag() <= view_distance) {
                                within_view = true;
                                break;
                            }
                        }
                    },
                    else => {},
                }
            }
            if (!within_view) continue;
            const actual_thing = game_state.individuals.get(id).?;
            const thing = PerceivedThing{
                .species = actual_thing.species,
                .rel_position = rel_position,
                .status_conditions = actual_thing.status_conditions,
                .has_shield = actual_thing.has_shield,
                .activity = activity,
            };
            if (id == my_id) {
                yourself = thing;
            } else {
                try others.append(thing);
            }
        }

        var view_position = makeCoord(-view_distance, -view_distance);
        const view_side_length = view_distance * 2 + 1;
        var view_size = makeCoord(view_side_length, view_side_length);
        if (actual_me.species == .blob) {
            switch (actual_me.abs_position) {
                .large => |data| {
                    // Blobs can also see with their butts.
                    if (data[1].x < data[0].x) {
                        view_position.x -= 1;
                        view_size.x += 1;
                    } else if (data[1].x > data[0].x) {
                        view_size.x += 1;
                    } else if (data[1].y < data[0].y) {
                        view_position.y -= 1;
                        view_size.y += 1;
                    } else if (data[1].y > data[0].y) {
                        view_size.y += 1;
                    } else unreachable;
                },
                else => {},
            }
        }
        var terrain_chunk = core.protocol.TerrainChunk{
            .rel_position = view_position,
            .matrix = try Terrain.initFill(self.allocator, @intCast(u16, view_size.x), @intCast(u16, view_size.y), oob_terrain),
        };
        const view_origin = your_coord.plus(view_position);
        var cursor = Coord{ .x = undefined, .y = 0 };
        while (cursor.y < view_size.y) : (cursor.y += 1) {
            cursor.x = 0;
            while (cursor.x < view_size.x) : (cursor.x += 1) {
                var seen_cell: TerrainSpace = if (game_state.terrain.getCoord(cursor.plus(view_origin))) |cell| cell else oob_terrain;
                if (actual_me.species == .blob) {
                    // blobs are blind.
                    seen_cell = if (isOpenSpace(seen_cell.wall))
                        TerrainSpace{ .floor = .unknown_floor, .wall = .air }
                    else
                        TerrainSpace{ .floor = .unknown, .wall = .unknown_wall };
                }
                terrain_chunk.matrix.atCoord(cursor).?.* = seen_cell;
            }
        }

        var winning_score: ?i32 = 0;
        const my_species = @as(std.meta.Tag(Species), game_state.individuals.get(my_id).?.species);
        for (game_state.individuals.values()) |individual| {
            if (my_species != individual.species) {
                winning_score = null;
                break;
            }
            winning_score.? += 1;
        }

        var your_new_head_coord = your_coord;
        switch (yourself.?.activity) {
            .movement, .growth => |move_delta| {
                // move your body, or grow your head into another square.
                your_new_head_coord = your_new_head_coord.plus(move_delta);
            },
            .shrink => |index| {
                if (index != 0) {
                    // move your head back into your butt.
                    const position_delta = your_abs_position.large[0].minus(your_abs_position.large[1]);
                    your_new_head_coord = your_new_head_coord.minus(position_delta);
                }
            },
            else => {},
        }

        return PerceivedFrame{
            .self = yourself.?,
            .others = others.toOwnedSlice(),
            .terrain = terrain_chunk,
            .winning_score = winning_score,
            .movement = your_new_head_coord.minus(your_coord), // sometimes the game server overrides this.
        };
    }
};

const MutablePerceivedHappening = struct {
    frames: ArrayList(PerceivedFrame),
    pub fn init(allocator: *std.mem.Allocator) MutablePerceivedHappening {
        return MutablePerceivedHappening{
            .frames = ArrayList(PerceivedFrame).init(allocator),
        };
    }
};

fn flushDeaths(total_deaths: *IdMap(void), local_deaths: *IdMap(void), everybody: *[]u32) !void {
    for (local_deaths.keys()) |id| {
        // write local deaths to total deaths
        if (null == try total_deaths.fetchPut(id, {})) {
            // actually removed.
            const index = std.mem.indexOfScalar(u32, everybody.*, id).?;
            // swap remove
            everybody.*[index] = everybody.*[everybody.len - 1];
            everybody.len -= 1;
        }
    }
    local_deaths.clearRetainingCapacity();
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
