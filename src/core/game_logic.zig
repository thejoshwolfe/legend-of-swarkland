const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const isScaledCardinalDirection = core.geometry.isScaledCardinalDirection;
const PerceivedActivity = core.protocol.PerceivedActivity;
const ThingPosition = core.protocol.ThingPosition;
const Species = core.protocol.Species;
const Wall = core.protocol.Wall;
const Action = core.protocol.Action;
const TerrainSpace = core.protocol.TerrainSpace;
const TerrainChunk = core.protocol.TerrainChunk;

const assert = @import("std").debug.assert;

pub const unseen_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .unknown,
};

pub const PerceivedTerrain = core.matrix.SparseChunkedMatrix(TerrainSpace, unseen_terrain);

pub fn getViewDistance(species: Species) i32 {
    return switch (species) {
        .blob => 1,
        else => 8,
    };
}

pub fn canAttack(species: Species) bool {
    return getAttackRange(species) > 0;
}

pub fn getAttackRange(species: Species) i32 {
    switch (species) {
        .centaur => return 16,
        .rhino, .blob, .kangaroo, .rat => return 0,
        else => return 1,
    }
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
        else => false,
    };
}

pub fn canNibble(species: Species) bool {
    return switch (species) {
        .rat => true,
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
    };
}

pub fn canUseDoors(species: Species) bool {
    return switch (species) {
        .human, .orc => true,
        .centaur => true,
        else => false,
    };
}

pub fn getPhysicsLayer(species: Species) u2 {
    switch (species) {
        .blob => return 0,
        .rat => return 1,
        .rhino => return 3,
        else => return 2,
    }
}

pub fn isFastMoveAligned(position: ThingPosition, move_delta: Coord) bool {
    assert(core.geometry.isScaledCardinalDirection(move_delta, 2));
    const facing_delta = position.large[0].minus(position.large[1]);
    return facing_delta.scaled(2).equals(move_delta);
}

pub fn isAffectedByAttacks(species: Species, position_index: usize) bool {
    return switch (species) {
        .turtle, .blob, .wood_golem => false,
        // only rhino's tail is affected, not head.
        .rhino => position_index == 1,
        else => true,
    };
}

pub fn isOpenSpace(wall: Wall) bool {
    return switch (wall) {
        .air => true,
        .dirt, .stone, .sandstone => false,
        .tree_northwest, .tree_northeast, .tree_southwest, .tree_southeast => false,
        .bush => true,
        .door_open => true,
        .door_closed => false,
        .polymorph_trap_centaur, .polymorph_trap_kangaroo, .polymorph_trap_turtle, .polymorph_trap_blob, .polymorph_trap_human, .unknown_polymorph_trap => true,
        .polymorph_trap_rhino_west, .polymorph_trap_blob_west, .unknown_polymorph_trap_west => true,
        .polymorph_trap_rhino_east, .polymorph_trap_blob_east, .unknown_polymorph_trap_east => true,
        .unknown => true,
        .unknown_wall => false,
    };
}

pub fn isTransparentSpace(wall: Wall) bool {
    return switch (wall) {
        .tree_northwest, .tree_northeast, .tree_southwest, .tree_southeast => true,
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
    }
}

pub fn validateAction(species: Species, position: ThingPosition, action: Action) !void {
    switch (action) {
        .wait => {},
        .move => |move_delta| {
            if (!isCardinalDirection(move_delta)) return error.BadDelta;
            if (!canMoveNormally(species)) return error.SpeciesIncapable;
        },
        .fast_move => |move_delta| {
            if (!isScaledCardinalDirection(move_delta, 2)) return error.BadDelta;
            if (!canCharge(species)) return error.SpeciesIncapable;
            if (!isFastMoveAligned(position, move_delta)) return error.BadAlignment;
        },
        .grow => |move_delta| {
            if (!isCardinalDirection(move_delta)) return error.BadDelta;
            if (!canGrowAndShrink(species)) return error.SpeciesIncapable;
            if (position != .small) return error.TooBig;
        },
        .shrink => {
            if (!canGrowAndShrink(species)) return error.SpeciesIncapable;
            if (position != .large) return error.TooSmall;
        },
        .attack => |direction| {
            if (!canAttack(species)) return error.SpeciesIncapable;
            if (!isCardinalDirection(direction)) return error.BadDelta;
        },
        .kick => |direction| {
            if (!canKick(species)) return error.SpeciesIncapable;
            if (!isCardinalDirection(direction)) return error.BadDelta;
        },
        .nibble => {
            if (!canNibble(species)) return error.SpeciesIncapable;
        },
        .stomp => {
            if (!canKick(species)) return error.SpeciesIncapable;
        },
        .lunge => |direction| {
            if (!canLunge(species)) return error.SpeciesIncapable;
            if (!isCardinalDirection(direction)) return error.BadDelta;
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
