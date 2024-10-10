const gui = @import("gui.zig");
const sdl = @import("sdl.zig");
const textures = @import("textures.zig");
const the_levels = @import("server").the_levels;

pub fn doMainMenu(renderer: *sdl.Renderer, menu_state: *gui.LinearMenuState, filename: []const u8) union(enum) { none, puzzle_levels, new_game: ?u64 } {
    var menu_renderer = gui.Gui.initInteractive(renderer, menu_state, textures.sprites.dagger);

    menu_renderer.seek(10, 10);
    menu_renderer.scale(2);
    menu_renderer.font(.large_bold);
    menu_renderer.marginBottom(5);
    menu_renderer.text("Legend of Swarkland");
    menu_renderer.scale(1);
    menu_renderer.font(.large);
    menu_renderer.seekRelative(70, 30);
    if (menu_renderer.button("New Game")) {
        // Workaround compiler bug (feature?)
        // For more information, watch ATTACK of the KILLER FEATURES by Martin Wickham.
        // https://www.youtube.com/watch?v=dEIsJPpCZYg
        return .{ .new_game = menu_state.buffered_cheatcode };
    }
    if (menu_renderer.button("Puzzle Levels")) {
        return .puzzle_levels;
    }

    menu_renderer.seekRelative(-70, 50);
    menu_renderer.text("Menu Controls:");
    menu_renderer.text(" Arrow keys + Enter");
    menu_renderer.text(" ");
    menu_renderer.text("Display Size: 712x512");
    menu_renderer.text(" (Just resize the window)");
    menu_renderer.text(" ");
    menu_renderer.text("Save file path:");
    menu_renderer.seekRelative(12, 0);
    const text_wrap_width = 57; // how many monospace chars fit in the window or something.
    menu_renderer.textWrapped(filename, text_wrap_width);
    menu_renderer.seekRelative(-12, 0);
    menu_renderer.text(" ");
    menu_renderer.text(" ");
    menu_renderer.text("version: " ++ textures.version_string);
    return .none;
}

pub fn doPuzzleLevelsMenu(renderer: *sdl.Renderer, menu_state: *gui.LinearMenuState, completed_levels: u32) union(enum) { none, new_game: usize } {
    var menu_renderer = gui.Gui.initInteractive(renderer, menu_state, textures.sprites.dagger);
    menu_renderer.seek(32, 32);
    menu_renderer.marginBottom(3);

    for (the_levels[0 .. the_levels.len - 1], 0..) |level, i| {
        if (i < completed_levels) {
            // Past levels
            if (menu_renderer.button(level.name)) {
                return .{ .new_game = i };
            }
        } else if (i == completed_levels) {
            // Current level
            menu_renderer.font(.large_bold);
            if (menu_renderer.button("??? (New)")) {
                return .{ .new_game = i };
            }
            menu_renderer.font(.large);
        } else {
            // Future level
            menu_renderer.text("---");
        }
    }

    menu_renderer.seek(448, 32);
    menu_renderer.text("Menu Controls:");
    menu_renderer.text(" Arrow keys");
    menu_renderer.text(" Ctrl+Arrows");
    menu_renderer.text(" PageUp/PageDown");
    menu_renderer.text(" Home/End");
    menu_renderer.text(" Enter/Escape");
    return .none;
}
