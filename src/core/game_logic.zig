const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const ThingPosition = core.protocol.ThingPosition;
const Species = core.protocol.Species;
const Wall = core.protocol.Wall;

const assert = @import("std").debug.assert;

pub const view_distance = 8;

pub fn getAttackRange(species: Species) i32 {
    switch (species) {
        .centaur => return 16,
        .rhino => return 0,
        else => return 1,
    }
}

pub fn hasFastMove(species: Species) bool {
    return switch (species) {
        .rhino => true,
        else => false,
    };
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
    return wall == .air;
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
