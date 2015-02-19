#include "decision.hpp"

#include "path_finding.hpp"
#include "tas.hpp"

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

static Action get_ai_decision(Thing individual) {
    List<PerceivedThing> closest_hostiles;
    PerceivedThing target;
    for (auto iterator = get_perceived_individuals(individual); iterator.next(&target);) {
        if (target->life().team == individual->life()->team)
            continue; // you're cool
        if (closest_hostiles.length() == 0) {
            // found somebody to punch
            closest_hostiles.append(target);
        } else {
            int beat_this_distance = distance_squared(individual->location, closest_hostiles[0]->location);
            int how_about_this = distance_squared(individual->location, target->location);
            if (how_about_this < beat_this_distance) {
                // this target is better
                closest_hostiles.clear();
                closest_hostiles.append(target);
            } else if (how_about_this == beat_this_distance) {
                // you're both so tempting. i'll decide randomly.
                closest_hostiles.append(target);
            } else {
                // too far away
                continue;
            }
        }
    }
    if (closest_hostiles.length() > 0) {
        // you want a piece a me?
        PerceivedThing target = closest_hostiles[random_int(closest_hostiles.length())];

        if (individual->life()->species()->uses_wands) {
            List<Thing> inventory;
            find_items_in_inventory(individual, &inventory);
            List<Thing> useful_inventory;
            for (int i = 0; i < inventory.length(); i++) {
                Thing item = inventory[i];
                WandId wand_id = individual->life()->knowledge.wand_identities[item->wand_info()->description_id];
                if (wand_id == WandId_WAND_OF_DIGGING)
                    continue; // don't dig the person.
                if (wand_id == WandId_WAND_OF_CONFUSION && target->status_effects.confused_timeout > 0)
                    continue; // already confused.
                // worth a try
                useful_inventory.append(item);
            }
            if (useful_inventory.length() > 0) {
                // should we zap it?
                Coord vector = target->location - individual->location;
                if (vector.x * vector.y == 0 || abs(vector.x) == abs(vector.y)) {
                    // you're in sight.
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
        find_path(individual->location, target->location, individual, &path);
        if (path.length() > 0) {
            Coord direction = path[0] - individual->location;
            if (path[0] == target->location)
                return Action::attack(direction);
            else
                return Action::move(direction);
        } else {
            // we must be stuck in a crowd
            return Action::wait();
        }
    } else {
        // idk what to do
        List<Action> actions;
        get_available_actions(individual, actions);
        List<Action> move_actions;
        for (int i = 0; i < actions.length(); i++)
            if (actions[i].type == Action::MOVE)
                move_actions.append(actions[i]);
        if (move_actions.length() > 0)
            return move_actions[random_int(move_actions.length())];
        // we must be stuck in a crowd
        return Action::wait();
    }
}

void init_decisions() {
    decision_makers[DecisionMakerType_PLAYER] = get_player_decision;
    decision_makers[DecisionMakerType_AI] = get_ai_decision;
}
