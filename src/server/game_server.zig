const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const GameEngine = @import("./game_engine.zig").GameEngine;
const GameState = @import("./game_engine.zig").GameState;
const IdMap = @import("./game_engine.zig").IdMap;
const GameEngineClient = @import("../client/game_engine_client.zig").GameEngineClient;
const Channel = core.protocol.Channel;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_channel: *Channel) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init");
    defer core.debug.warn("shutdown");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);

    core.debug.warn("start ai clients and send welcome messages");
    var decision_makers = IdMap(*Channel).init(allocator);
    var ai_clients = IdMap(*GameEngineClient).init(allocator);
    {
        const happenings = try game_engine.getStartGameHappenings();
        try game_engine.game_state.applyStateChanges(happenings.state_changes);
        for (happenings.state_changes) |diff, i| {
            switch (diff) {
                .spawn => |individual| {
                    var channel = if (i == 0) main_player_channel else blk: {
                        // initialize ai channel
                        var client = try allocator.create(GameEngineClient);
                        try ai_clients.putNoClobber(individual.id, client);
                        break :blk try client.startAsAi(individual.id);
                    };
                    try decision_makers.putNoClobber(individual.id, channel);

                    // welcome
                    try channel.writeResponse(Response{ .static_perception = try game_engine.getStaticPerception(individual.id) });
                },
                else => unreachable,
            }
        }
    }
    const main_player_id: u32 = 1;

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
        {
            var iterator = decision_makers.iterator();
            while (iterator.next()) |kv| {
                var channel = kv.value;
                switch ((try channel.readRequest()) orelse {
                    std.debug.assert(kv.key == main_player_id);
                    core.debug.warn("clean shutdown. close");
                    channel.close();
                    break :mainLoop;
                }) {
                    Request.act => |action| {
                        std.debug.assert(game_engine.validateAction(action));
                        try actions.putNoClobber(kv.key, action);
                    },
                    Request.rewind => {
                        std.debug.assert(kv.key == main_player_id);
                        @panic("TODO");
                    },
                }
            }
        }

        const happenings = try game_engine.computeHappenings(actions);
        core.debug.deep_print("happenings: ", happenings);
        try game_engine.applyStateChanges(happenings.state_changes);
        try main_player_channel.writeResponse(Response{ .stuff_happens = happenings.individual_to_perception.get(main_player_id).?.value });
    }
}

fn getAiAction(human_relative_position: Coord) Action {
    // TODO: move this to another thread/process and communicate using a channel.
    const delta = human_relative_position; // shorter name
    std.debug.assert(!(delta.x == 0 and delta.y == 0));
    if (delta.x * delta.y == 0 and (delta.x + delta.y) * (delta.x + delta.y) == 1) {
        // orthogonally adjacent.
        return Action{ .attack = delta };
    }
    // move toward the target
    if (delta.x != 0) {
        // prioritize sideways movement
        return Action{ .move = Coord{ .x = core.geometry.sign(delta.x), .y = 0 } };
    } else {
        return Action{ .move = Coord{ .x = 0, .y = core.geometry.sign(delta.y) } };
    }
}
