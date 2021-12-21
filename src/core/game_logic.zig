const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const isScaledCardinalDirection = core.geometry.isScaledCardinalDirection;
const ThingPosition = core.protocol.ThingPosition;
const Species = core.protocol.Species;
const Wall = core.protocol.Wall;
const Action = core.protocol.Action;

const assert = @import("std").debug.assert;

pub fn getViewDistance(species: Species) i32 {
    return switch (species) {
        .blob => 1,
        else => 8,
    };
}

pub fn getAttackRange(species: Species) i32 {
    switch (species) {
        .centaur => return 16,
        .rhino, .blob => return 0,
        else => return 1,
    }
}

pub fn canCharge(species: Species) bool {
    return switch (species) {
        .rhino => true,
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

pub fn getInertiaIndex(species: Species) u1 {
    switch (species) {
        .rhino, .blob => return 1,
        else => return 0,
    }
}

pub fn isFastMoveAligned(position: ThingPosition, move_delta: Coord) bool {
    assert(core.geometry.isScaledCardinalDirection(move_delta, 2));
    const facing_delta = position.large[0].minus(position.large[1]);
    return facing_delta.scaled(2).equals(move_delta);
}

pub fn isAffectedByAttacks(species: Species, position_index: usize) bool {
    return switch (species) {
        .turtle => false,
        // only rhino's tail is affected, not head.
        .rhino => position_index == 1,
        else => true,
    };
}

pub fn isOpenSpace(wall: Wall) bool {
    switch (wall) {
        .air, .centaur_transformer => return true,
        else => return false,
    }
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
};
pub fn getAnatomy(species: Species) Anatomy {
    switch (species) {
        .human, .orc => return .humanoid,
        .centaur => return .centauroid,
        .turtle, .rhino => return .quadruped,
        .kangaroo => return .kangaroid,
        .blob => return .bloboid,
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
            if (!isCardinalDirection(direction)) return error.BadDelta;
        },
        .kick => |direction| {
            if (!isCardinalDirection(direction)) return error.BadDelta;
        },
    }
}
