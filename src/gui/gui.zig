const sdl = @import("./sdl.zig");
const core = @import("core");
const geometry = core.geometry;
const Coord = geometry.Coord;
const Rect = geometry.Rect;
const makeCoord = geometry.makeCoord;
const textures = @import("./textures.zig");
const Font = textures.Font;

pub const LinearMenuState = struct {
    cursor_position: usize = 0,
    entry_count: usize = 0,
    enter_pressed: bool = false,
    buffered_cheatcode: ?u64 = null,

    pub fn moveUp(self: *LinearMenuState, how_many: usize) void {
        if (self.cursor_position < how_many) {
            self.cursor_position = 0;
        } else {
            self.cursor_position -= how_many;
        }
    }
    pub fn moveDown(self: *LinearMenuState, how_many: usize) void {
        if (self.cursor_position + how_many >= self.entry_count) {
            self.cursor_position = self.entry_count -| 1;
        } else {
            self.cursor_position += how_many;
        }
    }
    pub fn ensureButtonCount(self: *LinearMenuState, button_count: usize) void {
        if (button_count > self.entry_count) {
            self.entry_count = button_count;
        }
    }
    pub fn enter(self: *LinearMenuState) void {
        self.enter_pressed = true;
    }
    pub fn beginFrame(self: *LinearMenuState) void {
        self.enter_pressed = false;
    }
};

pub const Gui = struct {
    _renderer: *sdl.Renderer,
    _menu_state: ?*LinearMenuState = null,
    _cursor_icon: ?Rect = null,

    _button_count: usize = 0,
    _cursor: Coord = .{ .x = 0, .y = 0 },
    _scale: i32 = 1,
    _font: Font = .large,
    _marginBottom: i32 = 0,
    pub fn initInteractive(renderer: *sdl.Renderer, menu_state: *LinearMenuState, cursor_icon: Rect) Gui {
        return Gui{
            ._renderer = renderer,
            ._menu_state = menu_state,
            ._cursor_icon = cursor_icon,
        };
    }
    pub fn init(renderer: *sdl.Renderer) Gui {
        return Gui{
            ._renderer = renderer,
        };
    }

    pub fn seek(self: *Gui, x: i32, y: i32) void {
        self._cursor.x = x;
        self._cursor.y = y;
    }
    pub fn seekRelative(self: *Gui, dx: i32, dy: i32) void {
        self._cursor.x += dx;
        self._cursor.y += dy;
    }

    pub fn scale(self: *Gui, s: i32) void {
        self._scale = s;
    }
    pub fn font(self: *Gui, f: Font) void {
        self._font = f;
    }
    pub fn marginBottom(self: *Gui, m: i32) void {
        self._marginBottom = m;
    }

    pub fn text(self: *Gui, string: []const u8) void {
        self._cursor.y = textures.renderTextScaled(self._renderer, string, self._cursor, self._font, self._scale).y;
        self._cursor.y += self._marginBottom * self._scale;
    }

    pub fn textWrapped(self: *Gui, string: []const u8, text_wrap_width: u32) void {
        var cursor: usize = 0;
        while (cursor < string.len) {
            const end = @min(cursor + text_wrap_width, string.len);
            self.text(string[cursor..end]);
            cursor = end;
        }
    }

    pub fn imageAndText(self: *Gui, sprite: Rect, string: []const u8) void {
        textures.renderSprite(self._renderer, sprite, self._cursor);
        const text_heigh = textures.getCharRect(' ', self._font).height * self._scale;
        const text_position = Coord{
            .x = self._cursor.x + sprite.width + 4,
            .y = self._cursor.y + if (sprite.height < text_heigh) 0 else @divTrunc(sprite.height, 2) - @divTrunc(text_heigh, 2),
        };

        const starting_y = self._cursor.y;
        self._cursor.y = textures.renderTextScaled(self._renderer, string, text_position, self._font, self._scale).y;
        if (self._cursor.y < starting_y + sprite.height) {
            self._cursor.y = starting_y + sprite.height;
        }
        self._cursor.y += self._marginBottom * self._scale;
    }

    pub fn button(self: *Gui, string: []const u8) bool {
        const start_position = self._cursor;
        self.text(string);
        const this_button_index = self._button_count;
        self._button_count += 1;
        self._menu_state.?.ensureButtonCount(self._button_count);
        if (self._menu_state.?.cursor_position == this_button_index) {
            // also render the cursor
            const icon_position = makeCoord(start_position.x - self._cursor_icon.?.width, start_position.y);
            textures.renderSprite(self._renderer, self._cursor_icon.?, icon_position);
            return self._menu_state.?.enter_pressed;
        }
        return false;
    }
};
