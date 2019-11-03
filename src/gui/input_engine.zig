const std = @import("std");
const core = @import("core");
const sdl = @import("./sdl.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;

pub const Button = enum {
    up,
    down,
    left,
    right,
    start_attack,
    backspace,
    enter,
    escape,
    restart,
};

/// TODO: suppress key repeat
pub const InputEngine = struct {
    pub fn init() InputEngine {
        return InputEngine{};
    }

    pub fn handleEvent(self: *InputEngine, event: sdl.c.SDL_Event) ?Button {
        switch (event.@"type") {
            sdl.c.SDL_KEYUP => {
                return null;
            },
            sdl.c.SDL_KEYDOWN => {
                return self.scancodeToButton(self.getModifiers(), @enumToInt(event.key.keysym.scancode)) orelse return null;
            },
            else => unreachable,
        }
    }

    fn scancodeToButton(self: InputEngine, modifiers: Modifiers, scancode: c_int) ?Button {
        switch (scancode) {
            sdl.c.SDL_SCANCODE_LEFT => return if (modifiers == 0) Button.left else null,
            sdl.c.SDL_SCANCODE_RIGHT => return if (modifiers == 0) Button.right else null,
            sdl.c.SDL_SCANCODE_UP => return if (modifiers == 0) Button.up else null,
            sdl.c.SDL_SCANCODE_DOWN => return if (modifiers == 0) Button.down else null,
            sdl.c.SDL_SCANCODE_F => return if (modifiers == 0) Button.start_attack else null,
            sdl.c.SDL_SCANCODE_R => return if (modifiers == ctrl) Button.restart else null,
            sdl.c.SDL_SCANCODE_BACKSPACE => return if (modifiers == 0) Button.backspace else null,
            sdl.c.SDL_SCANCODE_RETURN => return if (modifiers == 0) Button.enter else null,
            sdl.c.SDL_SCANCODE_ESCAPE => return if (modifiers == 0) Button.escape else null,
            else => return null,
        }
    }

    const Modifiers = u4;
    const shift = 1;
    const ctrl = 2;
    const alt = 3;
    const meta = 4;

    fn getModifiers(self: InputEngine) Modifiers {
        const sdl_modifiers = @enumToInt(sdl.c.SDL_GetModState());
        var result: Modifiers = 0;
        if (sdl_modifiers & (sdl.c.KMOD_LSHIFT | sdl.c.KMOD_RSHIFT) != 0) result |= shift;
        if (sdl_modifiers & (sdl.c.KMOD_LCTRL | sdl.c.KMOD_RCTRL) != 0) result |= ctrl;
        if (sdl_modifiers & (sdl.c.KMOD_LALT | sdl.c.KMOD_RALT) != 0) result |= alt;
        if (sdl_modifiers & (sdl.c.KMOD_LGUI | sdl.c.KMOD_RGUI) != 0) result |= meta;

        return result;
    }
};
