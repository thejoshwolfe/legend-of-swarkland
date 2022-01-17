const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("../index.zig");
const ai = @import("./ai.zig");
const GameEngine = @import("./game_engine.zig").GameEngine;
const game_model = @import("./game_model.zig");
const GameState = game_model.GameState;
const IdMap = game_model.IdMap;
const SomeQueues = core.protocol.SomeQueues;
const Request = core.protocol.Request;
const NewGameSettings = core.protocol.NewGameSettings;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const PerceivedHappening = core.protocol.PerceivedHappening;

const validateAction = core.game_logic.validateAction;

const StateDiff = game_model.StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_queues: *SomeQueues) !void {
    var new_game_settings: NewGameSettings = readSettings: while (true) {
        if (main_player_queues.waitAndTakeRequest()) |request| {
            switch (request) {
                .start_game => |settings| {
                    var other_settings: NewGameSettings = settings;
                    break :readSettings other_settings;
                },
                else => {},
            }
        }
        core.debug.warning.print("Main player didn't start the game!", .{});
        core.debug.thread_lifecycle.print("clean shutdown. close", .{});
        main_player_queues.closeResponses();
        return;
    } else unreachable;

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    var game_state = try GameState.generate(allocator, new_game_settings);

    // create ai clients
    const main_player_id: u32 = 1;
    var you_are_alive = true;
    // Welcome to swarkland!
    try main_player_queues.enqueueResponse(Response{ .load_state = try game_engine.getStaticPerception(&game_state, main_player_id) });

    var response_for_ais = IdMap(Response).init(allocator);
    var history = HistoryList{};

    // start main loop
    mainLoop: while (true) {
        var actions = IdMap(Action).init(allocator);

        // do ai
        for (game_state.individuals.keys()) |id| {
            if (id == main_player_id) continue;
            const response = response_for_ais.get(id) orelse Response{ .load_state = try game_engine.getStaticPerception(&game_state, id) };
            const action = doAi(response);
            const individual = game_state.individuals.get(id).?;
            validateAction(individual.species, individual.abs_position, individual.status_conditions, action) catch |err| @panic(@errorName(err));
            debugPrintAction(id, action);
            try actions.putNoClobber(id, action);
        }
        response_for_ais.clearRetainingCapacity();

        // read all the inputs, which will block for the human client.
        var is_rewind = false;
        {
            retryRead: while (true) {
                const request = main_player_queues.waitAndTakeRequest() orelse {
                    core.debug.thread_lifecycle.print("clean shutdown. close", .{});
                    main_player_queues.closeResponses();
                    break :mainLoop;
                };
                switch (request) {
                    .act => |action| {
                        if (!you_are_alive) {
                            // no. you're are dead.
                            core.debug.warning.print("Main player attempted action while dead!", .{});
                            try main_player_queues.enqueueResponse(Response{ .reject_request = request });
                            continue :retryRead;
                        }
                        const individual = game_state.individuals.get(main_player_id).?;
                        validateAction(individual.species, individual.abs_position, individual.status_conditions, action) catch |err| {
                            core.debug.warning.print("Main player attempted invalid action: {s}", .{@errorName(err)});
                            try main_player_queues.enqueueResponse(Response{ .reject_request = request });
                            continue :retryRead;
                        };
                        try actions.putNoClobber(main_player_id, action);
                    },
                    .rewind => {
                        if (history.len == 0) {
                            // What do you want me to do, send you back to the main menu?
                            core.debug.warning.print("Main player attempted to undo the beginning of the game!", .{});
                            try main_player_queues.enqueueResponse(Response{ .reject_request = request });
                            continue :retryRead;
                        }
                        // delay actually rewinding so that we receive all requests.
                        is_rewind = true;
                    },
                    .start_game => {
                        core.debug.warning.print("Main player attempted to start an already started game?", .{});
                        try main_player_queues.enqueueResponse(Response{ .reject_request = request });
                        continue :retryRead;
                    },
                }
                break;
            }
        }

        if (is_rewind) {

            // Time goes backward.
            const state_changes = rewind(&history).?;
            try game_state.undoStateChanges(state_changes);
            for (state_changes) |_, i| {
                const diff = state_changes[state_changes.len - 1 - i];
                switch (diff) {
                    .despawn => |individual| {
                        if (individual.id == main_player_id) {
                            you_are_alive = true;
                        }
                    },
                    else => {},
                }
            }
            try main_player_queues.enqueueResponse(Response{ .load_state = try game_engine.getStaticPerception(&game_state, main_player_id) });
        } else {

            // Time goes forward.
            var scratch_game_state = try game_state.clone();
            const happenings = try game_engine.computeHappenings(&scratch_game_state, actions);
            core.debug.happening.deepPrint("happenings: ", happenings);
            try pushHistoryRecord(&history, happenings.state_changes);
            try game_state.applyStateChanges(happenings.state_changes);
            for (happenings.state_changes) |diff| {
                switch (diff) {
                    .despawn => |id_and_individual| {
                        if (id_and_individual.id == main_player_id) {
                            you_are_alive = false;
                        }
                    },
                    else => {},
                }
            }

            var iterator = happenings.individual_to_perception.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key_ptr.*;
                const response = Response{
                    .stuff_happens = PerceivedHappening{
                        .frames = kv.value_ptr.*,
                    },
                };
                if (id == main_player_id) {
                    try main_player_queues.enqueueResponse(response);
                } else {
                    try response_for_ais.putNoClobber(id, response);
                }
            }
        }
    }
}

fn rewind(history: *HistoryList) ?[]StateDiff {
    const node = history.pop() orelse return null;
    return node.data;
}

fn pushHistoryRecord(history: *HistoryList, state_changes: []StateDiff) !void {
    const history_node: *HistoryNode = try allocator.create(HistoryNode);
    history_node.data = state_changes;
    history.append(history_node);
}

fn doAi(response: Response) Action {
    // This should be sitting waiting for us already, since we just wrote it earlier.
    var last_frame = switch (response) {
        .load_state => |frame| frame,
        .stuff_happens => |perceived_happening| perceived_happening.frames[perceived_happening.frames.len - 1],
        else => @panic("unexpected response type in AI"),
    };
    return ai.getNaiveAiDecision(last_frame);
}

pub fn debugPrintAction(prefix_number: u32, action: Action) void {
    switch (action) {
        .wait => core.debug.actions.print("{}: Action{{ .wait = {{}} }},", .{prefix_number}),
        .move => |direction| core.debug.actions.print("{}: Action{{ .move = .{s} }},", .{ prefix_number, @tagName(direction) }),
        .charge => core.debug.actions.print("{}: Action{{ .charge = {{}} }},", .{prefix_number}),
        .grow => |direction| core.debug.actions.print("{}: Action{{ .grow = .{s} }},", .{ prefix_number, @tagName(direction) }),
        .shrink => |index| core.debug.actions.print("{}: Action{{ .shrink = {} }},", .{ prefix_number, index }),
        .attack => |direction| core.debug.actions.print("{}: Action{{ .attack = .{s} }},", .{ prefix_number, @tagName(direction) }),
        .nibble => core.debug.actions.print("{}: Action{{ .nibble = {{}} }},", .{prefix_number}),
        .stomp => core.debug.actions.print("{}: Action{{ .stomp = {{}} }},", .{prefix_number}),
        .lunge => |direction| core.debug.actions.print("{}: Action{{ .lunge = .{s} }},", .{ prefix_number, @tagName(direction) }),
        .kick => |direction| core.debug.actions.print("{}: Action{{ .kick = .{s} }},", .{ prefix_number, @tagName(direction) }),
        .open_close => |direction| core.debug.actions.print("{}: Action{{ .open_close = .{s} }},", .{ prefix_number, @tagName(direction) }),

        .cheatcode_warp => |index| core.debug.actions.print("{}: Action{{ .cheatcode_warp = {} }},", .{ prefix_number, index }),
    }
}
