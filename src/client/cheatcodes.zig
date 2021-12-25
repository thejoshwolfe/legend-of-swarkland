const core = @import("../index.zig");
const makeCoord = core.geometry.makeCoord;
const Action = core.protocol.Action;

pub fn beatLevelActions(level_number: usize) []const Action {
    return switch (level_number) {

        // Single orc
        0 => comptime &[_]Action{
            Action{ .move = makeCoord(0, -1) },
            Action{ .move = makeCoord(0, -1) },
            Action{ .move = makeCoord(0, -1) },
            Action{ .move = makeCoord(0, -1) },
            Action{ .attack = makeCoord(0, -1) },
            Action{ .move = makeCoord(0, 1) },
            Action{ .attack = makeCoord(0, -1) },
            Action{ .move = makeCoord(1, 0) },
            Action{ .move = makeCoord(1, 0) },
            Action{ .move = makeCoord(1, 0) },
            Action{ .move = makeCoord(1, 0) },
            Action{ .move = makeCoord(1, 0) },
            Action{ .move = makeCoord(1, 0) },
        },

        else => &[_]Action{},
    };
}
