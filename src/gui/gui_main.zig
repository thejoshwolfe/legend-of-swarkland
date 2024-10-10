const std = @import("std");
const ArrayList = std.ArrayList;
const allocator = std.heap.c_allocator;

const textures = @import("textures.zig");
const gui = @import("gui.zig");
const InputEngine = @import("InputEngine.zig");
const SaveFile = @import("SaveFile.zig");
const renderThings = @import("render.zig").renderThings;
const loadAnimations = @import("render.zig").loadAnimations;
const Animations = @import("render.zig").Animations;

const core = @import("core");
const Rect = core.geometry.Rect;
const GameEngineClient = @import("client").GameEngineClient;
const FancyClient = @import("client").FancyClient;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const Action = core.protocol.Action;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;

const handleGameInput = @import("game_input.zig").handleGameInput;
const updateTutorialData = @import("game_input.zig").updateTutorialData;

const doMainMenu = @import("menus.zig").doMainMenu;
const doPuzzleLevelsMenu = @import("menus.zig").doPuzzleLevelsMenu;

const the_levels = @import("server").the_levels;

const logical_window_size = Rect{ .x = 0, .y = 0, .width = 712, .height = 512 };

pub fn main() anyerror!void {
    core.debug.init();
    core.debug.nameThisThread("gui");
    defer core.debug.unnameThisThread();
    core.debug.thread_lifecycle.print("init", .{});
    defer core.debug.thread_lifecycle.print("shutdown", .{});

    var window = try gui.Window.init(
        "Legend of Swarkland",
        logical_window_size.width,
        logical_window_size.height,
    );
    defer window.deinit();

    textures.init(window.renderer);
    defer textures.deinit();

    try doMainLoop(&window);
}

const InputPrompt = enum {
    none,
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

fn doMainLoop(window: *gui.Window) !void {
    var input_engine = InputEngine.init();
    var save_file = try SaveFile.load(allocator);
    var game_state = mainMenuState(&save_file);

    defer switch (game_state) {
        .running => |*state| state.client.client.stopEngine(),
        else => {},
    };

    main_loop: while (true) {
        const now = input_engine.now();

        // Process responses from the server.
        switch (game_state) {
            .main_menu, .level_select => |*menu_state| {
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
                            updateTutorialData(state, happening.frames);

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

        while (input_engine.pollInput()) |input| {
            if (input == .shutdown) {
                core.debug.thread_lifecycle.print("gui quit", .{});
                return;
            }
            switch (game_state) {
                .main_menu, .level_select => |*menu_state| {
                    if (menu_state.handleInput(input) == .back) {
                        if (game_state == .level_select) {
                            game_state = mainMenuState(&save_file);
                            continue :main_loop;
                        }
                    }
                },
                .running => |*state| {
                    if (try handleGameInput(state, input) == .back) {
                        game_state = mainMenuState(&save_file);
                        continue :main_loop;
                    }
                },
            }
        }

        switch (game_state) {
            .main_menu => |*menu_state| {
                switch (doMainMenu(window.renderer, menu_state, save_file.filename)) {
                    .none => {},
                    .new_game => |cheatcode_game_seed| {
                        try startGame(&game_state, cheatcode_game_seed);
                        continue :main_loop;
                    },
                    .puzzle_levels => {
                        game_state = puzzleLevels(&save_file);
                        continue :main_loop;
                    },
                }
            },

            .level_select => |*menu_state| {
                switch (doPuzzleLevelsMenu(window.renderer, menu_state, save_file.completed_levels)) {
                    .none => {},
                    .new_game => |level_index| {
                        try startPuzzleGame(&game_state, level_index);
                        continue :main_loop;
                    },
                }
            },

            .running => |*state| blk: {
                if (state.client_state == null) break :blk; // waiting for first frame from server.

                try renderThings(state, window.renderer, now);
            },
        }

        window.present();
        input_engine.sleepUntilNextFrame();
    }
}

fn mainMenuState(save_file: *const SaveFile) GameState {
    _ = save_file;
    return GameState{ .main_menu = .{
        .entry_count = 2,
        .cursor_position = 0,
    } };
}
fn puzzleLevels(save_file: *const SaveFile) GameState {
    const button_count = if (save_file.completed_levels == the_levels.len - 1)
        save_file.completed_levels
    else
        save_file.completed_levels + 1;

    return GameState{ .level_select = .{
        .entry_count = button_count,
        .cursor_position = button_count - 1,
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

fn startGame(game_state: *GameState, seed: ?u64) !void {
    game_state.* = GameState{
        .running = .{
            .client = try FancyClient.init(GameEngineClient.init()),
            .new_game_settings = .regular,
        },
    };
    try game_state.running.client.client.startAsThread();
    try game_state.running.client.startGame(.{ .regular = .{ .seed = seed } });
}
