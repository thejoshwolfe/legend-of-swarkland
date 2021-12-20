const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const sign = core.geometry.sign;
const game_model = @import("./game_model.zig");
const Action = core.protocol.Action;
const Species = core.protocol.Species;
const PerceivedFrame = core.protocol.PerceivedFrame;
const ThingPosition = core.protocol.ThingPosition;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const canCharge = core.game_logic.canCharge;
const canMoveNormally = core.game_logic.canMoveNormally;
const canGrowAndShrink = core.game_logic.canGrowAndShrink;
const isFastMoveAligned = core.game_logic.isFastMoveAligned;

const StateDiff = game_model.StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn getNaiveAiDecision(last_frame: PerceivedFrame) Action {
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

        if (canCharge(last_frame.self.species) and isFastMoveAligned(last_frame.self.rel_position, delta_unit.scaled(2))) {
            // charge!
            return Action{ .fast_move = delta_unit.scaled(2) };
        } else if (delta.x * delta.x + delta.y * delta.y <= range * range) {
            // within attack range
            if (hesitatesOneSpaceAway(last_frame.self.species)) {
                // get away from me!
                return Action{ .kick = delta_unit };
            }
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
            return movelikeAction(last_frame.self.species, last_frame.self.rel_position, delta_unit);
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
        if (last_frame.terrain.matrix.getCoord(options[option_index].minus(last_frame.terrain.rel_position))) |cell| {
            if (cell.floor == .lava) {
                // On second thought, let's not try this.
                return .wait;
            }
        }
        return movelikeAction(last_frame.self.species, last_frame.self.rel_position, options[option_index]);
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

fn movelikeAction(species: Species, position: ThingPosition, delta: Coord) Action {
    if (canMoveNormally(species)) {
        return Action{ .move = delta };
    } else if (canGrowAndShrink(species)) {
        switch (position) {
            .small => return Action{ .grow = delta },
            .large => |positions| {
                // i shrink in your general direction
                const position_delta = positions[0].minus(positions[1]);
                if ((delta.x == 0) == (position_delta.x == 0)) {
                    // alignment is good
                    return Action{ .shrink = delta };
                } else {
                    // It's not clear which direction to shrink in this case.
                    return Action{ .shrink = position_delta };
                }
            },
        }
    } else {
        // Can't move T_T
        return .wait;
    }
}
