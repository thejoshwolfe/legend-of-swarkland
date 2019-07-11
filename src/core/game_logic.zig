const core = @import("../index.zig");

pub fn getAttackRange(species: core.protocol.Species) i32 {
    switch (species) {
        .centaur => return 16,
        else => return 1,
    }
}

pub fn isOpenSpace(wall: core.protocol.Wall) bool {
    return wall == core.protocol.Wall.air;
}
