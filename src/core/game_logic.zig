const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isOrthogonalVectorOfMagnitude = core.geometry.isOrthogonalVectorOfMagnitude;
const cardinalDirectionToDelta = core.geometry.cardinalDirectionToDelta;
const PerceivedActivity = core.protocol.PerceivedActivity;
const ThingPosition = core.protocol.ThingPosition;
const Species = core.protocol.Species;
const Wall = core.protocol.Wall;
const Floor = core.protocol.Floor;
const Action = core.protocol.Action;
const TerrainSpace = core.protocol.TerrainSpace;
const TerrainChunk = core.protocol.TerrainChunk;
const Equipment = core.protocol.Equipment;
const StatusConditions = core.protocol.StatusConditions;

const assert = @import("std").debug.assert;

pub const unseen_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .unknown,
};

pub const PerceivedTerrain = core.matrix.SparseChunkedMatrix(TerrainSpace, unseen_terrain, .{
    .metrics = true,
});

pub fn getViewDistance(species: Species) i32 {
    return switch (species) {
        .blob => 1,
        else => 8,
    };
}

pub fn validateAttack(species: Species, equipment: Equipment) !void {
    return switch (species) {
        .rhino, .blob, .kangaroo, .rat => error.SpeciesIncapable,
        .centaur => |subspecies| switch (subspecies) {
            .archer => error.SpeciesIncapable,
            .warrior => {},
        },
        .ant => |subspecies| switch (subspecies) {
            .worker => error.SpeciesIncapable,
            .queen => {},
        },
        .human => {
            if (equipment.is_equipped(.axe) or equipment.is_equipped(.torch) or equipment.is_equipped(.dagger)) return {};
            return error.MissingItem;
        },

        .orc, .turtle, .wolf, .wood_golem, .scorpion, .brown_snake, .minotaur, .ogre, .siren => {},
    };
}

pub const bow_range = 16;
pub fn hasBow(species: Species) bool {
    return switch (species) {
        .centaur => |subspecies| switch (subspecies) {
            .archer => true,
            .warrior => false,
        },
        else => false,
    };
}

pub const AttackEffect = enum {
    miss, // arrow flies past.
    no_effect, // arrow stops.
    kill,
    wound,
    malaise,
    shield_parry,
    heavy_hit_knocks_away_shield,
};

pub fn getAttackEffect(
    attacker_species: Species,
    attacker_equipment: Equipment, // TODO: replace with some kind of attack action, including stomp etc?
    target_species: Species,
    target_position_index: usize,
    target_status_conditions: StatusConditions,
    target_defending_with_shield: bool,
) AttackEffect {
    const attack_function = getAttackFunction(attacker_species, attacker_equipment);

    // Miss due to size?
    switch (getPhysicsLayer(target_species)) {
        0, 1 => switch (attack_function) {
            // too short to be hit by "regular" attacks
            .wound_then_kill, .just_wound, .malaise, .burn => return .miss,
            // these still reach
            .chop, .smash => {},
        },
        2, 3 => {},
    }

    // Innate defense?
    if (!isAffectedByAttacks(target_species, target_position_index)) {
        switch (attack_function) {
            // "regular" attacks
            .wound_then_kill, .just_wound, .malaise => return .no_effect,
            // these are still effective
            .chop, .smash, .burn => {},
        }
    }

    // Active defense?
    if (target_defending_with_shield) {
        if (attack_function == .smash) {
            // hammer counters shield
            return .heavy_hit_knocks_away_shield;
        }
        // shield parry (if within range).
        return .shield_parry;
    }

    // Get wrecked.
    switch (attack_function) {
        .wound_then_kill => {
            if (woundThenKillGoesRightToKill(target_species)) {
                // Fragile target gets instakilled.
                return .kill;
            }
            if (target_status_conditions & core.protocol.StatusCondition_wounded_leg == 0) {
                // First hit is a wound.
                return .wound;
            } else {
                // Second hit is a ded.
                return .kill;
            }
        },
        .just_wound => {
            return .wound;
        },
        .malaise => {
            return .malaise;
        },
        .smash, .chop => {
            return .kill;
        },
        .burn => {
            if (target_species == .wood_golem) {
                // You solved the puzzle!
                return .kill;
            }
            if (woundThenKillGoesRightToKill(target_species)) {
                // Fragile target can't handle the heat.
                return .kill;
            }
            // Fire isn't that deadly.
            return .wound;
        },
    }
    unreachable;
}

pub const AttackFunction = enum {
    just_wound,
    wound_then_kill,
    malaise,
    smash,
    chop,
    burn,
};

pub fn getAttackFunction(species: Species, equipment: Equipment) AttackFunction {
    if (equipment.is_equipped(.axe)) {
        return .chop;
    }
    if (equipment.is_equipped(.torch)) {
        return .burn;
    }
    if (equipment.is_equipped(.dagger)) {
        return .wound_then_kill;
    }
    return switch (species) {
        .orc => .wound_then_kill,
        .centaur => |subspecies| switch (subspecies) {
            .archer => .wound_then_kill,
            .warrior => .chop,
        },
        .turtle => .wound_then_kill,
        .wolf => .wound_then_kill,
        .wood_golem => .wound_then_kill,
        .brown_snake => .wound_then_kill,
        .ant => |subspecies| switch (subspecies) {
            .worker => unreachable,
            .queen => .wound_then_kill,
        },
        .minotaur => .wound_then_kill,
        .siren => .wound_then_kill,

        .rat => .just_wound,
        .scorpion => .malaise,

        .ogre => .smash,

        .human => unreachable, // Handled by equipment checks.
        .rhino => unreachable,
        .kangaroo => unreachable,
        .blob => unreachable,
    };
}

pub fn actionCausesPainWhileMalaised(action: std.meta.Tag(Action)) bool {
    return switch (action) {
        .charge, .attack, .kick, .nibble, .stomp, .lunge, .open_close, .fire_bow, .defend => true,
        .pick_up_and_equip, .pick_up_unequipped, .equip, .unequip => true,
        .move, .grow, .shrink, .wait => false,
        .nock_arrow => false,
        .cheatcode_warp => false,
    };
}

pub fn canCharge(species: Species) bool {
    return switch (species) {
        .rhino => true,
        else => false,
    };
}

pub fn canLunge(species: Species) bool {
    return switch (species) {
        .wolf => true,
        .brown_snake => true,
        .siren => |subspecies| switch (subspecies) {
            .water => true,
            .land => false,
        },
        else => false,
    };
}
pub fn limpsAfterLunge(species: Species) bool {
    return switch (species) {
        .wolf => false,
        .brown_snake => true,
        .siren => false,
        else => unreachable,
    };
}

pub fn canNibble(species: Species) bool {
    return switch (species) {
        .rat, .scorpion => true,
        .ant => |subspecies| switch (subspecies) {
            .worker => false,
            .queen => true,
        },
        else => false,
    };
}

pub fn canMoveNormally(species: Species) bool {
    return switch (species) {
        .blob => false,
        else => true,
    };
}

pub fn canGrowAndShrink(species: Species) bool {
    return switch (species) {
        .blob => true,
        else => false,
    };
}

pub fn isSlow(species: Species) bool {
    return switch (species) {
        .wood_golem => true,
        .scorpion => true,
        .ogre => true,
        else => false,
    };
}

pub fn canKick(species: Species) bool {
    return switch (species) {
        .human, .orc => true,
        .centaur => true,
        .turtle => false,
        .rhino => true,
        .kangaroo => true,
        .blob => false,
        .wolf => false,
        .rat => false,
        .wood_golem => true,
        .scorpion => false,
        .brown_snake => false,
        .ant => false,
        .minotaur => true,
        .ogre => true,
        .siren => |subspecies| switch (subspecies) {
            .water => false,
            .land => true,
        },
    };
}

pub fn canUseDoors(species: Species) bool {
    return hasHands(species);
}
pub fn canUseItems(species: Species) bool {
    return hasHands(species);
}

fn hasHands(species: Species) bool {
    return switch (getAnatomy(species)) {
        .humanoid => true,
        .centauroid => true,
        .minotauroid => true,
        .mermoid => true,
        .insectoid, .quadruped, .serpentine, .scorpionoid, .bloboid, .kangaroid => false,
    };
}

pub fn getPhysicsLayer(species: Species) u2 {
    switch (species) {
        .blob => return 0,
        .rat => return 1,
        .scorpion, .ant => return 1,
        .rhino, .ogre => return 3,
        else => return 2,
    }
}

pub fn isFastMoveAligned(position: ThingPosition, move_delta: Coord) bool {
    assert(isOrthogonalVectorOfMagnitude(move_delta, 2));
    if (position != .large) return false;
    const facing_delta = position.large[0].minus(position.large[1]);
    return facing_delta.scaled(2).equals(move_delta);
}

pub fn isAffectedByAttacks(species: Species, position_index: usize) bool {
    return switch (species) {
        .turtle, .blob, .wood_golem => false,
        // only rhino's tail is affected, not head.
        .rhino => position_index == 1,
        .siren => |subspecies| switch (subspecies) {
            .water => false,
            .land => true,
        },
        .ogre => false,
        .minotaur => false,
        else => true,
    };
}

pub fn woundThenKillGoesRightToKill(species: Species) bool {
    return switch (species) {
        .scorpion, .ant, .brown_snake => true,
        else => false,
    };
}

pub fn isOpenSpace(wall: Wall) bool {
    return switch (wall) {
        .air => true,
        .dirt, .stone, .sandstone => false,
        .sandstone_cracked => true,
        .tree_northwest, .tree_northeast, .tree_southwest, .tree_southeast => false,
        .bush => true,
        .door_open => true,
        .door_closed => false,
        .angel_statue => false,
        .chest => true,
        .polymorph_trap_centaur, .polymorph_trap_kangaroo, .polymorph_trap_turtle, .polymorph_trap_blob, .polymorph_trap_human, .unknown_polymorph_trap => true,
        .polymorph_trap_rhino_west, .polymorph_trap_blob_west, .unknown_polymorph_trap_west => true,
        .polymorph_trap_rhino_east, .polymorph_trap_blob_east, .unknown_polymorph_trap_east => true,
        .unknown => true,
        .unknown_wall => false,
    };
}
pub fn isWet(floor: Floor) bool {
    return switch (floor) {
        .water, .water_bloody, .water_deep => true,
        else => false,
    };
}

pub fn isTransparentSpace(wall: Wall) bool {
    return switch (wall) {
        .tree_northwest, .tree_northeast, .tree_southwest, .tree_southeast => true,
        .sandstone_cracked => false,
        else => return isOpenSpace(wall),
    };
}

pub fn getHeadPosition(thing_position: ThingPosition) Coord {
    return switch (thing_position) {
        .small => |coord| coord,
        .large => |coords| coords[0],
    };
}

pub fn getAllPositions(thing_position: *const ThingPosition) []const Coord {
    return switch (thing_position.*) {
        .small => |*coord| @as(*const [1]Coord, coord)[0..],
        .large => |*coords| coords[0..],
    };
}

pub fn applyMovementToPosition(position: ThingPosition, move_delta: Coord) ThingPosition {
    switch (position) {
        .small => |coord| {
            return .{ .small = coord.plus(move_delta) };
        },
        .large => |coords| {
            const next_head_coord = coords[0].plus(move_delta);
            return .{
                .large = .{
                    next_head_coord,
                    next_head_coord.minus(move_delta.signumed()),
                },
            };
        },
    }
}

pub const Anatomy = enum {
    humanoid,
    centauroid,
    quadruped,
    kangaroid,
    bloboid,
    scorpionoid,
    serpentine,
    insectoid,
    minotauroid,
    mermoid,
};
pub fn getAnatomy(species: Species) Anatomy {
    switch (species) {
        .human, .orc => return .humanoid,
        .centaur => return .centauroid,
        .turtle, .rhino => return .quadruped,
        .kangaroo => return .kangaroid,
        .blob => return .bloboid,
        .wolf => return .quadruped,
        .rat => return .quadruped,
        .wood_golem => return .humanoid,
        .scorpion => return .scorpionoid,
        .brown_snake => return .serpentine,
        .ant => return .insectoid,
        .minotaur => return .minotauroid,
        .siren => |subspecies| switch (subspecies) {
            .water => return .mermoid,
            .land => return .humanoid,
        },
        .ogre => return .humanoid,
    }
}

pub fn validateAction(species: Species, position: std.meta.Tag(ThingPosition), status_conditions: StatusConditions, equipment: Equipment, action: std.meta.Tag(Action)) !void {
    const immobilizing_statuses = core.protocol.StatusCondition_limping | core.protocol.StatusCondition_grappled;
    const pain_statuses = core.protocol.StatusCondition_pain;
    switch (action) {
        .wait => {},
        .move => {
            if (!canMoveNormally(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & immobilizing_statuses) return error.StatusForbids;
        },
        .charge => {
            if (!canCharge(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & (immobilizing_statuses | pain_statuses)) return error.StatusForbids;
        },
        .grow => {
            if (!canGrowAndShrink(species)) return error.SpeciesIncapable;
            if (position != .small) return error.TooBig;
        },
        .shrink => {
            if (!canGrowAndShrink(species)) return error.SpeciesIncapable;
            if (position != .large) return error.TooSmall;
        },
        .attack => {
            try validateAttack(species, equipment);
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .kick => {
            if (!canKick(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .nibble => {
            if (!canNibble(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .stomp => {
            if (!canKick(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .lunge => {
            if (!canLunge(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & (immobilizing_statuses | pain_statuses)) return error.StatusForbids;
        },
        .open_close => {
            if (!canUseDoors(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .pick_up_and_equip, .pick_up_unequipped => {
            if (!canUseItems(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .nock_arrow => {
            if (!hasBow(species)) return error.SpeciesIncapable;
            if (0 != status_conditions & (pain_statuses | core.protocol.StatusCondition_arrow_nocked)) return error.StatusForbids;
        },
        .fire_bow => {
            if (0 == status_conditions & core.protocol.StatusCondition_arrow_nocked) return error.StatusForbids;
        },
        .defend => {
            if (!equipment.is_equipped(.shield)) return error.MissingItem;
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .equip => {
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },
        .unequip => {
            if (0 != status_conditions & pain_statuses) return error.StatusForbids;
        },

        .cheatcode_warp => {},
    }
}

pub fn applyMovementFromActivity(activity: PerceivedActivity, from_position: ThingPosition) ThingPosition {
    return switch (activity) {
        .movement => |delta| applyMovementToPosition(from_position, delta),
        .growth => |delta| ThingPosition{ .large = .{
            from_position.small.plus(delta),
            from_position.small,
        } },
        .shrink => |index| ThingPosition{ .small = from_position.large[index] },
        else => from_position,
    };
}

pub fn positionEquals(a: ThingPosition, b: ThingPosition) bool {
    if (@as(std.meta.Tag(ThingPosition), a) != b) return false;
    return switch (a) {
        .small => |coord| coord.equals(b.small),
        .large => |coords| coords[0].equals(b.large[0]) and coords[1].equals(b.large[1]),
    };
}

pub fn offsetPosition(position: ThingPosition, delta: Coord) ThingPosition {
    return switch (position) {
        .small => |coord| ThingPosition{ .small = coord.plus(delta) },
        .large => |coords| ThingPosition{ .large = .{
            coords[0].plus(delta),
            coords[1].plus(delta),
        } },
    };
}

pub fn terrainAt(terrain: TerrainChunk, coord: Coord) ?TerrainSpace {
    const inner_coord = coord.minus(terrain.position);
    return terrainAtInner(terrain, inner_coord);
}
pub fn terrainAtInner(terrain: TerrainChunk, inner_coord: Coord) ?TerrainSpace {
    if (!(0 <= inner_coord.x and inner_coord.x < terrain.width and //
        0 <= inner_coord.y and inner_coord.y < terrain.height))
    {
        return null;
    }
    return terrain.matrix[@intCast(usize, inner_coord.y * terrain.width + inner_coord.x)];
}
