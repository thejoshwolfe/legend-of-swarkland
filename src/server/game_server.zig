const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const GameEngine = @import("./game_engine.zig").GameEngine;
const GameState = @import("./game_engine.zig").GameState;
const Channel = core.protocol.Channel;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;

const allocator = std.heap.c_allocator;

pub fn server_main(player_channel: *Channel) !void {
    core.debug.nameThisThread("core");
    core.debug.warn("init");
    defer core.debug.warn("shutdown");

    var game_engine: GameEngine = undefined;
    try game_engine.init(allocator);
    // TODO: initialize ai threads

    try player_channel.writeResponse(Response{ .static_perception = try game_engine.getStaticPerception(0) });

    core.debug.warn("start main loop");
    mainLoop: while (true) {
        // input from player
        var player_action = switch ((try player_channel.readRequest()) orelse {
            core.debug.warn("clean shutdown. close");
            player_channel.close();
            break :mainLoop;
        }) {
            Request.act => |player_action| blk: {
                if (!game_engine.validateAction(player_action)) continue;
                break :blk player_action;
            },
            Request.rewind => {
                @panic("TODO");
            },
        };
        // normal action

        // get the rest of the decisions
        var actions = try allocator.alloc(Action, game_engine.game_state.individuals.len);
        defer allocator.free(actions);
        for (actions) |*action, i| {
            if (i == 0) {
                action.* = player_action;
            } else {
                const human_relative_position = game_engine.game_state.individuals[0].abs_position.minus(game_engine.game_state.individuals[i].abs_position);
                action.* = getAiAction(human_relative_position);
            }
        }

        const happenings = try game_engine.computeHappenings(actions[0..]);
        try game_engine.applyStateChanges(happenings.state_changes);
        try player_channel.writeResponse(Response{ .stuff_happens = happenings.individual_perception_frames[0] });
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
