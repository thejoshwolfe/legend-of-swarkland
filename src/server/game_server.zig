const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const GameEngine = @import("./game_engine.zig").GameEngine;
const GameState = @import("./game_engine.zig").GameState;
const IdMap = @import("./game_engine.zig").IdMap;
const Individual = @import("./game_engine.zig").Individual;
const GameEngineClient = @import("../client/game_engine_client.zig").GameEngineClient;
const Socket = core.protocol.Socket;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const PerceivedHappening = core.protocol.PerceivedHappening;

const StateDiff = @import("./game_engine.zig").StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_socket: *Socket) !void {
    core.debug.nameThisThread("core");
    defer core.debug.unnameThisThread();
    core.debug.warn("init");
    defer core.debug.warn("shutdown");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    var game_state = GameState.init(allocator);

    core.debug.warn("start ai clients and send welcome messages");
    var decision_makers = IdMap(*Socket).init(allocator);
    var ai_clients = IdMap(*GameEngineClient).init(allocator);
    const main_player_id: u32 = 1;
    try decision_makers.putNoClobber(main_player_id, main_player_socket);
    {
        const happenings = try game_engine.getStartGameHappenings();
        try game_state.applyStateChanges(happenings.state_changes);
        for (happenings.state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    if (individual.id == main_player_id) continue;
                    try handleSpawn(&decision_makers, &ai_clients, individual);
                },
                else => unreachable,
            }
        }
        // Welcome to swarkland!
        var iterator = decision_makers.iterator();
        while (iterator.next()) |kv| {
            const id = kv.key;
            const socket = kv.value;
            try socket.out().write(Response{ .load_state = try game_engine.getStaticPerception(game_state, id) });
        }
    }

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
                for (state_changes) |diff| {
                    switch (diff) {
                        .despawn => |individual| {
                            try handleSpawn(&decision_makers, &ai_clients, individual);
                        },
                        .spawn => |individual| {
                            handleDespawn(&decision_makers, &ai_clients, individual.id);
                        },
                        else => {},
                    }
                }
            }
            // The people demand a response from their decision.
            // Even if we didn't actually do a rewind, send a response to everyone,
            // (as long as they still exist).
            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const socket = kv.value;
                try socket.out().write(Response{ .load_state = try game_engine.getStaticPerception(game_state, id) });
            }
        } else {

            // Time goes forward.
            const happenings = try game_engine.computeHappenings(game_state, actions);
            //core.debug.deep_print("happenings: ", happenings);
            try pushHistoryRecord(&history, happenings.state_changes);
            try game_state.applyStateChanges(happenings.state_changes);
            for (happenings.state_changes) |diff| {
                switch (diff) {
                    .spawn => |individual| {
                        try handleSpawn(&decision_makers, &ai_clients, individual);
                    },
                    .despawn => |individual| {
                        handleDespawn(&decision_makers, &ai_clients, individual.id);
                    },
                    else => {},
                }
            }

            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const socket = kv.value;
                try socket.out().write(Response{
                    .stuff_happens = PerceivedHappening{
                        .frames = happenings.individual_to_perception.get(id).?.value,
                        .static_perception = try game_engine.getStaticPerception(game_state, id),
                    },
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

fn handleSpawn(decision_makers: *IdMap(*Socket), ai_clients: *IdMap(*GameEngineClient), individual: Individual) !void {
    // initialize ai socket
    var client = try allocator.create(GameEngineClient);
    try ai_clients.putNoClobber(individual.id, client);
    var ai_socket = try client.startAsAi(individual.id);
    try decision_makers.putNoClobber(individual.id, ai_socket);
}

fn handleDespawn(decision_makers: *IdMap(*Socket), ai_clients: *IdMap(*GameEngineClient), id: u32) void {
    ai_clients.getValue(id).?.stopAi();
    ai_clients.removeAssertDiscard(id);
    decision_makers.removeAssertDiscard(id);
}
