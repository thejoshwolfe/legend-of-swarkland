const std = @import("std");
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const sign = core.geometry.sign;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;
const game_model = @import("./game_model.zig");
const Action = core.protocol.Action;
const Species = core.protocol.Species;
const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const ThingPosition = core.protocol.ThingPosition;
const getHeadPosition = core.game_logic.getHeadPosition;
const getAllPositions = core.game_logic.getAllPositions;
const canGrowAndShrink = core.game_logic.canGrowAndShrink;
const getPhysicsLayer = core.game_logic.getPhysicsLayer;
const isFastMoveAligned = core.game_logic.isFastMoveAligned;
const terrainAt = core.game_logic.terrainAt;

const StateDiff = game_model.StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn getNaiveAiDecision(last_frame: PerceivedFrame) Action {
    const me = last_frame.self;

    var target_position: ?Coord = null;
    var target_distance: ?i32 = null;
    var target_priority: i32 = -0x80000000;
    var target_species: ?Species = null;
    for (last_frame.others) |other| {
        const other_priority = getTargetHostilityPriority(me.species, other.species) orelse continue;
        if (target_priority > other_priority) continue;
        if (other_priority > target_priority) {
            target_position = null;
            target_distance = null;
            target_priority = other_priority;
            target_species = null;
        }
        for (getAllPositions(&other.position)) |other_coord| {
            const distance = distanceTo(other_coord, me);
            if (target_distance) |previous_distance| {
                // target whichever is closest.
                if (distance < previous_distance) {
                    target_position = other_coord;
                    target_distance = distance;
                    target_species = other.species;
                }
            } else {
                // First thing I see is the target so far.
                target_position = other_coord;
                target_distance = distance;
                target_species = other.species;
            }
        }
    }
    if (target_position == null) {
        // no targets.
        return .wait;
    }

    if (target_distance.? == 0) {
        // Overlapping the target.
        if (getPhysicsLayer(me.species) > getPhysicsLayer(target_species.?)) {
            if (can(me, .stomp)) |action| return action;
        } else {
            if (can(me, .nibble)) |action| return action;
        }
        if (canGrowAndShrink(me.species)) {
            switch (me.position) {
                .large => |data| {
                    if (target_position.?.equals(data[0])) {
                        return Action{ .shrink = 0 };
                    } else if (target_position.?.equals(data[1])) {
                        return Action{ .shrink = 1 };
                    } else unreachable;
                },
                .small => {
                    // eat
                    return .wait;
                },
            }
        }
        // Don't know how to handle this.
        core.debug.ai.print("waiting: overlapping target, but i'm not a blob.", .{});
        return .wait;
    }
    const my_head_position = getHeadPosition(me.position);
    const delta = target_position.?.minus(my_head_position);
    const range = if (hesitatesOneSpaceAway(me.species))
        1
    else
        core.game_logic.getAttackRange(me.species);

    if (delta.x * delta.y == 0) {
        // straight shot
        const delta_unit = delta.signumed();
        const delta_direction = deltaToCardinalDirection(delta_unit);
        if (hesitatesOneSpaceAway(me.species) and delta.magnitudeOrtho() == 2) {
            // preemptive attack
            if (can(me, Action{ .kick = delta_direction })) |action| return action;
            // should anyone do a preemptive actual attack?
        }

        if (isFastMoveAligned(me.position, delta_unit.scaled(2))) {
            // charge!
            if (can(me, .charge)) |action| return action;
        }
        if (delta.magnitudeOrtho() == 2) {
            // one lunge away
            if (can(me, Action{ .lunge = delta_direction })) |action| return action;
        }
        if (delta.euclideanDistanceSquared() <= range * range) {
            // within attack range
            if (hesitatesOneSpaceAway(me.species)) {
                // too close. get away!
                if (can(me, Action{ .kick = delta_direction })) |action| return action;
            }
            if (can(me, Action{ .attack = delta_direction })) |action| return action;
            // I'd love to do something about this situation, but there's nothing i can do.
            return .wait;
        } else {
            // We want to get closer.
            if (terrainAt(last_frame.terrain, my_head_position.plus(delta_unit))) |cell| {
                if (cell.floor == .lava) {
                    // I'm scared of lava
                    core.debug.ai.print("waiting: lava in my way", .{});
                    return .wait;
                }
            }
            // Move straight twoard the target, even if someone else is in the way
            return movelikeAction(me, delta_unit);
        }
    } else if (me.species == .blob and me.position == .large) {
        // Is this a straight shot from my butt?
        const butt_delta = target_position.?.minus(me.position.large[1]);
        if (butt_delta.x * butt_delta.y == 0) {
            // shrink back to approach.
            return Action{ .shrink = 1 };
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
            const move_into_position = my_head_position.plus(options[option_index]);
            if (terrainAt(last_frame.terrain, move_into_position)) |cell| {
                if (cell.floor == .lava or !core.game_logic.isOpenSpace(cell.wall)) {
                    // Can't go that way.
                    option_index = 1 - option_index;
                    continue :flip_flop_loop;
                }
            }
            for (last_frame.others) |perceived_other| {
                for (getAllPositions(&perceived_other.position)) |position| {
                    if (position.equals(move_into_position)) {
                        // somebody's there already.
                        option_index = 1 - option_index;
                        continue :flip_flop_loop;
                    }
                }
            }
        }
    }

    if (hesitatesOneSpaceAway(me.species) and delta.magnitudeOrtho() == 2) {
        // preemptive attack
        if (can(me, Action{ .kick = deltaToCardinalDirection(options[option_index]) })) |action| return action;
        // should anyone do a preemptive actual attack?
    }
    if (terrainAt(last_frame.terrain, my_head_position.plus(options[option_index]))) |cell| {
        if (cell.floor == .lava) {
            // On second thought, let's not try this.
            core.debug.ai.print("waiting: lava in my way (diagonal)", .{});
            return .wait;
        }
    }
    return movelikeAction(me, options[option_index]);
}

fn hesitatesOneSpaceAway(species: Species) bool {
    switch (species) {
        .kangaroo => return true,
        else => return false,
    }
}

const Faction = enum {
    humans,
    orcs,
    team_forest,
    team_desert,
    none,
};

fn getFaction(species: std.meta.Tag(core.protocol.Species)) Faction {
    return switch (species) {
        .human => .humans,
        .centaur => .team_forest,
        .turtle => .team_forest,
        .rhino => .team_forest,
        .kangaroo => .team_forest,
        .wood_golem => .team_forest,
        .orc => .orcs,
        .wolf => .orcs,
        .rat => .orcs,
        .blob => .none,
        .siren => .none,
        .scorpion => .team_desert,
        .brown_snake => .team_desert,
        .ant => .team_desert,
        .minotaur => .team_desert,
        .ogre => .team_desert,
    };
}

fn getTargetHostilityPriority(me: std.meta.Tag(core.protocol.Species), you: std.meta.Tag(core.protocol.Species)) ?i32 {
    const my_team = getFaction(me);
    const your_team = getFaction(you);
    if (my_team == your_team) return null;
    if (your_team == .humans) {
        // humans represent all that is evil and corrupt about the world.
        // most species are content to live in relative harmony with nature,
        // but humans can't resit uprooting trees, polluting the atmosphere,
        // and paving the rain forests with tar and concrete.
        // sure orcs are traditionally evil, and necro magicians may be literally evil,
        // but it's those humans and their ingenuity that are really to blame for all the problems in life.
        // just look at these pictures of sad animals amidst a bunch of trash.
        // doesn't that tug at your heartstrings? isn't that evidence enough for you?
        // humans are the them that you must hate. get em!
        return 9;
    }
    // whatever.
    return 1;
}

fn movelikeAction(me: PerceivedThing, delta: Coord) Action {
    const delta_direction = deltaToCardinalDirection(delta);
    if (can(me, Action{ .move = delta_direction })) |action| return action;

    if (canGrowAndShrink(me.species)) {
        switch (me.position) {
            .small => return Action{ .grow = delta_direction },
            .large => |positions| {
                // i shrink in your general direction
                const position_delta = positions[0].minus(positions[1]);
                if ((delta.x == 0) == (position_delta.x == 0)) {
                    // alignment is good
                    if (delta.equals(position_delta)) {
                        // foward
                        return Action{ .shrink = 0 };
                    } else {
                        // backward
                        return Action{ .shrink = 1 };
                    }
                } else {
                    // It's not clear which direction to shrink in this case.
                    return Action{ .shrink = 0 };
                }
            },
        }
    }
    // Can't move T_T
    return .wait;
}

fn distanceTo(coord: Coord, me: PerceivedThing) i32 {
    if (me.species == .blob and me.position == .large) {
        // measure distance from whichever of my coords is closer.
        const distance0 = coord.minus(me.position.large[0]).magnitudeOrtho();
        const distance1 = coord.minus(me.position.large[1]).magnitudeOrtho();
        return std.math.min(distance0, distance1);
    } else {
        // See with my head.
        return coord.minus(getHeadPosition(me.position)).magnitudeOrtho();
    }
}

fn can(me: PerceivedThing, action: Action) ?Action {
    core.game_logic.validateAction(me.species, me.position, me.status_conditions, action) catch |err| switch (err) {
        error.SpeciesIncapable, error.StatusForbids => return null,
        error.TooBig, error.TooSmall => unreachable,
    };
    return action;
}
