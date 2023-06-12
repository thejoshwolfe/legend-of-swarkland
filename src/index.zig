pub const geometry = @import("./core/geometry.zig");
pub const matrix = @import("./core/matrix.zig");
pub const protocol = @import("./core/protocol.zig");
pub const debug = @import("./core/debug.zig");
pub const game_logic = @import("./core/game_logic.zig");

test "index" {
    _ = @import("./core/matrix.zig");
    _ = @import("./core/protocol.zig");
}
