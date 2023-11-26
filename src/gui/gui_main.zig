const std = @import("std");
const ArrayList = std.ArrayList;
const allocator = std.heap.c_allocator;

const sdl = @import("sdl.zig");
const textures = @import("textures.zig");
const gui = @import("gui.zig");
const InputEngine = @import("input_engine.zig").InputEngine;
const SaveFile = @import("SaveFile.zig");
const renderThings = @import("render.zig").renderThings;
const loadAnimations = @import("render.zig").loadAnimations;
const Animations = @import("render.zig").Animations;

const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;
const GameEngineClient = @import("client").GameEngineClient;
const FancyClient = @import("client").FancyClient;
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
const Equipment = core.protocol.Equipment;
const EquippedItem = core.protocol.EquippedItem;
const getHeadPosition = core.game_logic.getHeadPosition;
const canCharge = core.game_logic.canCharge;
const canUseDoors = core.game_logic.canUseDoors;

const the_levels = @import("server").the_levels;

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
        if (renderer_info.flags & @as(u32, @bitCast(sdl.c.SDL_RENDERER_TARGETTEXTURE)) == 0) {
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
    open_close,
    defend,
};
const GameState = union(enum) {
    main_menu: gui.LinearMenuState,
    level_select: gui.LinearMenuState,

    running: RunningState,
};
pub const RunningState = struct {
    client: FancyClient,
    client_state: ?PerceivedFrame = null,
    input_prompt: InputPrompt = .none,
    animations: ?Animations = null,

    // tutorial state should *not* reset through undo.
    // these values cap out at small values, because we only use them for showing/hiding tutorials.
    kick_performed: bool = false,
    kick_observed: bool = false,
    charge_performed: bool = false,
    moves_performed: u2 = 0,
    attacks_performed: u2 = 0,
    wait_performed: bool = false,
    // puzzle level tutorials
    consecutive_undos: u3 = 0,
    performed_restart: bool = false,
    // feedback for pressing inputs wrong
    input_tutorial: ?[]const u8 = null,

    new_game_settings: union(enum) {
        regular,
        puzzle_level: usize,
    },
};

fn doMainLoop(renderer: *sdl.Renderer, screen_buffer: *sdl.Texture) !void {
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
        const now = @as(i32, @intCast(sdl.c.SDL_GetTicks()));
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
                                state.input_tutorial = null;
                            }
                            state.client_state = happening.frames[happening.frames.len - 1];

                            // Update tutorial data.
                            state.consecutive_undos = 0;
                            for (happening.frames) |frame| {
                                for (frame.others) |other| {
                                    if (other.kind == .individual and other.kind.individual.activity == .kick) {
                                        state.kick_observed = true;
                                    }
                                }
                                switch (frame.self.kind.individual.activity) {
                                    .kick => {
                                        state.kick_performed = true;
                                    },
                                    .movement => |delta| {
                                        if (core.geometry.isOrthogonalVectorOfMagnitude(delta, 2)) {
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
            switch (event.type) {
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

                                    .greaterthan => {
                                        if (state.input_prompt == .kick) {
                                            clearInputState(state);
                                            try doActionOrShowTutorialForError(state, .stomp);
                                        }
                                    },

                                    .start_attack => {
                                        if (validateAndShowTotorialForError(state, .attack)) {
                                            state.input_prompt = .attack;
                                        }
                                    },
                                    .start_kick => {
                                        _ = validateAndShowTotorialForError(state, .kick);
                                        state.input_prompt = .kick;
                                    },
                                    .start_open_close => {
                                        _ = validateAndShowTotorialForError(state, .open_close);
                                        state.input_prompt = .open_close;
                                    },
                                    .start_defend => {
                                        _ = validateAndShowTotorialForError(state, .defend);
                                        state.input_prompt = .defend;
                                    },
                                    .charge => {
                                        try doActionOrShowTutorialForError(state, .charge);
                                        state.input_prompt = .none;
                                    },
                                    .stomp => {
                                        try doActionOrShowTutorialForError(state, .stomp);
                                        state.input_prompt = .none;
                                    },
                                    .pick_up => {
                                        if (isFloorItemSuggestedEquip(state.client_state.?)) |should_equip| {
                                            if (should_equip) {
                                                try doActionOrShowTutorialForError(state, .pick_up_and_equip);
                                            } else {
                                                try doActionOrShowTutorialForError(state, .pick_up_unequipped);
                                            }
                                        }
                                        state.input_prompt = .none;
                                    },

                                    .equip_0 => try doEquipmentAction(state, .dagger, false),
                                    .force_equip_0 => try doEquipmentAction(state, .dagger, true),
                                    .equip_1 => try doEquipmentAction(state, .torch, false),
                                    .force_equip_1 => try doEquipmentAction(state, .torch, true),

                                    .backspace => {
                                        if (state.input_prompt == .none) {
                                            try state.client.rewind();
                                            state.consecutive_undos +|= 1;
                                        }
                                        clearInputState(state);
                                    },
                                    .spacebar => {
                                        if (state.input_prompt == .none) {
                                            clearInputState(state);
                                            try state.client.act(.wait);
                                            state.wait_performed = true;
                                        }
                                    },
                                    .escape => {
                                        clearInputState(state);
                                        state.animations = null;
                                    },
                                    .restart => {
                                        if (state.new_game_settings == .puzzle_level) {
                                            try state.client.restartLevel();
                                            clearInputState(state);
                                            state.performed_restart = true;
                                        }
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
                                    .warp_0 => try state.client.act(Action{ .cheatcode_warp = 0 }),
                                    .warp_1 => try state.client.act(Action{ .cheatcode_warp = 1 }),
                                    .warp_2 => try state.client.act(Action{ .cheatcode_warp = 2 }),
                                    .warp_3 => try state.client.act(Action{ .cheatcode_warp = 3 }),
                                    .warp_4 => try state.client.act(Action{ .cheatcode_warp = 4 }),
                                    .warp_5 => try state.client.act(Action{ .cheatcode_warp = 5 }),
                                    .warp_6 => try state.client.act(Action{ .cheatcode_warp = 6 }),
                                    .warp_7 => try state.client.act(Action{ .cheatcode_warp = 7 }),

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
                var menu_renderer = gui.Gui.initInteractive(renderer, menu_state, textures.sprites.dagger);

                menu_renderer.seek(10, 10);
                menu_renderer.scale(2);
                menu_renderer.font(.large_bold);
                menu_renderer.marginBottom(5);
                menu_renderer.text("Legend of Swarkland");
                menu_renderer.scale(1);
                menu_renderer.font(.large);
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
                var menu_renderer = gui.Gui.initInteractive(renderer, menu_state, textures.sprites.dagger);
                menu_renderer.seek(32, 32);
                menu_renderer.marginBottom(3);

                for (the_levels[0 .. the_levels.len - 1], 0..) |level, i| {
                    if (i < save_file.completed_levels) {
                        // Past levels
                        if (menu_renderer.button(level.name)) {
                            try startPuzzleGame(&game_state, i);
                            continue :main_loop;
                        }
                    } else if (i == save_file.completed_levels) {
                        // Current level
                        menu_renderer.font(.large_bold);
                        if (menu_renderer.button("??? (New)")) {
                            try startPuzzleGame(&game_state, i);
                            continue :main_loop;
                        }
                        menu_renderer.font(.large);
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

                try renderThings(state, renderer, now);
            },
        }

        {
            sdl.assertZero(sdl.c.SDL_SetRenderTarget(renderer, null));
            sdl.assertZero(sdl.c.SDL_GetRendererOutputSize(renderer, &output_rect.w, &output_rect.h));
            // preserve aspect ratio
            const source_aspect_ratio = comptime @as(f32, @floatFromInt(logical_window_size.w)) / @as(f32, @floatFromInt(logical_window_size.h));
            const dest_aspect_ratio = @as(f32, @floatFromInt(output_rect.w)) / @as(f32, @floatFromInt(output_rect.h));
            if (source_aspect_ratio > dest_aspect_ratio) {
                // use width
                const new_height = @as(c_int, @intFromFloat(@as(f32, @floatFromInt(output_rect.w)) / source_aspect_ratio));
                output_rect.x = 0;
                output_rect.y = @divTrunc(output_rect.h - new_height, 2);
                output_rect.h = new_height;
            } else {
                // use height
                const new_width = @as(c_int, @intFromFloat(@as(f32, @floatFromInt(output_rect.h)) * source_aspect_ratio));
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
    const action = switch (state.input_prompt) {
        .none => blk: {
            // The default input behavior is a move-like action.
            const myself = state.client_state.?.self;
            if (core.game_logic.canMoveNormally(myself.kind.individual.species)) {
                break :blk Action{ .move = deltaToCardinalDirection(delta) };
            } else if (core.game_logic.canGrowAndShrink(myself.kind.individual.species)) {
                switch (myself.position) {
                    .small => {
                        break :blk Action{ .grow = deltaToCardinalDirection(delta) };
                    },
                    .large => |large_position| {
                        // which direction should we shrink?
                        const position_delta = large_position[0].minus(large_position[1]);
                        if (position_delta.equals(delta)) {
                            // foward
                            break :blk Action{ .shrink = 0 };
                        } else if (position_delta.equals(delta.scaled(-1))) {
                            // backward
                            break :blk Action{ .shrink = 1 };
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
        },
        .attack => Action{ .attack = deltaToCardinalDirection(delta) },
        .kick => Action{ .kick = deltaToCardinalDirection(delta) },
        .open_close => Action{ .open_close = deltaToCardinalDirection(delta) },
        .defend => Action{ .defend = deltaToCardinalDirection(delta) },
    };
    clearInputState(state);
    try doActionOrShowTutorialForError(state, action);
}

fn doAutoDirectionInput(state: *RunningState, delta: Coord) !void {
    clearInputState(state);

    const myself = state.client_state.?.self;
    if (core.game_logic.canMoveNormally(myself.kind.individual.species)) {
        return state.client.autoMove(delta);
    }
}

fn doActionOrShowTutorialForError(state: *RunningState, action: Action) !void {
    if (validateAndShowTotorialForError(state, action)) {
        try state.client.act(action);
    }
}

var tutorial_text_buffer: [0x100]u8 = undefined;
fn validateAndShowTotorialForError(state: *RunningState, action: std.meta.Tag(Action)) bool {
    const me = state.client_state.?.self;
    if (core.game_logic.validateAction(me.kind.individual.species, me.position, me.kind.individual.status_conditions, me.kind.individual.equipment, action)) {
        state.input_tutorial = null;
        return true;
    } else |err| switch (err) {
        error.StatusForbids => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "You cannot {s}. Try Spacebar to wait.", .{@tagName(action)}) catch unreachable;
        },
        error.SpeciesIncapable => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "A {s} cannot {s}.", .{ @tagName(me.kind.individual.species), @tagName(action) }) catch unreachable;
        },
        error.MissingItem => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "You are missing an item.", .{}) catch unreachable;
        },
        error.TooBig, error.TooSmall => unreachable,
    }
    return false;
}

fn clearInputState(state: *RunningState) void {
    state.input_prompt = .none;
    state.input_tutorial = null;
    state.client.cancelAutoAction();
}

fn getUnequipAction(equipment: Equipment) ?Action {
    if (equipment.is_equipped(.axe)) {
        return .{ .unequip = .axe };
    }
    if (equipment.is_equipped(.dagger)) {
        return .{ .unequip = .dagger };
    }
    if (equipment.is_equipped(.torch)) {
        return .{ .unequip = .torch };
    }
    return null;
}

pub fn isFloorItemSuggestedEquip(frame: PerceivedFrame) ?bool {
    const coord = getHeadPosition(frame.self.position);
    for (frame.others) |other| {
        switch (other.kind) {
            .individual => continue,
            .item => |item| {
                if (!coord.equals(other.position.small)) continue;
                return switch (item) {
                    .dagger, .axe => true,
                    .shield => true,
                    .torch => false,
                };
            },
        }
    }
    return null;
}

fn doEquipmentAction(state: *RunningState, item: EquippedItem, force: bool) !void {
    if (state.input_prompt != .none) return;
    const myself = state.client_state.?.self;
    const equipment = myself.kind.individual.equipment;
    if (equipment.is_equipped(item)) {
        try doActionOrShowTutorialForError(state, .{ .unequip = item });
    } else if (equipment.is_held(item)) {
        if (!force) {
            if (getUnequipAction(equipment)) |unequip_action| {
                return doActionOrShowTutorialForError(state, unequip_action);
            }
        }
        try doActionOrShowTutorialForError(state, .{ .equip = item });
    }
}
