const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const GameEngine = @import("./game_engine.zig").GameEngine;
const GameState = @import("./game_engine.zig").GameState;
const IdMap = @import("./game_engine.zig").IdMap;
const GameEngineClient = @import("../client/game_engine_client.zig").GameEngineClient;
const Socket = core.protocol.Socket;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;

const StateDiff = @import("./game_engine.zig").StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_socket: *Socket) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init");
    defer core.debug.warn("shutdown");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    var game_state = GameState.init(allocator);

    core.debug.warn("start ai clients and send welcome messages");
    var decision_makers = IdMap(*Socket).init(allocator);
    var ai_clients = IdMap(*GameEngineClient).init(allocator);
    {
        const happenings = try game_engine.getStartGameHappenings();
        try game_state.applyStateChanges(happenings.state_changes);
        for (happenings.state_changes) |diff, i| {
            switch (diff) {
                .spawn => |individual| {
                    var socket = if (i == 0) main_player_socket else blk: {
                        // initialize ai socket
                        var client = try allocator.create(GameEngineClient);
                        try ai_clients.putNoClobber(individual.id, client);
                        break :blk try client.startAsAi(individual.id);
                    };
                    try decision_makers.putNoClobber(individual.id, socket);

                    // welcome
                    try socket.out().write(Response{
                        .static_perception = try game_engine.getStaticPerception(game_state, individual.id),
                    });
                },
                else => unreachable,
            }
        }
    }
    const main_player_id: u32 = 1;

    var history = HistoryList.init();

    core.debug.warn("start main loop");
    mainLoop: while (true) {
        // do ai
        {
            var iterator = ai_clients.iterator();
            while (iterator.next()) |kv| {
                var ai_client = kv.value;
                // TODO: actual ai
                try ai_client.move(Coord{ .x = 0, .y = 1 });
            }
        }

        // read all the inputs, which will block for the human client.
        var actions = IdMap(Action).init(allocator);
        var is_rewind = false;
        {
            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                var socket = kv.value;
                switch (try socket.in(allocator).read(Request)) {
                    .quit => {
                        std.debug.assert(kv.key == main_player_id);
                        core.debug.warn("clean shutdown. close");
                        // close all ai threads first, because the main player client doesn't know to wait for the ai threads.
                        var ai_iterator = ai_clients.iterator();
                        while (ai_iterator.next()) |ai_kv| {
                            var ai_client = ai_kv.value;
                            ai_client.stopAi();
                        }
                        core.debug.warn("all ais shutdown");
                        // now shutdown the main player
                        socket.close(Response.game_over);
                        break :mainLoop;
                    },
                    .act => |action| {
                        std.debug.assert(game_engine.validateAction(action));
                        try actions.putNoClobber(kv.key, action);
                    },
                    .rewind => {
                        std.debug.assert(kv.key == main_player_id);
                        // delay actually rewinding so that we receive all requests.
                        is_rewind = true;
                    },
                }
            }
        }

        if (is_rewind) {

            // Time goes backward.
            if (rewind(&history)) |state_changes| {
                try game_state.undoStateChanges(state_changes);
            }
            // Regardless of whether that succeeded, send the rewind to everyone.
            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const socket = kv.value;
                try socket.out().write(Response{ .undo = try game_engine.getStaticPerception(game_state, id) });
            }
        } else {

            // Time goes forward.
            const happenings = try game_engine.computeHappenings(game_state, actions);
            //core.debug.deep_print("happenings: ", happenings);
            try pushHistoryRecord(&history, happenings.state_changes);
            try game_state.applyStateChanges(happenings.state_changes);

            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const socket = kv.value;
                try socket.out().write(Response{
                    .stuff_happens = happenings.individual_to_perception.get(id).?.value,
                });
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
