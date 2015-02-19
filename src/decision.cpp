#include "decision.hpp"

#include "path_finding.hpp"
#include "tas.hpp"
#include "item.hpp"

Action (*decision_makers[DecisionMakerType_COUNT])(Thing);
Action current_player_decision;

static Action get_player_decision(Thing) {
    Action result = tas_get_decision();
    if (result == Action::undecided()) {
        // ok, ask the real player
        result = current_player_decision;
        current_player_decision = Action::undecided();
    }
    if (result != Action::undecided())
        tas_record_decision(result);
    return result;
}

static int rate_interest_in_target(Thing actor, PerceivedThing target) {
    int score = ordinal_distance(actor->location, target->location);
    // picking up items is less attractive if there are hostiles nearby
    if (target->thing_type == ThingType_WAND)
        score += 1;
    return score;
}

static Action get_ai_decision(Thing actor) {
    List<PerceivedThing> things_of_interest;
    PerceivedThing target;
    for (auto iterator = actor->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL:
                if (target->life().team == actor->life()->team)
                    continue; // you're cool
                break;
            case ThingType_WAND:
                if (!actor->life()->species()->uses_wands)
                    continue; // don't care
                if (target->location == Coord::nowhere())
                    continue; // somebody's already got it.
                break;
        }
        if (things_of_interest.length() == 0) {
            // found something to do
            things_of_interest.append(target);
        } else {
            int beat_this_distance = rate_interest_in_target(actor, things_of_interest[0]);
            int how_about_this = rate_interest_in_target(actor, target);
            if (how_about_this < beat_this_distance) {
                // this target is better
                things_of_interest.clear();
                things_of_interest.append(target);
            } else if (how_about_this == beat_this_distance) {
                // you're both so tempting. i'll decide randomly.
                things_of_interest.append(target);
            } else {
                // too far away
                continue;
            }
        }
    }

    if (things_of_interest.length() > 0) {
        PerceivedThing target = things_of_interest[random_int(things_of_interest.length())];

        if (target->thing_type == ThingType_INDIVIDUAL) {
            // you want a piece a me?
            if (actor->life()->species()->uses_wands) {
                Coord vector = target->location - actor->location;
                Coord abs_vector = abs(vector);
                int distnace = ordinal_distance(target->location, actor->location);
                if ((vector.x * vector.y == 0 || abs_vector.x == abs_vector.y) && distnace <= beam_length_average) {
                    // you're in sight.
                    List<Thing> inventory;
                    find_items_in_inventory(actor, &inventory);
                    List<Thing> useful_inventory;
                    for (int i = 0; i < inventory.length(); i++) {
                        Thing item = inventory[i];
                        WandId wand_id = actor->life()->knowledge.wand_identities[item->wand_info()->description_id];
                        if (wand_id == WandId_WAND_OF_DIGGING)
                            continue; // don't dig the person.
                        if (wand_id == WandId_WAND_OF_CONFUSION && target->status_effects.confused_timeout > 0)
                            continue; // already confused.
                        // worth a try
                        useful_inventory.append(item);
                    }
                    if (useful_inventory.length() > 0) {
                        // should we zap it?
                        if (random_int(3) == 0) {
                            // get him!!
                            Coord direction = sign(vector);
                            return Action::zap(useful_inventory[random_int(useful_inventory.length())]->id, direction);
                        }
                        // nah. let's save the charges.
                    }
                }
            }

            List<Coord> path;
            find_path(actor->location, target->location, actor, &path);
            if (path.length() > 0) {
                Coord direction = path[0] - actor->location;
                if (path[0] == target->location)
                    return Action::attack(direction);
                else
                    return Action::move(direction);
            } else {
                // we must be stuck in a crowd
                return Action::wait();
            }
        } else if (target->thing_type == ThingType_WAND) {
            // gimme that
            if (target->location == actor->location)
                return Action::pickup(target->id);

            List<Coord> path;
            find_path(actor->location, target->location, actor, &path);
            if (path.length() > 0) {
                Coord next_location = path[0];
                if (do_i_think_i_can_move_here(actor, next_location)) {
                    Coord direction = next_location - actor->location;
                    return Action::move(direction);
                }
                // someone friendly is in the way, or something.
            } else {
                // we must be stuck in a crowd
                return Action::wait();
            }
        } else {
            panic("thing type");
        }
    }

    // idk what to do
    List<Action> actions;
    get_available_actions(actor, actions);
    List<Action> move_actions;
    for (int i = 0; i < actions.length(); i++)
        if (actions[i].type == Action::MOVE)
            move_actions.append(actions[i]);
    if (move_actions.length() > 0)
        return move_actions[random_int(move_actions.length())];
    // we must be stuck in a crowd
    return Action::wait();
}

void init_decisions() {
    decision_makers[DecisionMakerType_PLAYER] = get_player_decision;
    decision_makers[DecisionMakerType_AI] = get_ai_decision;
}
