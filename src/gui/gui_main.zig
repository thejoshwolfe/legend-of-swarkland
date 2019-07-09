const std = @import("std");
const ArrayList = std.ArrayList;
const sdl = @import("./sdl.zig");
const textures = @import("./textures.zig");
const gui = @import("./gui.zig");
const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const directionToRotation = core.geometry.directionToRotation;
const hashU32 = core.geometry.hashU32;
const InputEngine = @import("./input_engine.zig").InputEngine;
const Button = @import("./input_engine.zig").Button;
const GameEngineClient = core.game_engine_client.GameEngineClient;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const Terrain = core.protocol.Terrain;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;

const allocator = std.heap.c_allocator;

const logical_window_size = sdl.makeRect(Rect{ .x = 0, .y = 0, .width = 512, .height = 512 });

/// changes when the window resizes
/// FIXME: should initialize to logical_window_size, but workaround https://github.com/ziglang/zig/issues/2855
var output_rect = sdl.makeRect(Rect{ .x = 0, .y = 0, .width = 512, .height = 512 });

pub fn main() anyerror!void {
    core.debug.init();
    core.debug.nameThisThread("gui");
    defer core.debug.unnameThisThread();
    core.debug.thread_lifecycle.print("init");
    defer core.debug.thread_lifecycle.print("shutdown");

    // SDL handling SIGINT blocks propagation to child threads.
    if (!(sdl.c.SDL_SetHintWithPriority(sdl.c.SDL_HINT_NO_SIGNAL_HANDLERS, c"1", sdl.c.SDL_HintPriority.SDL_HINT_OVERRIDE) != sdl.c.SDL_bool.SDL_FALSE)) {
        std.debug.panic("failed to disable sdl signal handlers\n");
    }
    if (sdl.c.SDL_Init(sdl.c.SDL_INIT_VIDEO) != 0) {
        std.debug.panic("SDL_Init failed: {c}\n", sdl.c.SDL_GetError());
    }
    defer sdl.c.SDL_Quit();

    const screen = sdl.c.SDL_CreateWindow(
        c"Legend of Swarkland",
        sdl.SDL_WINDOWPOS_UNDEFINED,
        sdl.SDL_WINDOWPOS_UNDEFINED,
        logical_window_size.w,
        logical_window_size.h,
        sdl.c.SDL_WINDOW_OPENGL | sdl.c.SDL_WINDOW_RESIZABLE,
    ) orelse {
        std.debug.panic("SDL_CreateWindow failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyWindow(screen);

    const renderer: *sdl.Renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyRenderer(renderer);

    {
        var renderer_info: sdl.c.SDL_RendererInfo = undefined;
        sdl.assertZero(sdl.c.SDL_GetRendererInfo(renderer, &renderer_info));
        if (renderer_info.flags & sdl.c.SDL_RENDERER_TARGETTEXTURE == 0) {
            std.debug.panic("rendering to a temporary texture is not supported");
        }
    }

    const screen_buffer: *sdl.Texture = sdl.c.SDL_CreateTexture(
        renderer,
        sdl.c.SDL_PIXELFORMAT_ABGR8888,
        sdl.c.SDL_TEXTUREACCESS_TARGET,
        logical_window_size.w,
        logical_window_size.h,
    ) orelse {
        std.debug.panic("SDL_CreateTexture failed: {c}\n", sdl.c.SDL_GetError());
    };
    defer sdl.c.SDL_DestroyTexture(screen_buffer);

    textures.init(renderer);
    defer textures.deinit();

    try doMainLoop(renderer, screen_buffer);
}

fn doMainLoop(renderer: *sdl.Renderer, screen_buffer: *sdl.Texture) !void {
    const aesthetic_seed = 0xbee894fc;
    var input_engine = InputEngine.init();
    var inputs_considered_harmful = true;

    const GameState = union(enum) {
        main_menu: gui.LinearMenuState,

        running: Running,
        const Running = struct {
            client: GameEngineClient,
            client_state: ?ClientState,
            started_attack: bool,
            animations: ?Animations,
        };
    };
    var game_state = GameState{ .main_menu = gui.LinearMenuState.init() };
    defer switch (game_state) {
        GameState.running => |*state| state.client.stopEngine(),
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
                if (state.client.pollResponse()) |response| {
                    switch (response) {
                        .stuff_happens => |happening| {
                            // Show animations for what's going on.
                            state.animations = try loadAnimations(happening.frames, now, state.client_state.?.terrain);
                            state.client_state = try loadStaticPerception(happening.static_perception);
                        },
                        .load_state => |static_perception| {
                            state.animations = null;
                            state.client_state = try loadStaticPerception(static_perception);
                        },
                        .game_over => {
                            @panic("did we die or something?");
                        },
                        .reject_request => {
                            // oh sorry.
                        },
                    }
                }
            },
        }

        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    core.debug.thread_lifecycle.print("sdl quit");
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
                                            try state.client.attack(makeCoord(-1, 0));
                                            state.started_attack = false;
                                        } else {
                                            try state.client.move(makeCoord(-1, 0));
                                        }
                                    },
                                    Button.right => {
                                        if (state.started_attack) {
                                            try state.client.attack(makeCoord(1, 0));
                                            state.started_attack = false;
                                        } else {
                                            try state.client.move(makeCoord(1, 0));
                                        }
                                    },
                                    Button.up => {
                                        if (state.started_attack) {
                                            try state.client.attack(makeCoord(0, -1));
                                            state.started_attack = false;
                                        } else {
                                            try state.client.move(makeCoord(0, -1));
                                        }
                                    },
                                    Button.down => {
                                        if (state.started_attack) {
                                            try state.client.attack(makeCoord(0, 1));
                                            state.started_attack = false;
                                        } else {
                                            try state.client.move(makeCoord(0, 1));
                                        }
                                    },
                                    Button.start_attack => {
                                        state.started_attack = true;
                                    },
                                    Button.backspace => {
                                        try state.client.rewind();
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

        sdl.assertZero(sdl.c.SDL_SetRenderTarget(renderer, screen_buffer));
        sdl.assertZero(sdl.c.SDL_RenderClear(renderer));

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
                menu_renderer.seekRelative(70, 30);
                if (menu_renderer.button("New Game (Thread)")) {
                    game_state = GameState{
                        .running = GameState.Running{
                            .client = undefined,
                            .client_state = null,
                            .started_attack = false,
                            .animations = null,
                        },
                    };
                    try game_state.running.client.startAsThread();
                }
                if (menu_renderer.button("Attach to Game (Process)")) {
                    game_state = GameState{
                        .running = GameState.Running{
                            .client = undefined,
                            .client_state = null,
                            .started_attack = false,
                            .animations = null,
                        },
                    };
                    try game_state.running.client.startAsChildProcess();
                }
                if (menu_renderer.button("Quit")) {
                    // quit
                    return;
                }

                menu_renderer.seekRelative(-70, 50);
                menu_renderer.text("Controls:");
                menu_renderer.text(" Arrow keys: Move");
                menu_renderer.text(" F: Start attack");
                menu_renderer.text("   Arrow keys: Attack in direction");
                menu_renderer.text(" Backspace: Undo");
                menu_renderer.text(" Enter: Begin");

                menu_renderer.seekRelative(140, 10);
                menu_renderer.imageAndText(textures.sprites.human, "This is you");
                menu_renderer.imageAndText(textures.sprites.hatch, "Unlock the stairs");
                menu_renderer.imageAndText(textures.sprites.stairs_down, "Go down the stairs");
            },
            GameState.running => |*state| blk: {
                if (state.client_state == null) break :blk;
                // render terrain
                const terrain = if (state.animations) |animations| animations.terrain else state.client_state.?.terrain;
                for (terrain.floor) |row, y| {
                    for (row) |cell, x| {
                        const coord = makeCoord(@intCast(i32, x), @intCast(i32, y));
                        const texture = switch (cell) {
                            Floor.unknown => textures.sprites.unknown_floor,
                            Floor.dirt => selectAesthetic(textures.sprites.dirt_floor[0..], aesthetic_seed, coord),
                            Floor.marble => selectAesthetic(textures.sprites.marble_floor[0..], aesthetic_seed, coord),
                            Floor.lava => selectAesthetic(textures.sprites.lava[0..], aesthetic_seed, coord),
                            Floor.hatch => textures.sprites.hatch,
                            Floor.stairs_down => textures.sprites.stairs_down,
                        };
                        textures.renderSprite(renderer, texture, coord.scaled(32));
                    }
                }
                for (terrain.walls) |row, y| {
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
                var animating = false;
                if (state.animations) |animations| {
                    const animation_time = @bitCast(u32, now -% animations.start_time);
                    const move_frame_time = 150;
                    const movement_phase = @divFloor(animation_time, move_frame_time);
                    if (movement_phase < animations.frames.len) {
                        animating = true;

                        const progress = @intCast(i32, animation_time - movement_phase * move_frame_time);
                        switch (animations.frames[movement_phase]) {
                            .movements => |movements| {
                                for (movements) |perceived_movement| {
                                    const a = perceived_movement; // shorter name
                                    // Scale velocity to halfway, because each is redundant with a neighboring frame.
                                    const display_position = core.geometry.bezier3(
                                        a.abs_position.scaled(32).minus(a.prior_velocity.scaled(32 / 2)),
                                        a.abs_position.scaled(32),
                                        a.abs_position.scaled(32).plus(a.next_velocity.scaled(32 / 2)),
                                        progress,
                                        move_frame_time,
                                    );
                                    textures.renderSprite(renderer, speciesToSprite(a.species), display_position);
                                }
                            },
                            .attacks => |attacks| {
                                for (attacks) |perceived_attack| {
                                    const a = perceived_attack; // shorter name
                                    textures.renderSprite(
                                        renderer,
                                        speciesToSprite(a.species),
                                        a.abs_position.scaled(32),
                                    );
                                    if (a.direction) |direction| {
                                        // render the attack
                                        const range = core.game_logic.getAttackRange(a.species);
                                        if (range == 1) {
                                            const dagger_sprite_normalizing_rotation = 1;
                                            textures.renderSpriteRotated(
                                                renderer,
                                                textures.sprites.dagger,
                                                a.abs_position.scaled(32).plus(direction.scaled(32 * 3 / 4)),
                                                u3(directionToRotation(direction)) +% dagger_sprite_normalizing_rotation,
                                            );
                                        } else {
                                            const arrow_sprite_normalizing_rotation = 4;
                                            textures.renderSpriteRotated(
                                                renderer,
                                                textures.sprites.arrow,
                                                core.geometry.bezier2(
                                                    a.abs_position.scaled(32).plus(direction.scaled(32 * 3 / 4)),
                                                    a.abs_position.plus(direction.scaled(range)).scaled(32),
                                                    progress,
                                                    move_frame_time,
                                                ),
                                                u3(directionToRotation(direction)) +% arrow_sprite_normalizing_rotation,
                                            );
                                        }
                                    }
                                }
                            },
                            .deaths => |deaths| {
                                for (deaths) |perceived_death| {
                                    const a = perceived_death; // shorter name
                                    const display_position = a.abs_position.scaled(32);
                                    textures.renderSprite(renderer, speciesToSprite(a.species), display_position);
                                    if (a.actually_dies) {
                                        textures.renderSprite(renderer, textures.sprites.death, display_position);
                                    }
                                }
                            },
                        }
                    } else {
                        // stale
                        state.animations = null;
                    }
                }
                if (!animating) {
                    // static
                    for (state.client_state.?.individuals.toSliceConst()) |individual| {
                        var display_position = individual.abs_position.scaled(32);
                        const sprite = speciesToSprite(individual.species);
                        textures.renderSprite(renderer, sprite, display_position);

                        const is_self = individual.species == .human;
                        if (is_self and state.started_attack) {
                            textures.renderSprite(renderer, textures.sprites.dagger, display_position);
                        }
                    }
                }
            },
        }

        {
            sdl.assertZero(sdl.c.SDL_SetRenderTarget(renderer, null));
            sdl.assertZero(sdl.c.SDL_GetRendererOutputSize(renderer, &output_rect.w, &output_rect.h));
            // preserve aspect ratio
            const source_aspect_ratio = comptime @intToFloat(f32, logical_window_size.w) / @intToFloat(f32, logical_window_size.h);
            const dest_aspect_ratio = @intToFloat(f32, output_rect.w) / @intToFloat(f32, output_rect.h);
            if (source_aspect_ratio > dest_aspect_ratio) {
                // use width
                const new_height = @floatToInt(c_int, @intToFloat(f32, output_rect.w) / source_aspect_ratio);
                output_rect.x = 0;
                output_rect.y = @divTrunc(output_rect.h - new_height, 2);
                output_rect.h = new_height;
            } else {
                // use height
                const new_width = @floatToInt(c_int, @intToFloat(f32, output_rect.h) * source_aspect_ratio);
                output_rect.x = @divTrunc(output_rect.w - new_width, 2);
                output_rect.y = 0;
                output_rect.w = new_width;
            }

            sdl.assertZero(sdl.c.SDL_RenderClear(renderer));
            sdl.assertZero(sdl.c.SDL_RenderCopy(renderer, screen_buffer, &logical_window_size, &output_rect));
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

fn speciesToSprite(species: Species) Rect {
    return switch (species) {
        .human => textures.sprites.human,
        .orc => textures.sprites.orc,
        .ogre => textures.sprites.ogre,
        .snake => textures.sprites.snake,
        .ant => textures.sprites.ant,
        .centaur => textures.sprites.centaur_archer,
    };
}

const StaticIndividual = core.protocol.StaticPerception.StaticIndividual;
const ClientState = struct {
    terrain: Terrain,
    individuals: ArrayList(StaticIndividual),
};

const Animations = struct {
    start_time: i32,
    frames: []PerceivedFrame,
    terrain: Terrain,
};
const AttackAnimation = struct {};
const DeathAnimation = struct {};

fn loadAnimations(frames: []PerceivedFrame, now: i32, previous_terrain: Terrain) !Animations {
    return Animations{
        .start_time = now,
        .frames = try core.protocol.deepClone(allocator, frames),
        .terrain = previous_terrain,
    };
}

fn loadStaticPerception(static_perception: core.protocol.StaticPerception) !ClientState {
    //core.debug.deep_print("static_perception: ", static_perception);
    return ClientState{
        .terrain = static_perception.terrain,
        .individuals = blk: {
            var individuals = ArrayList(StaticIndividual).init(allocator);
            try individuals.ensureCapacity(1 + static_perception.others.len);
            if (static_perception.self) |self| {
                individuals.append(self) catch unreachable;
            }
            for (static_perception.others) |other| {
                individuals.append(other) catch unreachable;
            }
            break :blk individuals;
        },
    };
}
