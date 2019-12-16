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
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const allocator = std.heap.c_allocator;
const getHeadPosition = core.game_logic.getHeadPosition;

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
    if (!(sdl.c.SDL_SetHintWithPriority(sdl.c.SDL_HINT_NO_SIGNAL_HANDLERS, "1", sdl.c.SDL_HintPriority.SDL_HINT_OVERRIDE) != sdl.c.SDL_bool.SDL_FALSE)) {
        std.debug.panic("failed to disable sdl signal handlers\n", .{});
    }
    if (sdl.c.SDL_Init(sdl.c.SDL_INIT_VIDEO) != 0) {
        std.debug.panic("SDL_Init failed: {c}\n", .{sdl.c.SDL_GetError()});
    }
    defer sdl.c.SDL_Quit();

    const screen = sdl.c.SDL_CreateWindow(
        "Legend of Swarkland",
        sdl.SDL_WINDOWPOS_UNDEFINED,
        sdl.SDL_WINDOWPOS_UNDEFINED,
        logical_window_size.w,
        logical_window_size.h,
        sdl.c.SDL_WINDOW_RESIZABLE,
    ) orelse {
        std.debug.panic("SDL_CreateWindow failed: {c}\n", .{sdl.c.SDL_GetError()});
    };
    defer sdl.c.SDL_DestroyWindow(screen);

    const renderer: *sdl.Renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {c}\n", .{sdl.c.SDL_GetError()});
    };
    defer sdl.c.SDL_DestroyRenderer(renderer);

    {
        var renderer_info: sdl.c.SDL_RendererInfo = undefined;
        sdl.assertZero(sdl.c.SDL_GetRendererInfo(renderer, &renderer_info));
        if (renderer_info.flags & sdl.c.SDL_RENDERER_TARGETTEXTURE == 0) {
            std.debug.panic("rendering to a temporary texture is not supported", .{});
        }
    }

    const screen_buffer: *sdl.Texture = sdl.c.SDL_CreateTexture(
        renderer,
        sdl.c.SDL_PIXELFORMAT_ABGR8888,
        sdl.c.SDL_TEXTUREACCESS_TARGET,
        logical_window_size.w,
        logical_window_size.h,
    ) orelse {
        std.debug.panic("SDL_CreateTexture failed: {c}\n", .{sdl.c.SDL_GetError()});
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
            client_state: ?PerceivedFrame,
            started_attack: bool,
            animations: ?Animations,

            /// only tracked to display aesthetics consistently through movement.
            total_journey_offset: Coord,
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
                if (state.client.queues.takeResponse()) |response| {
                    switch (response) {
                        .stuff_happens => |happening| {
                            // Show animations for what's going on.
                            state.animations = try loadAnimations(happening.frames, now);
                            state.client_state = happening.frames[happening.frames.len - 1];
                            state.total_journey_offset = state.total_journey_offset.plus(state.animations.?.frame_index_to_aesthetic_offset[happening.frames.len - 1]);
                        },
                        .load_state => |frame| {
                            state.animations = null;
                            state.client_state = frame;
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
                                        if (state.started_attack) {
                                            state.started_attack = false;
                                        } else {
                                            try state.client.rewind();
                                        }
                                    },
                                    Button.escape => {
                                        state.started_attack = false;
                                    },
                                    Button.restart => {
                                        state.client.stopEngine();
                                        game_state = GameState{ .main_menu = gui.LinearMenuState.init() };
                                    },
                                    Button.beat_level => {
                                        try state.client.beatLevelMacro();
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
                var menu_renderer = gui.Gui.init(renderer, main_menu_state, textures.sprites.human);

                menu_renderer.seek(10, 10);
                menu_renderer.scale(2);
                menu_renderer.bold(true);
                menu_renderer.marginBottom(5);
                menu_renderer.text("Legend of Swarkland");
                menu_renderer.scale(1);
                menu_renderer.bold(false);
                menu_renderer.seekRelative(70, 30);
                if (menu_renderer.button(" ")) {
                    game_state = GameState{
                        .running = GameState.Running{
                            .client = undefined,
                            .client_state = null,
                            .started_attack = false,
                            .animations = null,
                            .total_journey_offset = makeCoord(0, 0),
                        },
                    };
                    try game_state.running.client.startAsThread();
                }

                menu_renderer.seekRelative(-70, 50);
                menu_renderer.text("Controls:");
                menu_renderer.text(" Arrow keys: Move");
                menu_renderer.text(" F: Start attack");
                menu_renderer.text("   Arrow keys: Attack in direction");
                menu_renderer.text(" Backspace: Undo");
                menu_renderer.text(" Ctrl+R: Quit to this menu");
                menu_renderer.text(" Enter: Start Game");
                menu_renderer.text(" ");
                menu_renderer.text(" ");
                menu_renderer.text("version: " ++ textures.version_string);
            },
            GameState.running => |*state| blk: {
                if (state.client_state == null) break :blk;

                const move_frame_time = 300;

                // at one point in what frame should we render?
                var frame = state.client_state.?;
                var progress: i32 = 0;
                var show_poised_attack = true;
                var animated_aesthetic_offset = makeCoord(0, 0);
                if (state.animations) |animations| {
                    const animation_time = @bitCast(u32, now -% animations.start_time);
                    const movement_phase = @divFloor(animation_time, move_frame_time);

                    if (movement_phase < animations.frames.len) {
                        // animating
                        frame = animations.frames[movement_phase];
                        progress = @intCast(i32, animation_time - movement_phase * move_frame_time);
                        show_poised_attack = false;
                        animated_aesthetic_offset = animations.frame_index_to_aesthetic_offset[movement_phase].minus(animations.frame_index_to_aesthetic_offset[animations.frame_index_to_aesthetic_offset.len - 1]);
                    } else {
                        // stale
                        state.animations = null;
                    }
                }

                const center_screen = makeCoord(7, 7).scaled(32).plus(makeCoord(32 / 2, 32 / 2));
                const camera_offset = center_screen.minus(getRelDisplayPosition(progress, move_frame_time, frame.self));

                // render terrain
                {
                    const terrain_offset = frame.terrain.rel_position.scaled(32).plus(camera_offset);
                    const terrain = frame.terrain.matrix;
                    var cursor = makeCoord(undefined, 0);
                    while (cursor.y <= @as(i32, terrain.height)) : (cursor.y += 1) {
                        cursor.x = 0;
                        while (cursor.x <= @as(i32, terrain.width)) : (cursor.x += 1) {
                            if (terrain.getCoord(cursor)) |cell| {
                                const display_position = cursor.scaled(32).plus(terrain_offset);
                                const aesthetic_coord = cursor.plus(state.total_journey_offset).plus(animated_aesthetic_offset);
                                const floor_texture = switch (cell.floor) {
                                    Floor.unknown => textures.sprites.unknown_floor,
                                    Floor.dirt => selectAesthetic(textures.sprites.dirt_floor[0..], aesthetic_seed, aesthetic_coord),
                                    Floor.marble => selectAesthetic(textures.sprites.marble_floor[0..], aesthetic_seed, aesthetic_coord),
                                    Floor.lava => selectAesthetic(textures.sprites.lava[0..], aesthetic_seed, aesthetic_coord),
                                    Floor.hatch => textures.sprites.hatch,
                                    Floor.stairs_down => textures.sprites.stairs_down,
                                };
                                textures.renderSprite(renderer, floor_texture, display_position);
                                const wall_texture = switch (cell.wall) {
                                    Wall.unknown => textures.sprites.unknown_wall,
                                    Wall.air => continue,
                                    Wall.dirt => selectAesthetic(textures.sprites.brown_brick[0..], aesthetic_seed, aesthetic_coord),
                                    Wall.stone => selectAesthetic(textures.sprites.gray_brick[0..], aesthetic_seed, aesthetic_coord),
                                };
                                textures.renderSprite(renderer, wall_texture, display_position);
                            }
                        }
                    }
                }

                // render the things
                for (frame.others) |other| {
                    _ = renderThing(renderer, progress, move_frame_time, camera_offset, other);
                }
                const display_position = renderThing(renderer, progress, move_frame_time, camera_offset, frame.self);
                if (show_poised_attack and state.started_attack) {
                    textures.renderSprite(renderer, textures.sprites.dagger, display_position);
                }

                // if we're showing you dead, show a tutorial.
                if (frame.self.activity == .death) {
                    // gentle up/down bob
                    var animated_y: i32 = @divFloor(@mod(now, 2000), 100);
                    if (animated_y > 10) animated_y = 20 - animated_y;
                    const coord = makeCoord(512 / 2 - 384 / 2, 512 - 32 + animated_y);
                    const size = textures.renderTextScaled(renderer, "you died. use Backspace to undo.", coord, true, 1);
                } else if (frame.you_win) {
                    // gentle up/down bob
                    var animated_y: i32 = @divFloor(@mod(now, 2000), 100);
                    if (animated_y > 10) animated_y = 20 - animated_y;
                    const coord = makeCoord(512 / 2 - 384 / 2, 512 - 32 + animated_y);
                    const size = textures.renderTextScaled(renderer, "you are win. use Ctrl+R to quit.", coord, true, 1);
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

fn getRelDisplayPosition(progress: i32, progress_denominator: i32, thing: PerceivedThing) Coord {
    const rel_position = getHeadPosition(thing.rel_position);
    switch (thing.activity) {
        .movement => |move_delta| {
            if (progress < @divFloor(progress_denominator, 2)) {
                // in the first half, speed up toward the halfway point.
                return core.geometry.bezier3(
                    rel_position.scaled(32),
                    rel_position.scaled(32),
                    rel_position.scaled(32).plus(move_delta.scaled(32 / 2)),
                    progress,
                    @divFloor(progress_denominator, 2),
                );
            } else {
                // in the second half, slow down from the halfway point.
                return core.geometry.bezier3(
                    rel_position.scaled(32).plus(move_delta.scaled(32 / 2)),
                    rel_position.scaled(32).plus(move_delta.scaled(32)),
                    rel_position.scaled(32).plus(move_delta.scaled(32)),
                    progress - @divFloor(progress_denominator, 2),
                    @divFloor(progress_denominator, 2),
                );
            }
        },
        .failed_movement => |move_delta| {
            if (progress < @divFloor(progress_denominator, 2)) {
                // in the first half, speed up toward the halfway point of the would-be movement.
                return core.geometry.bezier3(
                    rel_position.scaled(32),
                    rel_position.scaled(32),
                    rel_position.scaled(32).plus(move_delta.scaled(32 / 2)),
                    progress,
                    @divFloor(progress_denominator, 2),
                );
            } else {
                // in the second half, abruptly reverse course and do the opposite of the above.
                return core.geometry.bezier3(
                    rel_position.scaled(32).plus(move_delta.scaled(32 / 2)),
                    rel_position.scaled(32),
                    rel_position.scaled(32),
                    progress - @divFloor(progress_denominator, 2),
                    @divFloor(progress_denominator, 2),
                );
            }
        },
        else => return rel_position.scaled(32),
    }
}

fn renderThing(renderer: *sdl.Renderer, progress: i32, progress_denominator: i32, camera_offset: Coord, thing: PerceivedThing) Coord {
    const rel_display_position = getRelDisplayPosition(progress, progress_denominator, thing);
    const display_position = rel_display_position.plus(camera_offset);
    switch (thing.rel_position) {
        .small => {
            textures.renderSprite(renderer, speciesToSprite(thing.species), display_position);
        },
        .large => |coords| {
            const oriented_delta = coords[1].minus(coords[0]);
            const tail_display_position = display_position.plus(oriented_delta.scaled(32));
            const rhino_sprite_normalizing_rotation = 0;
            const rotation = directionToRotation(oriented_delta) +% rhino_sprite_normalizing_rotation;
            textures.renderSpriteRotated(renderer, speciesToSprite(thing.species), display_position, rotation);
            textures.renderSpriteRotated(renderer, speciesToTailSprite(thing.species), tail_display_position, rotation);
        },
    }

    switch (thing.activity) {
        .none => {},
        .movement => {},
        .failed_movement => {},

        .attack => |data| {
            const max_range = core.game_logic.getAttackRange(thing.species);
            if (max_range == 1) {
                const dagger_sprite_normalizing_rotation = 1;
                textures.renderSpriteRotated(
                    renderer,
                    textures.sprites.dagger,
                    display_position.plus(data.direction.scaled(32 * 3 / 4)),
                    directionToRotation(data.direction) +% dagger_sprite_normalizing_rotation,
                );
            } else {
                // The animated bullet speed is determined by the max range,
                // but interrupt the progress if the arrow hits something.
                var clamped_progress = progress * max_range;
                if (clamped_progress > data.distance * progress_denominator) {
                    clamped_progress = data.distance * progress_denominator;
                }
                const arrow_sprite_normalizing_rotation = 4;
                textures.renderSpriteRotated(
                    renderer,
                    textures.sprites.arrow,
                    core.geometry.bezier2(
                        display_position,
                        display_position.plus(data.direction.scaled(32 * max_range)),
                        clamped_progress,
                        progress_denominator * max_range,
                    ),
                    directionToRotation(data.direction) +% arrow_sprite_normalizing_rotation,
                );
            }
        },

        .death => {
            textures.renderSprite(renderer, textures.sprites.death, display_position);
        },
    }

    return display_position;
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
        .centaur => textures.sprites.centaur_archer,
        .turtle => textures.sprites.turtle,
        .rhino => textures.sprites.rhino[0],
    };
}

fn speciesToTailSprite(species: Species) Rect {
    return switch (species) {
        .rhino => textures.sprites.rhino[1],
        .human, .orc, .centaur, .turtle => unreachable,
    };
}

const Animations = struct {
    start_time: i32,
    frames: []PerceivedFrame,
    frame_index_to_aesthetic_offset: []Coord,
};
const AttackAnimation = struct {};
const DeathAnimation = struct {};

fn loadAnimations(frames: []PerceivedFrame, now: i32) !Animations {
    var frame_index_to_aesthetic_offset = try allocator.alloc(Coord, frames.len);
    var current_offset = makeCoord(0, 0);
    for (frames) |frame, i| {
        frame_index_to_aesthetic_offset[i] = current_offset;
        switch (frame.self.activity) {
            .movement => |move_delta| {
                current_offset = current_offset.plus(move_delta);
            },
            else => {},
        }
    }

    return Animations{
        .start_time = now,
        .frames = try core.protocol.deepClone(allocator, frames),
        .frame_index_to_aesthetic_offset = frame_index_to_aesthetic_offset,
    };
}
