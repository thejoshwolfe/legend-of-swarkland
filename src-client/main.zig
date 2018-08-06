const std = @import("std");
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const Gui = @import("./gui.zig").Gui;
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

        var menuRenderer = Gui.init(renderer);

        menuRenderer.seek(10, 10);
        menuRenderer.scale(2);
        menuRenderer.bold(true);
        menuRenderer.marginBottom(5);
        menuRenderer.text("Legend of Swarkland");
        menuRenderer.scale(1);
        menuRenderer.bold(false);
        menuRenderer.seekRelative(70, 0);
        menuRenderer.text("New Game");
        menuRenderer.text("Load Yagni");
        menuRenderer.text("Quit");

        sdl.c.SDL_RenderPresent(renderer);

        // delay until the next multiple of 17 milliseconds
        const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
        sdl.c.SDL_Delay(delay_millis);
    }
}
