const sdl = @import("./sdl.zig");
const core = @import("core");
const geometry = core.geometry;
const Coord = geometry.Coord;
const Rect = geometry.Rect;
const makeCoord = geometry.makeCoord;
const textures = @import("./textures.zig");

pub const LinearMenuState = struct {
    cursor_position: usize,
    entry_count: usize,
    enter_pressed: bool,
    pub fn init() LinearMenuState {
        return LinearMenuState{
            .cursor_position = 0,
            .entry_count = 0,
            .enter_pressed = false,
        };
    }
    pub fn moveUp(self: *LinearMenuState) void {
        if (self.cursor_position == 0) return;
        self.cursor_position -= 1;
    }
    pub fn moveDown(self: *LinearMenuState) void {
        self.cursor_position += 1;
        if (self.cursor_position >= self.entry_count) {
            self.cursor_position = if (self.entry_count == 0) 0 else self.entry_count - 1;
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
    _menu_state: *LinearMenuState,
    _cursor_icon: Rect,

    _button_count: usize,
    _cursor: Coord,
    _scale: i32,
    _bold: bool,
    _marginBottom: i32,
    pub fn init(renderer: *sdl.Renderer, menu_state: *LinearMenuState, cursor_icon: Rect) Gui {
        return Gui{
            ._renderer = renderer,
            ._menu_state = menu_state,
            ._cursor_icon = cursor_icon,
            ._button_count = 0,
            ._cursor = geometry.makeCoord(0, 0),
            ._scale = 0,
            ._bold = false,
            ._marginBottom = 0,
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
    pub fn bold(self: *Gui, b: bool) void {
        self._bold = b;
    }
    pub fn marginBottom(self: *Gui, m: i32) void {
        self._marginBottom = m;
    }

    pub fn text(self: *Gui, string: []const u8) void {
        self._cursor.y = textures.renderTextScaled(self._renderer, string, self._cursor, self._bold, self._scale).y;
        self._cursor.y += self._marginBottom * self._scale;
    }

    pub fn imageAndText(self: *Gui, sprite: Rect, string: []const u8) void {
        textures.renderSprite(self._renderer, sprite, self._cursor);
        const text_position = Coord{
            .x = self._cursor.x + sprite.width + 4,
            .y = self._cursor.y,
        };

        self._cursor.y = textures.renderTextScaled(self._renderer, string, text_position, self._bold, self._scale).y;
        self._cursor.y += self._marginBottom * self._scale;
    }

    pub fn button(self: *Gui, string: []const u8) bool {
        var start_position = self._cursor;
        self.text(string);
        const this_button_index = self._button_count;
        self._button_count += 1;
        self._menu_state.ensureButtonCount(self._button_count);
        if (self._menu_state.cursor_position == this_button_index) {
            // also render the cursor
            const icon_position = makeCoord(start_position.x - self._cursor_icon.width, start_position.y);
            textures.renderSprite(self._renderer, self._cursor_icon, icon_position);
            return self._menu_state.enter_pressed;
        }
        return false;
    }
};
