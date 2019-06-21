const std = @import("std");
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
const Response = core.protocol.Response;
const Event = core.protocol.Event;

const allocator = std.heap.c_allocator;

pub fn main() anyerror!void {
    core.debug.init();
    core.debug.nameThisThread("gui");
    core.debug.warn("init");
    defer core.debug.warn("shutdown");

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
                        .stuff_happens => |perceived_frames| {
                            try loadAnimations(&state.animations, response, now);
                            state.client_state.?.individuals = blk: {
                                const last_movements = perceived_frames[perceived_frames.len - 1].perceived_movements;
                                var arr = try allocator.alloc(StaticIndividual, last_movements.len);
                                for (arr) |*x, i| {
                                    x.* = StaticIndividual{
                                        .abs_position = last_movements[i].abs_position,
                                        .species = last_movements[i].species,
                                    };
                                }
                                break :blk arr;
                            };
                        },
                        .static_perception => |static_perception| {
                            // FIXME duplicated
                            state.animations = null;
                            state.client_state = ClientState{
                                .terrain = static_perception.terrain,
                                .individuals = try std.mem.concat(allocator, StaticIndividual, [_][]const StaticIndividual{
                                    [_]StaticIndividual{static_perception.self},
                                    static_perception.others,
                                }),
                            };
                        },
                        .undo => |static_perception| {
                            // FIXME duplicated
                            state.animations = null;
                            state.client_state = ClientState{
                                .terrain = static_perception.terrain,
                                .individuals = try std.mem.concat(allocator, StaticIndividual, [_][]const StaticIndividual{
                                    [_]StaticIndividual{static_perception.self},
                                    static_perception.others,
                                }),
                            };
                        },
                    }
                }
            },
        }

        var event: sdl.c.SDL_Event = undefined;
        while (sdl.SDL_PollEvent(&event) != 0) {
            switch (event.@"type") {
                sdl.c.SDL_QUIT => {
                    core.debug.warn("sdl quit");
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
            },
            GameState.running => |*state| blk: {
                if (state.client_state == null) break :blk;
                // render terrain
                for (state.client_state.?.terrain.floor) |row, y| {
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
                for (state.client_state.?.terrain.walls) |row, y| {
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
                    if (movement_phase < animations.move_animations.len) {
                        animating = true;

                        const progress = @intCast(i32, animation_time - movement_phase * move_frame_time);
                        const animation_group = animations.move_animations[movement_phase];
                        for (animation_group) |a| {
                            // Scale velocity to halfway, because each is redundant with a neighboring frame.
                            const display_position = core.geometry.bezier3(
                                a.abs_position.scaled(32).minus(a.prior_velocity.scaled(32 / 2)),
                                a.abs_position.scaled(32),
                                a.abs_position.scaled(32).plus(a.next_velocity.scaled(32 / 2)),
                                progress,
                                move_frame_time,
                            );
                            const sprite = speciesToSprite(a.species);
                            textures.renderSprite(renderer, sprite, display_position);
                        }
                    } else {
                        // stale
                        state.animations = null;
                    }
                }
                if (!animating) {
                    // static
                    for (state.client_state.?.individuals) |individual| {
                        var display_position = individual.abs_position.scaled(32);
                        const sprite = speciesToSprite(individual.species);
                        textures.renderSprite(renderer, sprite, display_position);

                        const is_self = true; // TODO
                        if (is_self and state.started_attack) {
                            textures.renderSprite(renderer, textures.sprites.dagger, display_position);
                        }
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

fn speciesToSprite(species: Species) Rect {
    return switch (species) {
        .human => textures.sprites.human,
        .orc => textures.sprites.orc,
        .ogre => textures.sprites.ogre,
        .snake => textures.sprites.snake,
        .ant => textures.sprites.ant,
    };
}

const StaticIndividual = core.protocol.StaticPerception.StaticIndividual;
const ClientState = struct {
    terrain: core.protocol.Terrain,
    individuals: []StaticIndividual,
};

const Animations = struct {
    start_time: i32,
    move_animations: [][]MoveAnimation,
    attack_animations: [0]AttackAnimation, // TODO
    death_animations: [0]DeathAnimation, // TODO
};
const MoveAnimation = struct {
    prior_velocity: Coord,
    abs_position: Coord,
    species: Species,
    next_velocity: Coord,
};
const AttackAnimation = struct {};
const DeathAnimation = struct {};

fn loadAnimations(animations: *?Animations, response: Response, now: i32) !void {
    switch (response) {
        Response.static_perception => {},
        Response.stuff_happens => |frames| {
            animations.* = Animations{
                .start_time = now,
                .move_animations = try allocator.alloc([]MoveAnimation, frames.len),
                .attack_animations = [_]AttackAnimation{},
                .death_animations = [_]DeathAnimation{},
            };
            animations.*.?.start_time = now;
            animations.*.?.move_animations = try allocator.alloc([]MoveAnimation, frames.len);
            for (animations.*.?.move_animations) |*animation_group, i| {
                const frame = frames[i];
                animation_group.* = try allocator.alloc(MoveAnimation, frame.perceived_movements.len);
                for (animation_group.*) |*animation, j| {
                    const movement = frame.perceived_movements[j];
                    animation.* = MoveAnimation{
                        .prior_velocity = movement.prior_velocity,
                        .abs_position = movement.abs_position,
                        .species = movement.species,
                        .next_velocity = movement.next_velocity,
                    };
                }
            }
        },
        Response.undo => |events| {
            // cancel animations
            animations.* = null;
        },
    }
}
