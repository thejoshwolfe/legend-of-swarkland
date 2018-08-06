const sdl = @import("./sdl.zig");
const geometry = @import("core").geometry;
const textures = @import("./textures.zig");

pub const Gui = struct {
    _renderer: *sdl.Renderer,
    _cursor: geometry.Coord,
    _scale: i32,
    _bold: bool,
    _marginBottom: i32,
    pub fn init(renderer: *sdl.Renderer) Gui {
        return Gui{
            ._renderer = renderer,
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
        self._cursor.y = textures.render_text_scaled(self._renderer, string, self._cursor, self._bold, self._scale).y;
        self._cursor.y += self._marginBottom * self._scale;
    }
};
