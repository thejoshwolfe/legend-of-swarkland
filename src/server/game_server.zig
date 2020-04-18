const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const sign = core.geometry.sign;
const GameEngine = @import("./game_engine.zig").GameEngine;
const game_model = @import("./game_model.zig");
const GameState = game_model.GameState;
const IdMap = game_model.IdMap;
const SomeQueues = @import("../client/game_engine_client.zig").SomeQueues;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const Species = core.protocol.Species;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const hasFastMove = core.game_logic.hasFastMove;
const isFastMoveAligned = core.game_logic.isFastMoveAligned;

const StateDiff = game_model.StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_queues: *SomeQueues) !void {
    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    var game_state = try GameState.generate(allocator);

    // create ai clients
    const main_player_id: u32 = 1;
    var you_are_alive = true;
    // Welcome to swarkland!
    try main_player_queues.enqueueResponse(Response{ .load_state = try game_engine.getStaticPerception(game_state, main_player_id) });

    var response_for_ais = IdMap(Response).init(allocator);
    var history = HistoryList.init();

    // start main loop
    mainLoop: while (true) {
        var actions = IdMap(Action).init(allocator);

        // do ai
        {
            var iterator = game_state.individuals.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                if (id == main_player_id) continue;
                const response = response_for_ais.getValue(id) orelse Response{ .load_state = try game_engine.getStaticPerception(game_state, id) };
                try actions.putNoClobber(id, doAi(response));
            }
        }
        response_for_ais.clear();

        // read all the inputs, which will block for the human client.
        var is_rewind = false;
        {
            retryRead: while (true) {
                switch (main_player_queues.waitAndTakeRequest() orelse {
                    core.debug.thread_lifecycle.print("clean shutdown. close", .{});
                    main_player_queues.closeResponses();
                    break :mainLoop;
                }) {
                    .act => |action| {
                        if (!you_are_alive) {
                            // no. you're are dead.
                            try main_player_queues.enqueueResponse(Response.reject_request);
                            continue :retryRead;
                        }
                        std.debug.assert(game_engine.validateAction(action));
                        try actions.putNoClobber(main_player_id, action);
                    },
                    .rewind => {
                        if (history.len == 0) {
                            // What do you want me to do, send you back to the main menu?
                            try main_player_queues.enqueueResponse(Response.reject_request);
                            continue :retryRead;
                        }
                        // delay actually rewinding so that we receive all requests.
                        is_rewind = true;
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
            try main_player_queues.enqueueResponse(Response{ .load_state = try game_engine.getStaticPerception(game_state, main_player_id) });
        } else {

            // Time goes forward.
            var scratch_game_state = try game_state.clone();
            const happenings = try game_engine.computeHappenings(&scratch_game_state, actions);
            core.debug.happening.deepPrint("happenings: ", happenings);
            try pushHistoryRecord(&history, happenings.state_changes);
            try game_state.applyStateChanges(happenings.state_changes);
            for (happenings.state_changes) |diff| {
                switch (diff) {
                    .despawn => |individual| {
                        if (individual.id == main_player_id) {
                            you_are_alive = false;
                        }
                    },
                    else => {},
                }
            }

            var iterator = happenings.individual_to_perception.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const response = Response{
                    .stuff_happens = PerceivedHappening{
                        .frames = kv.value,
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
    return getNaiveAiDecision(last_frame);
}

fn getNaiveAiDecision(last_frame: PerceivedFrame) Action {
    var target_position: ?Coord = null;
    var target_priority: i32 = -0x80000000;
    for (last_frame.others) |other| {
        const other_priority = getTargetHostilityPriority(last_frame.self.species, other.species) orelse continue;
        if (target_priority > other_priority) continue;
        if (other_priority > target_priority) {
            target_position = null;
            target_priority = other_priority;
        }
        const other_coord = getHeadPosition(other.rel_position);
        if (target_position) |previous_coord| {
            // target whichever is closest. (positions are relative to me.)
            if (other_coord.magnitudeOrtho() < previous_coord.magnitudeOrtho()) {
                target_position = other_coord;
            }
        } else {
            target_position = other_coord;
        }
    }
    if (target_position == null) {
        // nothing to do.
        return .wait;
    }

    const delta = target_position.?;
    std.debug.assert(!(delta.x == 0 and delta.y == 0));
    const range = core.game_logic.getAttackRange(last_frame.self.species);

    if (delta.x * delta.y == 0) {
        // straight shot
        const delta_unit = delta.signumed();
        if (hesitatesOneSpaceAway(last_frame.self.species) and delta.magnitudeOrtho() == 2) {
            // preemptive attack
            return Action{ .kick = delta_unit };
        }

        if (hasFastMove(last_frame.self.species) and isFastMoveAligned(last_frame.self.rel_position, delta_unit.scaled(2))) {
            // charge!
            return Action{ .fast_move = delta_unit.scaled(2) };
        } else if (delta.x * delta.x + delta.y * delta.y <= range * range) {
            if (hesitatesOneSpaceAway(last_frame.self.species)) {
                // just kick
                return Action{ .kick = delta_unit };
            }
            // within attack range
            return Action{ .attack = delta_unit };
        } else {
            // We want to get closer.
            if (last_frame.terrain.matrix.getCoord(delta_unit.minus(last_frame.terrain.rel_position))) |cell| {
                if (cell.floor == .lava) {
                    // I'm scared of lava
                    return .wait;
                }
            }
            // Move straight twoard the target, even if someone else is in the way
            return Action{ .move = delta_unit };
        }
    }
    // We have a diagonal space to traverse.
    // We need to choose between the long leg of the rectangle and the short leg.
    const options = [_]Coord{
        Coord{ .x = sign(delta.x), .y = 0 },
        Coord{ .x = 0, .y = sign(delta.y) },
    };
    const long_index: usize = blk: {
        if (delta.x * delta.x > delta.y * delta.y) {
            // x is longer
            break :blk 0;
        } else if (delta.x * delta.x < delta.y * delta.y) {
            // y is longer
            break :blk 1;
        } else {
            // exactly diagonal. let's say that clockwise is longer.
            break :blk @boolToInt(delta.x != delta.y);
        }
    };
    // Archers want to line up for a shot; melee wants to avoid lining up for a shot.
    var option_index = if (range == 1) long_index else 1 - long_index;
    // If something's in the way, then prefer the other way.
    // If something's in the way in both directions, then go with our initial preference.
    {
        var flip_flop_counter: usize = 0;
        flip_flop_loop: while (flip_flop_counter < 2) : (flip_flop_counter += 1) {
            const move_into_position = options[option_index];
            if (last_frame.terrain.matrix.getCoord(move_into_position.minus(last_frame.terrain.rel_position))) |cell| {
                if (cell.floor == .lava or !core.game_logic.isOpenSpace(cell.wall)) {
                    // Can't go that way.
                    option_index = 1 - option_index;
                    continue :flip_flop_loop;
                }
            }
            for (last_frame.others) |perceived_other| {
                for (getAllPositions(&perceived_other.rel_position)) |rel_position| {
                    if (rel_position.equals(move_into_position)) {
                        // somebody's there already.
                        option_index = 1 - option_index;
                        continue :flip_flop_loop;
                    }
                }
            }
        }
    }

    if (hesitatesOneSpaceAway(last_frame.self.species) and delta.magnitudeOrtho() == 2) {
        // preemptive attack
        return Action{ .kick = options[option_index] };
    } else {
        return Action{ .move = options[option_index] };
    }
}

fn hesitatesOneSpaceAway(species: Species) bool {
    switch (species) {
        .kangaroo => return true,
        else => return false,
    }
}

fn getTargetHostilityPriority(me: Species, you: Species) ?i32 {
    // this is straight up racism.
    if (me == you) return null;

    if (me == .human) {
        // humans, being the enlightened race, want to murder everything else equally.
        return 9;
    }
    switch (you) {
        // humans are just the worst.
        .human => return 9,
        // orcs are like humans.
        .orc => return 6,
        // half human.
        .centaur => return 5,
        // whatever.
        else => return 1,
    }
}
