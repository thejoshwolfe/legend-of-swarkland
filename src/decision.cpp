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
    if (target->thing_type != ThingType_INDIVIDUAL)
        score += 1;
    return score;
}

static bool is_clear_line_of_sight(Thing actor, Coord location) {
    int distnace = ordinal_distance(location, actor->location);
    if (distnace > beam_length_average)
        return false; // out of range
    Coord vector = location - actor->location;
    Coord abs_vector = abs(vector);
    if (!(vector.x * vector.y == 0 || abs_vector.x == abs_vector.y))
        return false; // not a straight line
    Coord step = sign(vector);
    MapMatrix<Tile> & tiles = actor->life()->knowledge.tiles;
    for (Coord cursor = actor->location + step; cursor != location; cursor += step)
        if (!is_open_space(tiles[cursor].tile_type))
            return false;
    return true;
}

static Action get_ai_decision(Thing actor) {
    List<Thing> inventory;
    find_items_in_inventory(actor->id, &inventory);

    if (actor->life()->species()->advanced_strategy) {
        // defense first
        if (has_status(actor, StatusEffect::CONFUSION) || has_status(actor, StatusEffect::POISON) || has_status(actor, StatusEffect::BLINDNESS)) {
            // can we remedy?
            for (int i = 0; i < inventory.length(); i++) {
                if (inventory[i]->thing_type != ThingType_WAND)
                    continue;
                WandId wand_id = actor->life()->knowledge.wand_identities[inventory[i]->wand_info()->description_id];
                if (wand_id != WandId_WAND_OF_REMEDY)
                    continue;
                // sweet sweet soothing
                return Action::zap(inventory[i]->id, {0, 0});
            }
            // no remedy. oh well.
        }
    }

    List<PerceivedThing> things_of_interest;
    PerceivedThing target;
    for (auto iterator = actor->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL:
                if (target->life()->team == actor->life()->team)
                    continue; // you're cool
                break;
            case ThingType_WAND:
            case ThingType_POTION:
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
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL: {
                // we are aggro!!
                if (actor->life()->species()->advanced_strategy) {
                    if (!has_status(actor, StatusEffect::SPEED)) {
                        // use speed boost if we can
                        for (int i = 0; i < inventory.length(); i++) {
                            if (inventory[i]->thing_type != ThingType_WAND)
                                continue;
                            WandId wand_id = actor->life()->knowledge.wand_identities[inventory[i]->wand_info()->description_id];
                            if (wand_id != WandId_WAND_OF_SPEED)
                                continue;
                            // gotta go fast!
                            return Action::zap(inventory[i]->id, {0, 0});
                        }
                    }
                }

                // zap him?
                if (actor->life()->species()->uses_wands) {
                    if (is_clear_line_of_sight(actor, target->location)) {
                        Coord vector = target->location - actor->location;
                        // you're in line, but is there a clear path?
                        List<Thing> useful_inventory;
                        for (int i = 0; i < inventory.length(); i++) {
                            Thing item = inventory[i];
                            if (item->thing_type != ThingType_WAND)
                                continue;
                            WandId wand_id = actor->life()->knowledge.wand_identities[item->wand_info()->description_id];
                            if (wand_id == WandId_WAND_OF_DIGGING)
                                continue; // don't dig the person.
                            if (wand_id == WandId_WAND_OF_SPEED || wand_id == WandId_WAND_OF_REMEDY)
                                continue; // you'd like that, wouldn't you.
                            if (wand_id == WandId_WAND_OF_CONFUSION && has_status(target, StatusEffect::CONFUSION))
                                continue; // already confused.
                            // worth a try
                            useful_inventory.append(item);
                        }
                        if (useful_inventory.length() > 0) {
                            // should we zap it?
                            if (actor->life()->species()->advanced_strategy || random_int(3) == 0) {
                                // get him!!
                                Coord direction = sign(vector);
                                return Action::zap(useful_inventory[random_int(useful_inventory.length())]->id, direction);
                            }
                            // nah. let's save the charges.
                        }
                    }
                }

                // move/attack
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
            }
            case ThingType_POTION:
            case ThingType_WAND: {
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
            }
        }
    }

    // idk what to do
    List<Action> actions;
    get_available_moves(&actions);
    List<Action> move_actions;
    for (int i = 0; i < actions.length(); i++)
        if (do_i_think_i_can_move_here(actor, actor->location + actions[i].coord))
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
