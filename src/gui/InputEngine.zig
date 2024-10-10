const std = @import("std");
const core = @import("core");
const sdl = @import("./sdl.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;

const InputEngine = @This();

inputs_considered_harmful: bool = true,

pub const Input = enum {
    shutdown,

    up,
    down,
    left,
    right,
    shift_up,
    shift_down,
    shift_left,
    shift_right,
    greaterthan,

    start_attack,
    start_kick,
    start_open_close,
    start_defend,
    charge,
    stomp,
    pick_up,
    backspace,
    spacebar,
    enter,
    escape,
    restart,
    quit,
    beat_level,
    beat_level_5,
    unbeat_level,
    unbeat_level_5,
    page_up,
    page_down,
    home,
    end,

    warp_0,
    warp_1,
    warp_2,
    warp_3,
    warp_4,
    warp_5,
    warp_6,
    warp_7,

    equip_0,
    equip_1,
    equip_2,
    equip_3,
    equip_4,
    equip_5,
    equip_6,
    equip_7,

    force_equip_0,
    force_equip_1,
    force_equip_2,
    force_equip_3,
    force_equip_4,
    force_equip_5,
    force_equip_6,
    force_equip_7,
};

pub fn init() InputEngine {
    return InputEngine{};
}

pub fn now(_: *const InputEngine) i32 {
    // TODO: use better source of time (that doesn't crash after running for a month)
    return @as(i32, @intCast(sdl.c.SDL_GetTicks()));
}

pub fn sleepUntilNextFrame(self: *InputEngine) void {
    // delay until the next multiple of 17 milliseconds
    const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
    sdl.c.SDL_Delay(delay_millis);
    self.inputs_considered_harmful = false;
}

pub fn pollInput(self: *InputEngine) ?Input {
    var event: sdl.c.SDL_Event = undefined;
    while (sdl.SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            sdl.c.SDL_QUIT => {
                return .shutdown;
            },
            sdl.c.SDL_WINDOWEVENT => {
                switch (event.window.event) {
                    sdl.c.SDL_WINDOWEVENT_FOCUS_GAINED => {
                        self.inputs_considered_harmful = true;
                    },
                    else => {},
                }
            },
            sdl.c.SDL_KEYDOWN => {
                if (self.inputs_considered_harmful) {
                    // when we first get focus, SDL gives a friendly digest of all the buttons that are already held down.
                    // these are not inputs for us.
                    continue;
                }
                if (event.key.repeat != 0) continue;
                return self.fromKeydown(self.getModifiers(), event.key.keysym.scancode);
            },
            else => {},
        }
    }
    return null;
}

fn fromKeydown(_: InputEngine, modifiers: Modifiers, scancode: c_uint) ?Input {
    switch (scancode) {
        sdl.c.SDL_SCANCODE_LEFT => if (modifiers == 0)
            return .left
        else if (modifiers == shift)
            return .shift_left
        else
            return null,
        sdl.c.SDL_SCANCODE_RIGHT => if (modifiers == 0)
            return .right
        else if (modifiers == shift)
            return .shift_right
        else
            return null,
        sdl.c.SDL_SCANCODE_UP => if (modifiers == 0)
            return .up
        else if (modifiers == shift)
            return .shift_up
        else if (modifiers == ctrl)
            return .page_up
        else
            return null,
        sdl.c.SDL_SCANCODE_DOWN => if (modifiers == 0)
            return .down
        else if (modifiers == shift)
            return .shift_down
        else if (modifiers == ctrl)
            return .page_down
        else
            return null,
        sdl.c.SDL_SCANCODE_F => return if (modifiers == 0) .start_attack else null,
        sdl.c.SDL_SCANCODE_K => return if (modifiers == 0) .start_kick else null,
        sdl.c.SDL_SCANCODE_O => return if (modifiers == 0) .start_open_close else null,
        sdl.c.SDL_SCANCODE_C => return if (modifiers == 0) .charge else null,
        sdl.c.SDL_SCANCODE_S => return if (modifiers == 0) .stomp else null,
        sdl.c.SDL_SCANCODE_R => if (modifiers == 0)
            return .restart
        else if (modifiers == ctrl)
            return .quit
        else
            return null,
        sdl.c.SDL_SCANCODE_G => return if (modifiers == 0) .pick_up else null,
        sdl.c.SDL_SCANCODE_D => return if (modifiers == 0) .start_defend else null,
        sdl.c.SDL_SCANCODE_PERIOD => if (modifiers == 0)
            return null // .
        else if (modifiers == shift)
            return .greaterthan // >
        else
            return null,
        sdl.c.SDL_SCANCODE_SPACE => return if (modifiers == 0) .spacebar else null,
        sdl.c.SDL_SCANCODE_BACKSPACE => return if (modifiers == 0) .backspace else null,
        sdl.c.SDL_SCANCODE_RETURN => return if (modifiers == 0) .enter else null,
        sdl.c.SDL_SCANCODE_ESCAPE => return if (modifiers == 0) .escape else null,
        sdl.c.SDL_SCANCODE_LEFTBRACKET => if (modifiers == shift)
            return .unbeat_level
        else if (modifiers == shift | ctrl)
            return .unbeat_level_5
        else
            return null,
        sdl.c.SDL_SCANCODE_RIGHTBRACKET => if (modifiers == shift)
            return .beat_level
        else if (modifiers == shift | ctrl)
            return .beat_level_5
        else
            return null,
        sdl.c.SDL_SCANCODE_PAGEUP => return if (modifiers == 0) .page_up else null,
        sdl.c.SDL_SCANCODE_PAGEDOWN => return if (modifiers == 0) .page_down else null,
        sdl.c.SDL_SCANCODE_HOME => return if (modifiers == 0) .home else null,
        sdl.c.SDL_SCANCODE_END => return if (modifiers == 0) .end else null,

        sdl.c.SDL_SCANCODE_1 => return if (modifiers == ctrl) .warp_0 else if (modifiers == shift) .force_equip_0 else if (modifiers == 0) .equip_0 else null,
        sdl.c.SDL_SCANCODE_2 => return if (modifiers == ctrl) .warp_1 else if (modifiers == shift) .force_equip_1 else if (modifiers == 0) .equip_1 else null,
        sdl.c.SDL_SCANCODE_3 => return if (modifiers == ctrl) .warp_2 else if (modifiers == shift) .force_equip_2 else if (modifiers == 0) .equip_2 else null,
        sdl.c.SDL_SCANCODE_4 => return if (modifiers == ctrl) .warp_3 else if (modifiers == shift) .force_equip_3 else if (modifiers == 0) .equip_3 else null,
        sdl.c.SDL_SCANCODE_5 => return if (modifiers == ctrl) .warp_4 else if (modifiers == shift) .force_equip_4 else if (modifiers == 0) .equip_4 else null,
        sdl.c.SDL_SCANCODE_6 => return if (modifiers == ctrl) .warp_5 else if (modifiers == shift) .force_equip_5 else if (modifiers == 0) .equip_5 else null,
        sdl.c.SDL_SCANCODE_7 => return if (modifiers == ctrl) .warp_6 else if (modifiers == shift) .force_equip_6 else if (modifiers == 0) .equip_6 else null,
        sdl.c.SDL_SCANCODE_8 => return if (modifiers == ctrl) .warp_7 else if (modifiers == shift) .force_equip_7 else if (modifiers == 0) .equip_7 else null,

        else => return null,
    }
}

const Modifiers = u4;
const shift = 1;
const ctrl = 2;
const alt = 3;
const meta = 4;

fn getModifiers(_: InputEngine) Modifiers {
    const sdl_modifiers = sdl.c.SDL_GetModState();
    var result: Modifiers = 0;
    if (sdl_modifiers & (sdl.c.KMOD_LSHIFT | sdl.c.KMOD_RSHIFT) != 0) result |= shift;
    if (sdl_modifiers & (sdl.c.KMOD_LCTRL | sdl.c.KMOD_RCTRL) != 0) result |= ctrl;
    if (sdl_modifiers & (sdl.c.KMOD_LALT | sdl.c.KMOD_RALT) != 0) result |= alt;
    if (sdl_modifiers & (sdl.c.KMOD_LGUI | sdl.c.KMOD_RGUI) != 0) result |= meta;

    return result;
}
