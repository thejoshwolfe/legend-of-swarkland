const std = @import("std");
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const gui = @import("./gui.zig");
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

    const renderer: *sdl.Renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyRenderer(renderer);

    textures.init(renderer);
    defer textures.deinit();

    doMainLoop(renderer);
}

fn doMainLoop(renderer: *sdl.Renderer) void {
    var main_menu_state = gui.LinearMenuState.init();

    while (true) {
        main_menu_state.beginFrame();
        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    return;
                },
                sdl.c.SDL_KEYDOWN => {
                    switch (@enumToInt(event.key.keysym.scancode)) {
                        sdl.c.SDL_SCANCODE_UP => {
                            main_menu_state.moveUp();
                        },
                        sdl.c.SDL_SCANCODE_DOWN => {
                            main_menu_state.moveDown();
                        },
                        sdl.c.SDL_SCANCODE_RETURN => {
                            main_menu_state.enter();
                        },
                        else => {},
                    }
                },
                else => {},
            }
        }

        _ = sdl.c.SDL_RenderClear(renderer);

        var menuRenderer = gui.Gui.init(renderer, &main_menu_state, textures.sprites.shiny_purple_wand);

        menuRenderer.seek(10, 10);
        menuRenderer.scale(2);
        menuRenderer.bold(true);
        menuRenderer.marginBottom(5);
        menuRenderer.text("Legend of Swarkland");
        menuRenderer.scale(1);
        menuRenderer.bold(false);
        menuRenderer.seekRelative(70, 0);
        if (menuRenderer.button("New Game")) {
            // actually quit
            return;
        }
        if (menuRenderer.button("Load Yagni")) {
            // actually quit
            return;
        }
        if (menuRenderer.button("Quit")) {
            // quit
            return;
        }

        sdl.c.SDL_RenderPresent(renderer);

        // delay until the next multiple of 17 milliseconds
        const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
        sdl.c.SDL_Delay(delay_millis);
    }
}
