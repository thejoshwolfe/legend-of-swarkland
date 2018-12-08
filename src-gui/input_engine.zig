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
};

pub const InputEngine = struct {
    const button_count = @memberCount(Button);
    button_states: [button_count]bool,

    pub fn init() InputEngine {
        return InputEngine{ .button_states = []bool{false} ** button_count };
    }

    pub fn handleEvent(self: *InputEngine, event: sdl.c.SDL_Event) ?Button {
        switch (event.@"type") {
            sdl.c.SDL_KEYUP => {
                if (self.scancodeToButton(@enumToInt(event.key.keysym.scancode))) |button| {
                    self.button_states[@enumToInt(button)] = false;
                }
                return null;
            },
            sdl.c.SDL_KEYDOWN => {
                // TODO
                const button = self.scancodeToButton(@enumToInt(event.key.keysym.scancode)) orelse return null;
                if (self.button_states[@enumToInt(button)]) {
                    // keyrepeat
                    return null;
                }
                self.button_states[@enumToInt(button)] = true;
                return button;
            },
            else => unreachable,
        }
    }

    fn scancodeToButton(self: *InputEngine, scancode: c_int) ?Button {
        switch (scancode) {
            sdl.c.SDL_SCANCODE_LEFT => return Button.left,
            sdl.c.SDL_SCANCODE_RIGHT => return Button.right,
            sdl.c.SDL_SCANCODE_UP => return Button.up,
            sdl.c.SDL_SCANCODE_DOWN => return Button.down,
            sdl.c.SDL_SCANCODE_F => return Button.start_attack,
            sdl.c.SDL_SCANCODE_BACKSPACE => return Button.backspace,
            sdl.c.SDL_SCANCODE_RETURN => return Button.enter,
            sdl.c.SDL_SCANCODE_ESCAPE => return Button.escape,
            else => return null,
        }
    }
};
