const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const ThingPosition = core.protocol.ThingPosition;
const Species = core.protocol.Species;
const Wall = core.protocol.Wall;

pub const view_distance = 8;

pub fn getAttackRange(species: Species) i32 {
    switch (species) {
        .centaur => return 16,
        else => return 1,
    }
}

pub fn isAffectedByAttacks(species: Species) bool {
    return switch (species) {
        .turtle => false,
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

pub fn getAllPositions(thing_position: ThingPosition) []const Coord {
    return switch (thing_position) {
        .small => |*coord| @as(*const [1]Coord, coord),
        .large => |*coords| coords[0..],
    };
}
