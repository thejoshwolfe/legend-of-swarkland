const std = @import("std");
const ArrayList = std.ArrayList;
const ArrayListUnmanaged = std.ArrayListUnmanaged;

const sdl = @import("sdl.zig");
const textures = @import("textures.zig");
const gui = @import("gui.zig");
const InputEngine = @import("input_engine.zig").InputEngine;
const Button = @import("input_engine.zig").Button;
const SaveFile = @import("SaveFile.zig");

const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const directionToRotation = core.geometry.directionToRotation;
const GameEngineClient = @import("../client/game_engine_client.zig").GameEngineClient;
const FancyClient = @import("../client/FancyClient.zig");
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const TerrainSpace = core.protocol.TerrainSpace;
const NewGameSettings = core.protocol.NewGameSettings;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const Action = core.protocol.Action;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const allocator = std.heap.c_allocator;
const getHeadPosition = core.game_logic.getHeadPosition;
const getPhysicsLayer = core.game_logic.getPhysicsLayer;
const canAttack = core.game_logic.canAttack;
const canCharge = core.game_logic.canCharge;
const canKick = core.game_logic.canKick;
const terrainAtInner = core.game_logic.terrainAtInner;

const the_levels = @import("../server/map_gen.zig").the_levels;

const logical_window_size = sdl.makeRect(Rect{ .x = 0, .y = 0, .width = 712, .height = 512 });

/// changes when the window resizes
/// FIXME: should initialize to logical_window_size, but workaround https://github.com/ziglang/zig/issues/2855
var output_rect = sdl.makeRect(Rect{
    .x = logical_window_size.x,
    .y = logical_window_size.y,
    .width = logical_window_size.w,
    .height = logical_window_size.h,
});

pub fn main() anyerror!void {
    core.debug.init();
    core.debug.nameThisThread("gui");
    defer core.debug.unnameThisThread();
    core.debug.thread_lifecycle.print("init", .{});
    defer core.debug.thread_lifecycle.print("shutdown", .{});

    // SDL handling SIGINT blocks propagation to child threads.
    if (!(sdl.c.SDL_SetHintWithPriority(sdl.c.SDL_HINT_NO_SIGNAL_HANDLERS, "1", sdl.c.SDL_HINT_OVERRIDE) != sdl.c.SDL_FALSE)) {
        std.debug.panic("failed to disable sdl signal handlers\n", .{});
    }
    if (sdl.c.SDL_Init(sdl.c.SDL_INIT_VIDEO) != 0) {
        std.debug.panic("SDL_Init failed: {s}\n", .{sdl.c.SDL_GetError()});
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
        std.debug.panic("SDL_CreateWindow failed: {s}\n", .{sdl.c.SDL_GetError()});
    };
    defer sdl.c.SDL_DestroyWindow(screen);

    const renderer: *sdl.Renderer = sdl.c.SDL_CreateRenderer(screen, -1, 0) orelse {
        std.debug.panic("SDL_CreateRenderer failed: {s}\n", .{sdl.c.SDL_GetError()});
    };
    defer sdl.c.SDL_DestroyRenderer(renderer);

    {
        var renderer_info: sdl.c.SDL_RendererInfo = undefined;
        sdl.assertZero(sdl.c.SDL_GetRendererInfo(renderer, &renderer_info));
        if (renderer_info.flags & @bitCast(u32, sdl.c.SDL_RENDERER_TARGETTEXTURE) == 0) {
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
        std.debug.panic("SDL_CreateTexture failed: {s}\n", .{sdl.c.SDL_GetError()});
    };
    defer sdl.c.SDL_DestroyTexture(screen_buffer);

    textures.init(renderer);
    defer textures.deinit();

    try doMainLoop(renderer, screen_buffer);
}

const InputPrompt = enum {
    none, // TODO: https://github.com/ziglang/zig/issues/1332 and use null instead of this.
    attack,
    kick,
};
const GameState = union(enum) {
    main_menu: gui.LinearMenuState,
    level_select: gui.LinearMenuState,

    running: RunningState,
};
const RunningState = struct {
    client: FancyClient,
    client_state: ?PerceivedFrame = null,
    input_prompt: InputPrompt = .none,
    animations: ?Animations = null,

    // tutorial state should *not* reset through undo.
    // these values cap out at small values, because we only use them for showing/hiding tutorials.
    kicks_performed: u2 = 0,
    observed_kangaroo_death: bool = false,
    charge_performed: bool = false,
    moves_performed: u2 = 0,
    attacks_performed: u2 = 0,
    performed_wait: bool = false,
    consecutive_undos: u3 = 0,
    performed_restart: bool = false,

    new_game_settings: union(enum) {
        regular,
        puzzle_level: usize,
    },
};

fn doMainLoop(renderer: *sdl.Renderer, screen_buffer: *sdl.Texture) !void {
    const aesthetic_seed = 0xbee894fc;
    var input_engine = InputEngine.init();
    var inputs_considered_harmful = true;

    var save_file = SaveFile.load();
    var save_file_text_lines = ArrayList([]const u8).init(allocator);
    {
        const save_file_abs_path = try std.fs.path.join(allocator, &[_][]const u8{
            try std.fs.cwd().realpathAlloc(allocator, "."),
            SaveFile.filename,
        });
        const text_wrap_width = @divTrunc(logical_window_size.w - 64, 12);
        var path_remaining = save_file_abs_path;
        while (path_remaining.len > 0) {
            if (path_remaining.len <= text_wrap_width) {
                try save_file_text_lines.append(try std.mem.concat(allocator, u8, &[_][]const u8{ " ", path_remaining }));
                break;
            }
            try save_file_text_lines.append(try std.mem.concat(allocator, u8, &[_][]const u8{ " ", path_remaining[0..text_wrap_width] }));
            path_remaining = path_remaining[text_wrap_width..];
        }
    }

    var game_state = mainMenuState(&save_file);
    defer switch (game_state) {
        .running => |*state| state.client.client.stopEngine(),
        else => {},
    };

    main_loop: while (true) {
        // TODO: use better source of time (that doesn't crash after running for a month)
        const now = @intCast(i32, sdl.c.SDL_GetTicks());
        switch (game_state) {
            .main_menu => |*menu_state| {
                menu_state.beginFrame();
            },
            .level_select => |*menu_state| {
                menu_state.beginFrame();
            },
            .running => |*state| {
                while (try state.client.takeResponse()) |response| {
                    switch (response) {
                        .stuff_happens => |happening| {
                            if (state.new_game_settings == .puzzle_level and happening.frames[0].completed_levels < state.new_game_settings.puzzle_level) {
                                // Don't show the skip-to-level animations
                                state.animations = null;
                            } else {
                                // Show animations for what's going on.
                                try loadAnimations(&state.animations, happening.frames, now);
                            }
                            state.client_state = happening.frames[happening.frames.len - 1];

                            // Update tutorial data.
                            state.consecutive_undos = 0;
                            for (happening.frames) |frame| {
                                for (frame.others) |other| {
                                    if (other.activity == .death and other.species == .kangaroo) {
                                        state.observed_kangaroo_death = true;
                                    }
                                }
                                switch (frame.self.activity) {
                                    .kick => {
                                        state.kicks_performed +|= 1;
                                    },
                                    .movement => |delta| {
                                        if (core.geometry.isScaledCardinalDirection(delta, 2)) {
                                            state.charge_performed = true;
                                        } else {
                                            state.moves_performed +|= 1;
                                        }
                                    },
                                    .attack => {
                                        state.attacks_performed +|= 1;
                                    },
                                    else => {},
                                }
                            }

                            // Save progress
                            const new_completed_levels = state.client_state.?.completed_levels;
                            if (new_completed_levels > save_file.completed_levels) {
                                save_file.completed_levels = new_completed_levels;
                                save_file.save();
                            }
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
                    core.debug.thread_lifecycle.print("sdl quit", .{});
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
                            // when we first get focus, SDL gives a friendly digest of all the buttons that are already held down.
                            // these are not inputs for us.
                            continue;
                        }
                        switch (game_state) {
                            .main_menu => |*menu_state| {
                                switch (button) {
                                    .up => {
                                        menu_state.moveUp(1);
                                    },
                                    .down => {
                                        menu_state.moveDown(1);
                                    },
                                    .page_up => {
                                        menu_state.moveUp(5);
                                    },
                                    .page_down => {
                                        menu_state.moveDown(5);
                                    },
                                    .home => {
                                        menu_state.cursor_position = 0;
                                    },
                                    .end => {
                                        menu_state.cursor_position = menu_state.entry_count -| 1;
                                    },
                                    .enter => {
                                        menu_state.enter();
                                    },
                                    else => {},
                                }
                            },
                            .level_select => |*menu_state| {
                                switch (button) {
                                    .up => {
                                        menu_state.moveUp(1);
                                    },
                                    .down => {
                                        menu_state.moveDown(1);
                                    },
                                    .page_up => {
                                        menu_state.moveUp(5);
                                    },
                                    .page_down => {
                                        menu_state.moveDown(5);
                                    },
                                    .home => {
                                        menu_state.cursor_position = 0;
                                    },
                                    .end => {
                                        menu_state.cursor_position = menu_state.entry_count -| 1;
                                    },
                                    .enter => {
                                        menu_state.enter();
                                    },
                                    .escape => {
                                        game_state = mainMenuState(&save_file);
                                        continue :main_loop;
                                    },
                                    else => {},
                                }
                            },
                            .running => |*state| {
                                switch (button) {
                                    .left => try doDirectionInput(state, makeCoord(-1, 0)),
                                    .right => try doDirectionInput(state, makeCoord(1, 0)),
                                    .up => try doDirectionInput(state, makeCoord(0, -1)),
                                    .down => try doDirectionInput(state, makeCoord(0, 1)),

                                    .shift_left => try doAutoDirectionInput(state, makeCoord(-1, 0)),
                                    .shift_right => try doAutoDirectionInput(state, makeCoord(1, 0)),
                                    .shift_up => try doAutoDirectionInput(state, makeCoord(0, -1)),
                                    .shift_down => try doAutoDirectionInput(state, makeCoord(0, 1)),

                                    .start_attack => {
                                        if (canAttack(state.client_state.?.self.species)) {
                                            state.input_prompt = .attack;
                                        }
                                    },
                                    .start_kick => {
                                        if (canKick(state.client_state.?.self.species)) {
                                            state.input_prompt = .kick;
                                        }
                                    },
                                    .charge => {
                                        if (canCharge(state.client_state.?.self.species)) {
                                            const position_coords = state.client_state.?.self.position.large;
                                            const delta = position_coords[0].minus(position_coords[1]);
                                            try state.client.act(Action{ .fast_move = delta.scaled(2) });
                                        }
                                    },
                                    .backspace => {
                                        if (state.input_prompt != .none) {
                                            state.input_prompt = .none;
                                        } else {
                                            try state.client.rewind();
                                            state.consecutive_undos +|= 1;
                                        }
                                    },
                                    .spacebar => {
                                        try state.client.act(.wait);
                                        state.performed_wait = true;
                                    },
                                    .escape => {
                                        state.input_prompt = .none;
                                        state.animations = null;
                                        state.client.cancelAutoAction();
                                    },
                                    .restart => {
                                        try state.client.restartLevel();
                                        state.input_prompt = .none;
                                        state.performed_restart = true;
                                    },
                                    .quit => {
                                        state.client.client.stopEngine();
                                        game_state = mainMenuState(&save_file);
                                        continue :main_loop;
                                    },
                                    .beat_level => {
                                        try state.client.beatLevelMacro(1);
                                    },
                                    .beat_level_5 => {
                                        try state.client.beatLevelMacro(5);
                                    },
                                    .unbeat_level => {
                                        try state.client.unbeatLevelMacro(1);
                                    },
                                    .unbeat_level_5 => {
                                        try state.client.unbeatLevelMacro(5);
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
            .main_menu => |*menu_state| {
                var menu_renderer = gui.Gui.init(renderer, menu_state, textures.sprites.dagger);

                menu_renderer.seek(10, 10);
                menu_renderer.scale(2);
                menu_renderer.bold(true);
                menu_renderer.marginBottom(5);
                menu_renderer.text("Legend of Swarkland");
                menu_renderer.scale(1);
                menu_renderer.bold(false);
                menu_renderer.seekRelative(70, 30);
                if (menu_renderer.button("New Game")) {
                    try startGame(&game_state);
                    continue :main_loop;
                }
                if (menu_renderer.button("Puzzle Levels")) {
                    const button_count = if (save_file.completed_levels == the_levels.len - 1)
                        save_file.completed_levels
                    else
                        save_file.completed_levels + 1;
                    game_state = GameState{ .level_select = .{
                        .entry_count = button_count,
                        .cursor_position = button_count - 1,
                    } };
                    continue :main_loop;
                }

                menu_renderer.seekRelative(-70, 50);
                menu_renderer.text("Menu Controls:");
                menu_renderer.text(" Arrow keys + Enter");
                menu_renderer.text(" ");
                menu_renderer.text("Display Size: 712x512");
                menu_renderer.text(" (Just resize the window)");
                menu_renderer.text(" ");
                menu_renderer.text("Save file path:");
                for (save_file_text_lines.items) |line| {
                    menu_renderer.text(line);
                }
                menu_renderer.text(" ");
                menu_renderer.text(" ");
                menu_renderer.text("version: " ++ textures.version_string);
            },

            .level_select => |*menu_state| {
                var menu_renderer = gui.Gui.init(renderer, menu_state, textures.sprites.dagger);
                menu_renderer.seek(32, 32);
                menu_renderer.marginBottom(3);

                for (the_levels[0 .. the_levels.len - 1]) |level, i| {
                    if (i < save_file.completed_levels) {
                        // Past levels
                        if (menu_renderer.button(level.name)) {
                            try startPuzzleGame(&game_state, i);
                            continue :main_loop;
                        }
                    } else if (i == save_file.completed_levels) {
                        // Current level
                        menu_renderer.bold(true);
                        if (menu_renderer.button("??? (New)")) {
                            try startPuzzleGame(&game_state, i);
                            continue :main_loop;
                        }
                        menu_renderer.bold(false);
                    } else {
                        // Future level
                        menu_renderer.text("---");
                    }
                }

                menu_renderer.seek(448, 32);
                menu_renderer.text("Menu Controls:");
                menu_renderer.text(" Arrow keys");
                menu_renderer.text(" Ctrl+Arrows");
                menu_renderer.text(" PageUp/PageDown");
                menu_renderer.text(" Home/End");
                menu_renderer.text(" Enter/Escape");
            },

            .running => |*state| blk: {
                if (state.client_state == null) break :blk;

                // at one point in what frame should we render?
                var frame = state.client_state.?;
                var progress: i32 = 0;
                var move_frame_time: i32 = 1;
                var display_any_input_prompt = true;
                if (state.animations) |animations| {
                    if (animations.frameAtTime(now)) |data| {
                        // animating
                        frame = animations.frames.items[data.frame_index];
                        progress = data.progress;
                        move_frame_time = data.time_per_frame;
                        display_any_input_prompt = false;
                    } else {
                        // stale
                        state.animations = null;
                    }
                }

                // top left corner of the screen in world coordinates * 32
                const screen_display_position = b: {
                    const self_display_rect = getDisplayRect(progress, move_frame_time, frame.self);
                    const self_display_center = self_display_rect.position().plus(self_display_rect.size().scaledDivTrunc(2));
                    const screen_half_size = makeCoord(15, 15).scaled(32 / 2);
                    break :b self_display_center.minus(screen_half_size);
                };

                // render terrain
                {
                    const my_head_coord = getHeadPosition(frame.self.position);
                    const render_bounding_box = Rect{
                        .x = my_head_coord.x - 8,
                        .y = my_head_coord.y - 8,
                        .width = 18,
                        .height = 18,
                    };
                    var cursor = render_bounding_box.position();
                    while (cursor.y < render_bounding_box.bottom()) : (cursor.y += 1) {
                        cursor.x = render_bounding_box.x;
                        while (cursor.x < render_bounding_box.right()) : (cursor.x += 1) {
                            const cell = state.client.remembered_terrain.getCoord(cursor);
                            const render_position = cursor.scaled(32).minus(screen_display_position);

                            render_floor: {
                                const floor_texture = switch (cell.floor) {
                                    .unknown => break :render_floor,
                                    .dirt => selectAesthetic(textures.sprites.dirt_floor[0..], aesthetic_seed, cursor),
                                    .grass => selectAesthetic(textures.sprites.grass[0..], aesthetic_seed, cursor),
                                    .marble => selectAesthetic(textures.sprites.marble_floor[0..], aesthetic_seed, cursor),
                                    .lava => selectAesthetic(textures.sprites.lava[0..], aesthetic_seed, cursor),
                                    .hatch => textures.sprites.hatch,
                                    .stairs_down => textures.sprites.stairs_down,
                                    .unknown_floor => textures.sprites.unknown_floor,
                                };
                                textures.renderSprite(renderer, floor_texture, render_position);
                            }
                            render_wall: {
                                const wall_texture = switch (cell.wall) {
                                    .unknown => break :render_wall,
                                    .air => break :render_wall,
                                    .dirt => selectAesthetic(textures.sprites.brown_brick[0..], aesthetic_seed, cursor),
                                    .tree_northwest => textures.sprites.tree[0],
                                    .tree_northeast => textures.sprites.tree[1],
                                    .tree_southwest => textures.sprites.tree[2],
                                    .tree_southeast => textures.sprites.tree[3],
                                    .bush => selectAesthetic(textures.sprites.bush[0..], aesthetic_seed, cursor),
                                    .stone => selectAesthetic(textures.sprites.gray_brick[0..], aesthetic_seed, cursor),
                                    .polymorph_trap_centaur, .polymorph_trap_kangaroo, .polymorph_trap_turtle, .polymorph_trap_blob, .polymorph_trap_human, .unknown_polymorph_trap => textures.sprites.polymorph_trap,
                                    .polymorph_trap_rhino_west, .polymorph_trap_blob_west, .unknown_polymorph_trap_west => textures.sprites.polymorph_trap_wide[0],
                                    .polymorph_trap_rhino_east, .polymorph_trap_blob_east, .unknown_polymorph_trap_east => textures.sprites.polymorph_trap_wide[1],
                                    .unknown_wall => textures.sprites.unknown_wall,
                                };
                                textures.renderSprite(renderer, wall_texture, render_position);
                            }

                            if (!state.client.terrain_is_currently_in_view.getCoord(cursor)) {
                                // Gray out the space
                                textures.renderSprite(renderer, textures.sprites.black_alpha_square, render_position);
                            }
                        }
                    }
                }

                // render the things
                {
                    var layers = [4]ArrayList(PerceivedThing){
                        ArrayList(PerceivedThing).init(allocator),
                        ArrayList(PerceivedThing).init(allocator),
                        ArrayList(PerceivedThing).init(allocator),
                        ArrayList(PerceivedThing).init(allocator),
                    };
                    defer {
                        for (layers) |layer| {
                            layer.deinit();
                        }
                    }
                    for (frame.others) |other| {
                        try layers[getPhysicsLayer(other.species)].append(other);
                    }
                    for (layers) |layer| {
                        for (layer.items) |other| {
                            _ = renderThing(renderer, progress, move_frame_time, screen_display_position, other);
                        }
                    }
                }
                const render_position = renderThing(renderer, progress, move_frame_time, screen_display_position, frame.self);
                // render input prompt
                if (display_any_input_prompt) {
                    switch (state.input_prompt) {
                        .none => {},
                        .attack => {
                            textures.renderSprite(renderer, textures.sprites.dagger, render_position);
                        },
                        .kick => {
                            textures.renderSprite(renderer, textures.sprites.kick, render_position);
                        },
                    }
                }
                // render activity effects
                for (frame.others) |other| {
                    renderActivity(renderer, progress, move_frame_time, screen_display_position, other);
                }
                renderActivity(renderer, progress, move_frame_time, screen_display_position, frame.self);

                // sidebar
                {
                    const anatomy_coord = makeCoord(512, 0);
                    {
                        const sidebard_render_rect = sdl.makeRect(Rect{
                            .x = anatomy_coord.x,
                            .y = anatomy_coord.y,
                            .width = 200,
                            .height = 512,
                        });
                        sdl.assertZero(sdl.c.SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
                        sdl.assertZero(sdl.c.SDL_RenderFillRect(renderer, &sidebard_render_rect));
                    }

                    const AnatomySprites = struct {
                        diagram: Rect,
                        being_digested: ?Rect = null,
                        leg_wound: ?Rect = null,
                        grappled: ?Rect = null,
                        limping: ?Rect = null,
                        grappling_back: ?Rect = null,
                        digesting: ?Rect = null,
                        grappling_front: ?Rect = null,
                    };
                    const anatomy_sprites = switch (core.game_logic.getAnatomy(frame.self.species)) {
                        .humanoid => AnatomySprites{
                            .diagram = textures.large_sprites.humanoid,
                            .being_digested = textures.large_sprites.humanoid_being_digested,
                            .leg_wound = textures.large_sprites.humanoid_leg_wound,
                            .grappled = textures.large_sprites.humanoid_grappled,
                            .limping = textures.large_sprites.humanoid_limping,
                        },
                        .centauroid => AnatomySprites{
                            .diagram = textures.large_sprites.centauroid,
                            .being_digested = textures.large_sprites.centauroid_being_digested,
                            .leg_wound = textures.large_sprites.centauroid_leg_wound,
                            .grappled = textures.large_sprites.centauroid_grappled,
                            .limping = textures.large_sprites.centauroid_limping,
                        },
                        .bloboid => AnatomySprites{
                            .diagram = switch (frame.self.species.blob) {
                                .small_blob => switch (frame.self.position) {
                                    .small => textures.large_sprites.bloboid_small,
                                    .large => textures.large_sprites.bloboid_small_wide,
                                },
                                .large_blob => switch (frame.self.position) {
                                    .small => textures.large_sprites.bloboid_large,
                                    .large => textures.large_sprites.bloboid_large_wide,
                                },
                            },
                            .grappling_back = textures.large_sprites.bloboid_grappling_back,
                            .digesting = textures.large_sprites.bloboid_digesting,
                            .grappling_front = textures.large_sprites.bloboid_grappling_front,
                        },
                        .kangaroid => AnatomySprites{
                            .diagram = textures.large_sprites.kangaroid,
                            .being_digested = textures.large_sprites.kangaroid_being_digested,
                            .leg_wound = textures.large_sprites.kangaroid_leg_wound,
                            .grappled = textures.large_sprites.kangaroid_grappled,
                            .limping = textures.large_sprites.kangaroid_limping,
                        },
                        .quadruped => AnatomySprites{
                            .diagram = textures.large_sprites.quadruped,
                            .being_digested = textures.large_sprites.quadruped_being_digested,
                            .leg_wound = textures.large_sprites.quadruped_leg_wound,
                            .grappled = textures.large_sprites.quadruped_grappled,
                            .limping = textures.large_sprites.quadruped_limping,
                        },
                    };
                    textures.renderLargeSprite(renderer, anatomy_sprites.diagram, anatomy_coord);

                    if (frame.self.has_shield) {
                        textures.renderLargeSprite(renderer, textures.large_sprites.humanoid_shieled, anatomy_coord);
                    }
                    // explicit integer here to provide a compile error when new items get added.
                    var status_conditions: u6 = frame.self.status_conditions;
                    if (0 != status_conditions & core.protocol.StatusCondition_being_digested) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.being_digested.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_wounded_leg) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.leg_wound.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_grappled) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.grappled.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_limping) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.limping.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_grappling) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.grappling_back.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_digesting) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.digesting.?, anatomy_coord);
                    }
                    if (0 != status_conditions & core.protocol.StatusCondition_grappling) {
                        textures.renderLargeSprite(renderer, anatomy_sprites.grappling_front.?, anatomy_coord);
                    }
                }

                // tutorials
                var maybe_tutorial_text: ?[]const u8 = null;
                if (state.animations != null and state.animations.?.turns > 10) {
                    maybe_tutorial_text = "Use Escape to skip animations.";
                } else if (frame.self.activity == .death) {
                    maybe_tutorial_text = "You died. Use Backspace to undo.";
                } else if (state.consecutive_undos >= 6 and !state.performed_restart) {
                    maybe_tutorial_text = "Press R to restart the current level.";
                } else if (frame.completed_levels == the_levels.len - 1) {
                    maybe_tutorial_text = "You're are win. Use Ctrl+R to quit.";
                } else if (state.observed_kangaroo_death and state.kicks_performed < 2 and canKick(frame.self.species)) {
                    maybe_tutorial_text = "You learned to kick! Use K then a direction.";
                } else if (frame.self.species == .rhino and !state.charge_performed) {
                    maybe_tutorial_text = "Press C to charge.";
                } else if (state.moves_performed < 2) {
                    maybe_tutorial_text = "Use Arrow Keys to move.";
                } else if (state.attacks_performed < 2) {
                    maybe_tutorial_text = "Press F then a direction to attack.";
                } else if (!state.performed_wait and frame.self.status_conditions & core.protocol.StatusCondition_digesting != 0) {
                    maybe_tutorial_text = "Use Spacebar to wait.";
                }
                if (maybe_tutorial_text) |tutorial_text| {
                    // gentle up/down bob
                    var animated_y: i32 = @divFloor(@mod(now, 2000), 100);
                    if (animated_y > 10) animated_y = 20 - animated_y;
                    const coord = makeCoord(512 / 2 - 384 / 2, 512 - 32 + animated_y);
                    _ = textures.renderTextScaled(renderer, tutorial_text, coord, true, 1);
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

fn mainMenuState(save_file: *SaveFile) GameState {
    _ = save_file;
    return GameState{ .main_menu = .{
        .entry_count = 2,
        .cursor_position = 0,
    } };
}

fn startPuzzleGame(game_state: *GameState, levels_to_skip: usize) !void {
    game_state.* = GameState{
        .running = .{
            .client = try FancyClient.init(GameEngineClient.init()),
            .new_game_settings = .{ .puzzle_level = levels_to_skip },
        },
    };
    try game_state.running.client.client.startAsThread();
    try game_state.running.client.startGame(.puzzle_levels);
    try game_state.running.client.beatLevelMacro(levels_to_skip);
    game_state.running.client.stopUndoPastLevel(levels_to_skip);
}

fn startGame(game_state: *GameState) !void {
    game_state.* = GameState{
        .running = .{
            .client = try FancyClient.init(GameEngineClient.init()),
            .new_game_settings = .regular,
        },
    };
    try game_state.running.client.client.startAsThread();
    try game_state.running.client.startGame(.regular);
}

fn doDirectionInput(state: *RunningState, delta: Coord) !void {
    switch (state.input_prompt) {
        .none => {},
        .attack => {
            try state.client.attack(delta);
            state.input_prompt = .none;
            return;
        },
        .kick => {
            try state.client.kick(delta);
            state.input_prompt = .none;
            return;
        },
    }

    // The default input behavior is a move-like action.
    const myself = state.client_state.?.self;
    if (core.game_logic.canMoveNormally(myself.species)) {
        return state.client.move(delta);
    } else if (core.game_logic.canGrowAndShrink(myself.species)) {
        switch (myself.position) {
            .small => {
                return state.client.act(Action{ .grow = delta });
            },
            .large => |large_position| {
                // which direction should we shrink?
                const position_delta = large_position[0].minus(large_position[1]);
                if (position_delta.equals(delta)) {
                    // foward
                    return state.client.act(Action{ .shrink = 0 });
                } else if (position_delta.equals(delta.scaled(-1))) {
                    // backward
                    return state.client.act(Action{ .shrink = 1 });
                } else {
                    // You cannot shrink sideways.
                    return;
                }
            },
        }
    } else {
        // You're not a moving one.
        return;
    }
}

fn doAutoDirectionInput(state: *RunningState, delta: Coord) !void {
    state.input_prompt = .none;

    const myself = state.client_state.?.self;
    if (core.game_logic.canMoveNormally(myself.species)) {
        return state.client.autoMove(delta);
    }
}

fn positionedRect32(position: Coord) Rect {
    return core.geometry.makeRect(position, makeCoord(32, 32));
}

fn getDisplayRect(progress: i32, progress_denominator: i32, thing: PerceivedThing) Rect {
    const head_coord = getHeadPosition(thing.position);
    switch (thing.activity) {
        .movement => |move_delta| {
            return positionedRect32(core.geometry.bezierMove(
                head_coord.scaled(32),
                head_coord.scaled(32).plus(move_delta.scaled(32)),
                progress,
                progress_denominator,
            ));
        },
        .failed_movement => |move_delta| {
            return positionedRect32(core.geometry.bezierBounce(
                head_coord.scaled(32),
                head_coord.scaled(32).plus(move_delta.scaled(32)),
                progress,
                progress_denominator,
            ));
        },
        .growth => |delta| {
            const start_position = thing.position.small;
            const end_position = makeCoord(
                if (delta.x < 0) start_position.x - 1 else start_position.x,
                if (delta.y < 0) start_position.y - 1 else start_position.y,
            );
            const start_size = makeCoord(1, 1);
            const end_size = makeCoord(
                if (delta.x == 0) 1 else 2,
                if (delta.y == 0) 1 else 2,
            );
            return core.geometry.makeRect(
                core.geometry.bezierMove(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierMove(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        .failed_growth => |delta| {
            const start_position = thing.position.small;
            const end_position = makeCoord(
                if (delta.x < 0) start_position.x - 1 else start_position.x,
                if (delta.y < 0) start_position.y - 1 else start_position.y,
            );
            const start_size = makeCoord(1, 1);
            const end_size = makeCoord(
                if (delta.x == 0) 1 else 2,
                if (delta.y == 0) 1 else 2,
            );
            return core.geometry.makeRect(
                core.geometry.bezierBounce(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierBounce(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        .shrink => |index| {
            const large_position = thing.position.large;
            const position_delta = large_position[0].minus(large_position[1]);
            const start_position = core.geometry.min(large_position[0], large_position[1]);
            const end_position = large_position[index];
            const start_size = position_delta.abs().plus(makeCoord(1, 1));
            const end_size = makeCoord(1, 1);
            return core.geometry.makeRect(
                core.geometry.bezierMove(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierMove(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        else => {
            if (thing.species == .blob and thing.position == .large) {
                const position_delta = thing.position.large[0].minus(thing.position.large[1]);
                return core.geometry.makeRect(
                    core.geometry.min(thing.position.large[0], thing.position.large[1]).scaled(32),
                    position_delta.abs().plus(makeCoord(1, 1)).scaled(32),
                );
            }
            return positionedRect32(head_coord.scaled(32));
        },
    }
}

fn renderThing(renderer: *sdl.Renderer, progress: i32, progress_denominator: i32, screen_display_position: Coord, thing: PerceivedThing) Coord {
    // compute position
    const display_rect = getDisplayRect(progress, progress_denominator, thing);
    const render_rect = display_rect.translated(screen_display_position.scaled(-1));
    const render_position = display_rect.position().minus(screen_display_position);

    // render main sprite
    switch (thing.species) {
        else => {
            textures.renderSpriteScaled(renderer, speciesToSprite(thing.species), render_rect);
        },
        .rhino => {
            const oriented_delta = if (progress < @divTrunc(progress_denominator, 2))
                thing.position.large[1].minus(thing.position.large[0])
            else blk: {
                const future_position = core.game_logic.applyMovementFromActivity(thing.activity, thing.position);
                break :blk future_position.large[1].minus(future_position.large[0]);
            };
            const tail_render_position = render_position.plus(oriented_delta.scaled(32));
            const rhino_sprite_normalizing_rotation = 0;
            const rotation = directionToRotation(oriented_delta) +% rhino_sprite_normalizing_rotation;
            textures.renderSpriteRotated(renderer, speciesToSprite(thing.species), render_position, rotation);
            textures.renderSpriteRotated(renderer, speciesToTailSprite(thing.species), tail_render_position, rotation);
        },
    }

    // render status effects
    if (thing.status_conditions & (core.protocol.StatusCondition_wounded_leg | core.protocol.StatusCondition_being_digested) != 0) {
        textures.renderSprite(renderer, textures.sprites.wounded, render_position);
    }
    if (thing.status_conditions & (core.protocol.StatusCondition_limping | core.protocol.StatusCondition_grappled) != 0) {
        textures.renderSprite(renderer, textures.sprites.limping, render_position);
    }
    if (thing.has_shield) {
        textures.renderSprite(renderer, textures.sprites.equipment, render_position);
    }

    return render_position;
}

fn renderActivity(renderer: *sdl.Renderer, progress: i32, progress_denominator: i32, screen_display_position: Coord, thing: PerceivedThing) void {
    const display_position = getDisplayRect(progress, progress_denominator, thing).position();
    const render_position = display_position.minus(screen_display_position);

    switch (thing.activity) {
        .none => {},
        .movement => {},
        .failed_movement => {},
        .growth => {},
        .failed_growth => {},
        .shrink => {},

        .attack => |data| {
            const max_range = core.game_logic.getAttackRange(thing.species);
            if (max_range == 1) {
                const dagger_sprite_normalizing_rotation = 1;
                textures.renderSpriteRotated(
                    renderer,
                    textures.sprites.dagger,
                    core.geometry.bezierBounce(
                        render_position.plus(data.direction.scaled(32 * 2 / 4)),
                        render_position.plus(data.direction.scaled(32 * 4 / 4)),
                        progress,
                        progress_denominator,
                    ),
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
                        render_position,
                        render_position.plus(data.direction.scaled(32 * max_range)),
                        clamped_progress,
                        progress_denominator * max_range,
                    ),
                    directionToRotation(data.direction) +% arrow_sprite_normalizing_rotation,
                );
            }
        },

        .kick => |coord| {
            const kick_sprite_normalizing_rotation = 6;
            textures.renderSpriteRotated(
                renderer,
                textures.sprites.kick,
                core.geometry.bezierBounce(
                    render_position.plus(coord.scaled(32 * 2 / 4)),
                    render_position.plus(coord.scaled(32 * 4 / 4)),
                    progress,
                    progress_denominator,
                ),
                directionToRotation(coord) +% kick_sprite_normalizing_rotation,
            );
        },

        .polymorph => {
            const sprites = textures.sprites.polymorph_effect[4..];
            const sprite_index = @divTrunc(progress * @intCast(i32, sprites.len), progress_denominator);
            textures.renderSprite(renderer, sprites[@intCast(usize, sprite_index)], render_position);
        },

        .death => {
            textures.renderSprite(renderer, textures.sprites.death, render_position);
        },
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

fn hashU32(input: u32) u32 {
    // https://nullprogram.com/blog/2018/07/31/
    var x = input;
    x ^= x >> 17;
    x *%= 0xed5ad4bb;
    x ^= x >> 11;
    x *%= 0xac4c1b51;
    x ^= x >> 15;
    x *%= 0x31848bab;
    x ^= x >> 14;
    return x;
}

fn speciesToSprite(species: Species) Rect {
    return switch (species) {
        .human => textures.sprites.human,
        .orc => textures.sprites.orc,
        .centaur => textures.sprites.centaur_archer,
        .turtle => textures.sprites.turtle,
        .rhino => textures.sprites.rhino[0],
        .kangaroo => textures.sprites.kangaroo,
        .blob => |subspecies| switch (subspecies) {
            .small_blob => textures.sprites.pink_blob,
            .large_blob => textures.sprites.blob_large,
        },
        .wolf => textures.sprites.dog,
        .rat => textures.sprites.rat,
        .wood_golem => textures.sprites.wood_golem,
    };
}

fn speciesToTailSprite(species: Species) Rect {
    return switch (species) {
        .rhino => textures.sprites.rhino[1],
        else => unreachable,
    };
}

const slow_animation_speed = 300;
const fast_animation_speed = 100;
const hyper_animation_speed = 10;

const Animations = struct {
    start_time: i32,
    turns: u32 = 0,
    last_turn_first_frame: usize = 0,
    time_per_frame: i32,
    frames: ArrayListUnmanaged(PerceivedFrame),

    const SomeData = struct { frame_index: usize, progress: i32, time_per_frame: i32 };
    pub fn frameAtTime(self: @This(), now: i32) ?SomeData {
        const animation_time = @bitCast(u32, now -% self.start_time);
        const index = @divFloor(animation_time, @intCast(u32, self.time_per_frame));
        if (index >= self.last_turn_first_frame) {
            // Always show the last turn of animations slowly.
            const time_since_turn = animation_time - @intCast(u32, self.time_per_frame) * @intCast(u32, self.last_turn_first_frame);
            const adjusted_index = self.last_turn_first_frame + @divFloor(time_since_turn, slow_animation_speed);
            if (adjusted_index >= self.frames.items.len - 1) {
                // The last frame is always everyone standing still.
                return null;
            }
            const progress = @intCast(i32, time_since_turn - (adjusted_index - self.last_turn_first_frame) * slow_animation_speed);
            return SomeData{
                .frame_index = adjusted_index,
                .progress = progress,
                .time_per_frame = slow_animation_speed,
            };
        } else if (index >= self.frames.items.len - 1) {
            // The last frame is always everyone standing still.
            return null;
        } else {
            const progress = @intCast(i32, animation_time - index * @intCast(u32, self.time_per_frame));
            return SomeData{
                .frame_index = index,
                .progress = progress,
                .time_per_frame = self.time_per_frame,
            };
        }
    }

    pub fn speedUp(self: *@This(), now: i32) void {
        const data = self.frameAtTime(now) orelse return;
        const old_time_per_frame = self.time_per_frame;
        if (self.turns >= 4) {
            // We're falling behind. Activate emergency NOS.
            self.time_per_frame = hyper_animation_speed;
        } else if (self.turns >= 2) {
            self.time_per_frame = fast_animation_speed;
        } else return;
        self.start_time = now - ( //
            self.time_per_frame * @intCast(i32, data.frame_index) + //
            @divFloor(data.progress * self.time_per_frame, old_time_per_frame) //
        );
    }
};

fn loadAnimations(animations: *?Animations, frames: []PerceivedFrame, now: i32) !void {
    if (animations.* == null or animations.*.?.frameAtTime(now) == null) {
        animations.* = Animations{
            .start_time = now,
            .time_per_frame = slow_animation_speed,
            .frames = .{},
        };
    }
    animations.*.?.last_turn_first_frame = animations.*.?.frames.items.len -| 1;
    animations.*.?.turns += 1;
    if (animations.*.?.turns > 1) {
        animations.*.?.speedUp(now);
    }
    var have_previous_frame = animations.*.?.frames.items.len > 0;
    for (frames) |frame, i| {
        if (have_previous_frame and i < frames.len - 1) {
            // try compressing the new frame into the previous one.
            if (try tryCompressingFrames(lastPtr(animations.*.?.frames.items), frame)) {
                continue;
            }
        } else {
            have_previous_frame = true;
        }
        try animations.*.?.frames.append(allocator, frame);
    }
}

fn tryCompressingFrames(base_frame: *PerceivedFrame, patch_frame: PerceivedFrame) !bool {
    core.debug.animation_compression.print("trying to compress frames...", .{});

    if (base_frame.others.len != patch_frame.others.len) {
        core.debug.animation_compression.print("NOPE: A change to the number of people we can is too complex to compress.", .{});
        return false;
    }

    // The order of frame.others is not meaningful, so we need to derrive continuity ourselves,
    // which is intentionally not possible sometimes.
    const others_mapping = try allocator.alloc(usize, base_frame.others.len);
    for (base_frame.others) |base_other, i| {
        const new_position = core.game_logic.applyMovementFromActivity(base_other.activity, base_other.position);

        var mapped_index: ?usize = null;
        for (patch_frame.others) |patch_other, j| {
            // Are these the same person?
            const could_be = blk: {
                if (base_other.species != @as(std.meta.Tag(Species), patch_other.species)) break :blk false;
                if (!core.game_logic.positionEquals(
                    new_position,
                    patch_other.position,
                )) break :blk false;
                break :blk true;
            };
            if (could_be) {
                if (mapped_index != null) {
                    core.debug.animation_compression.deepPrint("NOPE: individual has too many matches in the next frame: ", base_other);
                    return false;
                }
                // our best guess so far.
                mapped_index = j;
            }
        }

        if (mapped_index) |j| {
            others_mapping[i] = j;
        } else {
            core.debug.animation_compression.deepPrint("NOPE: individual has no matches in the next frame: ", base_other);
            core.debug.animation_compression.deepPrint("  expected position: ", new_position);
            for (patch_frame.others) |patch_other| {
                core.debug.animation_compression.deepPrint("  saw position: ", patch_other.position);
            }
            return false;
        }
    }
    core.debug.animation_compression.print("...all ({}) individuals tracked...", .{base_frame.others.len});

    // If someone does two different things in subsequent frames, then we can't compress those.
    if (base_frame.self.activity != .none and patch_frame.self.activity != .none) {
        core.debug.animation_compression.print("NOPE: self doing two different things.", .{});
        return false;
    }
    for (base_frame.others) |base_other, i| {
        const patch_other = patch_frame.others[others_mapping[i]];
        if (base_other.activity != .none and patch_other.activity != .none) {
            core.debug.animation_compression.print("NOPE: someone({}, {}) doing two different things.", .{ i, others_mapping[i] });
            return false;
        }
    }

    compressPerceivedThings(&base_frame.self, patch_frame.self);
    core.debug.animation_compression.print("...compressing...", .{});
    for (base_frame.others) |*base_other, i| {
        const patch_other = patch_frame.others[others_mapping[i]];
        compressPerceivedThings(base_other, patch_other);
    }
    base_frame.completed_levels = patch_frame.completed_levels;
    // uhhh. how do we compress this?
    _ = patch_frame.terrain;

    return true;
}

fn compressPerceivedThings(base_thing: *PerceivedThing, patch_thing: PerceivedThing) void {
    if (patch_thing.activity == .none) {
        // That was easy.
        return;
    }
    base_thing.* = patch_thing;
}

fn lastPtr(arr: anytype) @TypeOf(&arr[arr.len - 1]) {
    return &arr[arr.len - 1];
}
