pub const geometry = @import("./core/geometry.zig");
pub const matrix = @import("./core/matrix.zig");
pub const protocol = @import("./core/protocol.zig");
pub const debug = @import("./core/debug.zig");
pub const game_logic = @import("./core/game_logic.zig");

pub const game_engine_client = @import("./client/game_engine_client.zig");

test "index" {
    _ = @import("./client/game_engine_client.zig");
    _ = @import("./core/matrix.zig");
    _ = @import("./core/protocol.zig");
}
