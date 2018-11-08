const std = @import("std");
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const gui = @import("./gui.zig");
const makeCoord = @import("core").geometry.makeCoord;
const GameEngine = @import("core").game_engine_client.GameEngine;

const GameState = enum.{
    MainMenu,
    Running,
};

pub fn displayMain() !void {
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

    try doMainLoop(renderer);
}

fn doMainLoop(renderer: *sdl.Renderer) !void {
    var game_state = GameState.MainMenu;
    var main_menu_state = gui.LinearMenuState.init();
    var game_engine: ?GameEngine = null;
    defer if (game_engine) |g| {
        _ = g.child_process.kill() catch undefined;
    };

    while (true) {
        main_menu_state.beginFrame();
        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    return;
                },
                sdl.c.SDL_KEYDOWN => {
                    switch (game_state) {
                        GameState.MainMenu => {
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
                        GameState.Running => {
                            switch (@enumToInt(event.key.keysym.scancode)) {
                                sdl.c.SDL_SCANCODE_LEFT => {
                                    try game_engine.?.move(-1);
                                },
                                sdl.c.SDL_SCANCODE_RIGHT => {
                                    try game_engine.?.move(1);
                                },
                                else => {},
                            }
                        },
                    }
                },
                else => {},
            }
        }

        _ = sdl.c.SDL_RenderClear(renderer);

        switch (game_state) {
            GameState.MainMenu => {
                var menu_renderer = gui.Gui.init(renderer, &main_menu_state, textures.sprites.shiny_purple_wand);

                menu_renderer.seek(10, 10);
                menu_renderer.scale(2);
                menu_renderer.bold(true);
                menu_renderer.marginBottom(5);
                menu_renderer.text("Legend of Swarkland");
                menu_renderer.scale(1);
                menu_renderer.bold(false);
                menu_renderer.seekRelative(70, 0);
                if (menu_renderer.button("New Game")) {
                    game_state = GameState.Running;
                    game_engine = GameEngine(undefined);
                    try game_engine.?.startEngine();
                }
                if (menu_renderer.button("Load Yagni")) {
                    // actually quit
                    return;
                }
                if (menu_renderer.button("Quit")) {
                    // quit
                    return;
                }
            },
            GameState.Running => {
                textures.renderSprite(renderer, textures.sprites.human, makeCoord(game_engine.?.position * 32 + 128, 128));
            },
        }

        sdl.c.SDL_RenderPresent(renderer);

        // delay until the next multiple of 17 milliseconds
        const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
        sdl.c.SDL_Delay(delay_millis);
    }
}
