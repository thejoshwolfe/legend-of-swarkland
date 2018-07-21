const std = @import("std");
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const makeCoord = @import("core").geometry.makeCoord;

pub fn display_main() void {
    if (sdl.c.SDL_Init(sdl.c.SDL_INIT_VIDEO) != 0) {
        std.debug.panic("SDL_Init failed: {c}\n", sdl.c.SDL_GetError());
    }
    defer sdl.c.SDL_Quit();

    const screen = sdl.c.SDL_CreateWindow(
        c"Legend of Swarkland",
        sdl.SDL_WINDOWPOS_UNDEFINED,
        sdl.SDL_WINDOWPOS_UNDEFINED,
        512,
        512,
        sdl.c.SDL_WINDOW_OPENGL | sdl.c.SDL_WINDOW_RESIZABLE,
    ) orelse {
        std.debug.panic("SDL_CreateWindow failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyWindow(screen);

    const renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyRenderer(renderer);

    textures.init(renderer);
    defer textures.deinit();

    while (true) {
        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    return;
                },
                else => {},
            }
        }

        _ = sdl.c.SDL_RenderClear(renderer);

        var cursor: i32 = 10;
        cursor = 10 + textures.render_text_scaled(renderer, "Legend of Swarkland", makeCoord(10, cursor), true, 2).y;
        cursor = 5 + textures.render_text(renderer, "New Game", makeCoord(80, cursor), false).y;
        cursor = 5 + textures.render_text(renderer, "Load Yagni", makeCoord(80, cursor), false).y;
        cursor = 5 + textures.render_text(renderer, "Quit", makeCoord(80, cursor), false).y;

        sdl.c.SDL_RenderPresent(renderer);

        // delay until the next multiple of 17 milliseconds
        const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
        sdl.c.SDL_Delay(delay_millis);
    }
}
