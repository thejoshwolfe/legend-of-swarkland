const std = @import("std");
const sdl = @import("./sdl.zig");
const core = @import("core");
const geometry = core.geometry;
const Coord = geometry.Coord;
const Rect = geometry.Rect;
const makeCoord = geometry.makeCoord;
const textures = @import("./textures.zig");
const Font = textures.Font;
const InputEngine = @import("InputEngine.zig");

pub const LinearMenuState = struct {
    cursor_position: usize = 0,
    entry_count: usize = 0,
    enter_pressed: bool = false,
    buffered_cheatcode: ?u64 = null,

    pub fn handleInput(self: *LinearMenuState, input: InputEngine.Input) ?enum { back } {
        switch (input) {
            .up => {
                self.moveUp(1);
            },
            .down => {
                self.moveDown(1);
            },
            .page_up => {
                self.moveUp(5);
            },
            .page_down => {
                self.moveDown(5);
            },
            .home => {
                self.cursor_position = 0;
            },
            .end => {
                self.cursor_position = self.entry_count -| 1;
            },
            .enter => {
                self.enter();
            },
            .escape => {
                return .back;
            },
            .equip_0 => self.buffered_cheatcode = 1,
            .equip_1 => self.buffered_cheatcode = 2,
            .equip_2 => self.buffered_cheatcode = 3,
            .equip_3 => self.buffered_cheatcode = 4,
            .equip_4 => self.buffered_cheatcode = 5,
            .equip_5 => self.buffered_cheatcode = 6,
            .equip_6 => self.buffered_cheatcode = 7,
            .equip_7 => self.buffered_cheatcode = null,
            else => {},
        }
        return null;
    }

    fn moveUp(self: *LinearMenuState, how_many: usize) void {
        if (self.cursor_position < how_many) {
            self.cursor_position = 0;
        } else {
            self.cursor_position -= how_many;
        }
    }
    fn moveDown(self: *LinearMenuState, how_many: usize) void {
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
    fn enter(self: *LinearMenuState) void {
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

pub const Window = struct {
    screen: *sdl.c.SDL_Window,
    renderer: *sdl.Renderer,
    screen_buffer: *sdl.Texture,

    pub fn init(title: [:0]const u8, width: i32, height: i32) !Window {
        // SDL handling SIGINT blocks propagation to child threads.
        if (!(sdl.c.SDL_SetHintWithPriority(sdl.c.SDL_HINT_NO_SIGNAL_HANDLERS, "1", sdl.c.SDL_HINT_OVERRIDE) != sdl.c.SDL_FALSE)) {
            std.debug.panic("failed to disable sdl signal handlers\n", .{});
        }
        if (sdl.c.SDL_Init(sdl.c.SDL_INIT_VIDEO) != 0) {
            std.debug.panic("SDL_Init failed: {s}\n", .{sdl.c.SDL_GetError()});
        }
        errdefer sdl.c.SDL_Quit();

        const screen = sdl.c.SDL_CreateWindow(
            title,
            sdl.SDL_WINDOWPOS_UNDEFINED,
            sdl.SDL_WINDOWPOS_UNDEFINED,
            width,
            height,
            sdl.c.SDL_WINDOW_RESIZABLE,
        ) orelse {
            std.debug.panic("SDL_CreateWindow failed: {s}\n", .{sdl.c.SDL_GetError()});
        };
        errdefer sdl.c.SDL_DestroyWindow(screen);

        const renderer: *sdl.Renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
            std.debug.panic("SDL_CreateRenderer failed: {s}\n", .{sdl.c.SDL_GetError()});
        };
        errdefer sdl.c.SDL_DestroyRenderer(renderer);

        // Make sure this works.
        {
            var renderer_info: sdl.c.SDL_RendererInfo = undefined;
            sdl.assertZero(sdl.c.SDL_GetRendererInfo(renderer, &renderer_info));
            if (renderer_info.flags & @as(u32, @bitCast(sdl.c.SDL_RENDERER_TARGETTEXTURE)) == 0) {
                std.debug.panic("rendering to a temporary texture is not supported", .{});
            }
        }

        const screen_buffer: *sdl.Texture = sdl.c.SDL_CreateTexture(
            renderer,
            sdl.c.SDL_PIXELFORMAT_ABGR8888,
            sdl.c.SDL_TEXTUREACCESS_TARGET,
            width,
            height,
        ) orelse {
            std.debug.panic("SDL_CreateTexture failed: {s}\n", .{sdl.c.SDL_GetError()});
        };
        errdefer sdl.c.SDL_DestroyTexture(screen_buffer);

        beginFrame(renderer, screen_buffer);

        return Window{
            .screen = screen,
            .renderer = renderer,
            .screen_buffer = screen_buffer,
        };
    }
    pub fn deinit(window: *Window) void {
        sdl.c.SDL_DestroyTexture(window.screen_buffer);
        sdl.c.SDL_DestroyRenderer(window.renderer);
        sdl.c.SDL_DestroyWindow(window.screen);
        sdl.c.SDL_Quit();
        window.* = undefined;
    }

    fn beginFrame(renderer: *sdl.Renderer, screen_buffer: *sdl.Texture) void {
        sdl.assertZero(sdl.c.SDL_SetRenderTarget(renderer, screen_buffer));
        sdl.assertZero(sdl.c.SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
        sdl.assertZero(sdl.c.SDL_RenderClear(renderer));
    }

    pub fn present(window: *const Window) void {
        var logical_window_size = sdl.c.SDL_Rect{
            .x = 0,
            .y = 0,
            .w = undefined,
            .h = undefined,
        };
        sdl.assertZero(sdl.c.SDL_GetRendererOutputSize(window.renderer, &logical_window_size.w, &logical_window_size.h));
        // Switch to rendering to the window.
        sdl.assertZero(sdl.c.SDL_SetRenderTarget(window.renderer, null));
        var output_rect = sdl.c.SDL_Rect{
            .x = 0,
            .y = 0,
            .w = undefined,
            .h = undefined,
        };
        sdl.assertZero(sdl.c.SDL_GetRendererOutputSize(window.renderer, &output_rect.w, &output_rect.h));
        // Center the screen buffer preserving the aspect ratio.
        const source_aspect_ratio = @as(f32, @floatFromInt(logical_window_size.w)) / @as(f32, @floatFromInt(logical_window_size.h));
        const dest_aspect_ratio = @as(f32, @floatFromInt(output_rect.w)) / @as(f32, @floatFromInt(output_rect.h));
        if (source_aspect_ratio > dest_aspect_ratio) {
            // Scale width exactly. Letterbox on top/bottom.
            const new_height = @as(c_int, @intFromFloat(@as(f32, @floatFromInt(output_rect.w)) / source_aspect_ratio));
            output_rect.x = 0;
            output_rect.y = @divTrunc(output_rect.h - new_height, 2);
            output_rect.h = new_height;
        } else {
            // Scale height exactly. Letterbox on left/right.
            const new_width = @as(c_int, @intFromFloat(@as(f32, @floatFromInt(output_rect.h)) * source_aspect_ratio));
            output_rect.x = @divTrunc(output_rect.w - new_width, 2);
            output_rect.y = 0;
            output_rect.w = new_width;
        }

        // Blit and present.
        sdl.assertZero(sdl.c.SDL_SetRenderDrawColor(window.renderer, 0, 0, 0, 255));
        sdl.assertZero(sdl.c.SDL_RenderClear(window.renderer));
        sdl.assertZero(sdl.c.SDL_RenderCopy(window.renderer, window.screen_buffer, &logical_window_size, &output_rect));
        sdl.c.SDL_RenderPresent(window.renderer);

        // Switch back to rendering to the screen buffer.
        beginFrame(window.renderer, window.screen_buffer);
    }
};
