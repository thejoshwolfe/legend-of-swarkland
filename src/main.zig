const c = @cImport({
    @cInclude("SDL2/SDL.h");
});
const std = @import("std");
const assert = std.debug.assert;
const spritesheet = @import("../zig-cache/spritesheet.zig");

// See https://github.com/zig-lang/zig/issues/565
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000u
const SDL_WINDOWPOS_UNDEFINED = @bitCast(c_int, c.SDL_WINDOWPOS_UNDEFINED_MASK);

extern fn SDL_PollEvent(event: *c.SDL_Event) c_int;

// SDL_RWclose is fundamentally unrepresentable in Zig, because `ctx` is
// evaluated twice. One could make the case that this is a bug in SDL,
// especially since the docs list a real function prototype that would not
// have this double-evaluation of the parameter.
// If SDL would instead of a macro use a static inline function,
// it would resolve the SDL bug as well as make the function visible to Zig
// and to debuggers.
// SDL_rwops.h:#define SDL_RWclose(ctx)        (ctx)->close(ctx)
inline fn SDL_RWclose(ctx: [*]c.SDL_RWops) c_int {
    return ctx[0].close.?(ctx);
}

pub fn main() void {
    if (c.SDL_Init(c.SDL_INIT_VIDEO) != 0) {
        std.debug.panic("SDL_Init failed: {c}\n", c.SDL_GetError());
    }
    defer c.SDL_Quit();

    const screen = c.SDL_CreateWindow(c"My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, spritesheet.width, spritesheet.height, c.SDL_WINDOW_OPENGL) orelse {
        std.debug.panic("SDL_CreateWindow failed: {c}\n", c.SDL_GetError());
    };
    defer c.SDL_DestroyWindow(screen);

    const renderer = c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {c}\n", c.SDL_GetError());
    };
    defer c.SDL_DestroyRenderer(renderer);

    const spritesheet_buffer = @embedFile("../zig-cache/spritesheet_resource");

    var texture: *c.SDL_Texture = c.SDL_CreateTexture(renderer, c.SDL_PIXELFORMAT_RGBA8888, c.SDL_TEXTUREACCESS_STATIC, spritesheet.width, spritesheet.height) orelse {
        std.debug.panic("SDL_CreateTexture failed: {c}\n", c.SDL_GetError());
    };
    if (c.SDL_SetTextureBlendMode(texture, @intToEnum(c.SDL_BlendMode, c.SDL_BLENDMODE_BLEND)) != 0) {
        std.debug.panic("SDL_SetTextureBlendMode failed: {c}\n", c.SDL_GetError());
    }
    const pitch = spritesheet.width * 4;
    if (c.SDL_UpdateTexture(texture, null, @ptrCast(?*const c_void, spritesheet_buffer[0..].ptr), pitch) != 0) {
        std.debug.panic("SDL_UpdateTexture failed: {c}\n", c.SDL_GetError());
    }

    defer c.SDL_DestroyTexture(texture);

    var quit = false;
    while (!quit) {
        var event: c.SDL_Event = undefined;
        while (SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                c.SDL_QUIT => {
                    quit = true;
                },
                else => {},
            }
        }

        _ = c.SDL_RenderClear(renderer);
        _ = c.SDL_RenderCopy(renderer, texture, null, null);
        c.SDL_RenderPresent(renderer);

        c.SDL_Delay(17);
    }
}
