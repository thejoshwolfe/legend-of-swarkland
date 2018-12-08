const std = @import("std");
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const gui = @import("./gui.zig");
const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const hashU32 = core.geometry.hashU32;
const InputEngine = @import("./input_engine.zig").InputEngine;
const Button = @import("./input_engine.zig").Button;
const GameEngineClient = core.game_engine_client.GameEngineClient;
const Floor = core.game_state.Floor;
const Wall = core.game_state.Wall;

pub fn main() anyerror!void {
    core.debug.init();
    core.debug.nameThisThread("gui");
    core.debug.warn("init\n");
    defer core.debug.warn("shutdown\n");

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
    const aesthetic_seed = 0xbee894fc;
    var input_engine = InputEngine.init();
    var inputs_considered_harmful = true;

    const GameState = union(enum) {
        main_menu: gui.LinearMenuState,

        running: Running,
        const Running = struct {
            g: GameEngineClient,
            started_attack: bool,
        };
    };
    var game_state = GameState{ .main_menu = gui.LinearMenuState.init() };
    defer switch (game_state) {
        GameState.running => |*state| state.g.stopEngine(),
        else => {},
    };

    while (true) {
        // TODO: use better source of time (that doesn't crash after running for a month)
        const now = @intCast(i32, sdl.c.SDL_GetTicks());
        switch (game_state) {
            GameState.main_menu => |*main_menu_state| {
                main_menu_state.beginFrame();
            },
            GameState.running => |*state| {
                state.g.pollEvents(now);
            },
        }

        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    return;
                },
                sdl.c.SDL_WINDOWEVENT => {
                    switch (event.window.event) {
                        sdl.c.SDL_WINDOWEVENT_FOCUS_GAINED => {
                            inputs_considered_harmful = true;
                        },
                        else => {},
                    }
                },
                sdl.c.SDL_KEYDOWN, sdl.c.SDL_KEYUP => {
                    if (input_engine.handleEvent(event)) |button| {
                        if (inputs_considered_harmful) {
                            // when we first get focus, SDL gives a friendly digest of all the buttons that already held down.
                            // these are not inputs for us.
                            continue;
                        }
                        switch (game_state) {
                            GameState.main_menu => |*main_menu_state| {
                                switch (button) {
                                    Button.up => {
                                        main_menu_state.moveUp();
                                    },
                                    Button.down => {
                                        main_menu_state.moveDown();
                                    },
                                    Button.enter => {
                                        main_menu_state.enter();
                                    },
                                    else => {},
                                }
                            },
                            GameState.running => |*state| {
                                switch (button) {
                                    Button.left => {
                                        if (state.started_attack) {
                                            try state.g.attack(makeCoord(-1, 0));
                                            state.started_attack = false;
                                        } else {
                                            try state.g.move(makeCoord(-1, 0));
                                        }
                                    },
                                    Button.right => {
                                        if (state.started_attack) {
                                            try state.g.attack(makeCoord(1, 0));
                                            state.started_attack = false;
                                        } else {
                                            try state.g.move(makeCoord(1, 0));
                                        }
                                    },
                                    Button.up => {
                                        if (state.started_attack) {
                                            try state.g.attack(makeCoord(0, -1));
                                            state.started_attack = false;
                                        } else {
                                            try state.g.move(makeCoord(0, -1));
                                        }
                                    },
                                    Button.down => {
                                        if (state.started_attack) {
                                            try state.g.attack(makeCoord(0, 1));
                                            state.started_attack = false;
                                        } else {
                                            try state.g.move(makeCoord(0, 1));
                                        }
                                    },
                                    Button.start_attack => {
                                        state.started_attack = true;
                                    },
                                    Button.backspace => {
                                        try state.g.rewind();
                                    },
                                    Button.escape => {
                                        state.started_attack = false;
                                    },
                                    else => {},
                                }
                            },
                        }
                    }
                },
                else => {},
            }
        }

        _ = sdl.c.SDL_RenderClear(renderer);

        switch (game_state) {
            GameState.main_menu => |*main_menu_state| {
                var menu_renderer = gui.Gui.init(renderer, main_menu_state, textures.sprites.shiny_purple_wand);

                menu_renderer.seek(10, 10);
                menu_renderer.scale(2);
                menu_renderer.bold(true);
                menu_renderer.marginBottom(5);
                menu_renderer.text("Legend of Swarkland");
                menu_renderer.scale(1);
                menu_renderer.bold(false);
                menu_renderer.seekRelative(70, 0);
                if (menu_renderer.button("New Game (Thread)")) {
                    game_state = GameState{
                        .running = GameState.Running{
                            .g = undefined,
                            .started_attack = false,
                        },
                    };
                    switch (game_state) {
                        GameState.running => |*state| {
                            try state.g.startAsThread();
                        },
                        else => unreachable,
                    }
                }
                if (menu_renderer.button("Attach to Game (Process)")) {
                    game_state = GameState{ .running = undefined };
                    switch (game_state) {
                        GameState.running => |*state| {
                            try state.g.startAsChildProcess();
                        },
                        else => unreachable,
                    }
                }
                if (menu_renderer.button("Quit")) {
                    // quit
                    return;
                }
            },
            GameState.running => |state| {
                // render terrain
                for (state.g.game_state.terrain.floor) |row, y| {
                    for (row) |cell, x| {
                        const coord = makeCoord(@intCast(i32, x), @intCast(i32, y));
                        const texture = switch (cell) {
                            Floor.unknown => textures.sprites.unknown_floor,
                            Floor.dirt => selectAesthetic(textures.sprites.dirt_floor[0..], aesthetic_seed, coord),
                            Floor.marble => selectAesthetic(textures.sprites.marble_floor[0..], aesthetic_seed, coord),
                            Floor.lava => selectAesthetic(textures.sprites.lava[0..], aesthetic_seed, coord),
                        };
                        textures.renderSprite(renderer, texture, coord.scaled(32));
                    }
                }
                for (state.g.game_state.terrain.walls) |row, y| {
                    for (row) |cell, x| {
                        const coord = makeCoord(@intCast(i32, x), @intCast(i32, y));
                        const texture = switch (cell) {
                            Wall.unknown => textures.sprites.unknown_wall,
                            Wall.air => continue,
                            Wall.dirt => selectAesthetic(textures.sprites.brown_brick[0..], aesthetic_seed, coord),
                            Wall.stone => selectAesthetic(textures.sprites.gray_brick[0..], aesthetic_seed, coord),
                        };
                        textures.renderSprite(renderer, texture, coord.scaled(32));
                    }
                }

                // render the things
                for (state.g.game_state.player_positions) |p, i| {
                    var display_position = p.scaled(32);
                    if (state.g.move_animations[i]) |animation| blk: {
                        const duration = animation.end_time - animation.start_time;
                        const progress = now - animation.start_time;
                        if (progress > duration) {
                            break :blk;
                        }
                        const vector = animation.to.minus(animation.from).scaled(32);
                        display_position = animation.from.scaled(32).plus(vector.scaled(progress).scaledDivTrunc(duration));
                    }
                    if (i == 0) {
                        textures.renderSprite(renderer, textures.sprites.human, display_position);
                        if (state.started_attack) {
                            textures.renderSprite(renderer, textures.sprites.dagger, display_position);
                        }
                    } else {
                        textures.renderSprite(renderer, textures.sprites.ogre, display_position);
                    }
                }
                for (state.g.attack_animations) |a, i| {
                    if (a) |animation| {
                        const duration = animation.end_time - animation.start_time;
                        const progress = now - animation.start_time;
                        if (progress > duration) {
                            continue;
                        }
                        const display_position = animation.location.scaled(32);
                        textures.renderSpriteRotated(renderer, textures.sprites.dagger, display_position, animation.rotation +% 1);
                    }
                }
            },
        }

        sdl.c.SDL_RenderPresent(renderer);

        // delay until the next multiple of 17 milliseconds
        const delay_millis = 17 - (sdl.c.SDL_GetTicks() % 17);
        sdl.c.SDL_Delay(delay_millis);
        inputs_considered_harmful = false;
    }
}

fn selectAesthetic(array: []const Rect, seed: u32, coord: Coord) Rect {
    var hash = seed;
    hash ^= @bitCast(u32, coord.x);
    hash = hashU32(hash);
    hash ^= @bitCast(u32, coord.y);
    hash = hashU32(hash);
    return array[@intCast(usize, std.rand.limitRangeBiased(u32, hash, @intCast(u32, array.len)))];
}
