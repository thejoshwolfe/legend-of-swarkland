#include "decision.hpp"

#include "path_finding.hpp"
#include "tas.hpp"

Action (*decision_makers[DecisionMakerType_COUNT])(Individual);
Action current_player_decision;

static Action get_player_decision(Individual) {
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

static Action get_ai_decision(Individual individual) {
    List<PerceivedIndividual> closest_hostiles;
    for (auto iterator = individual->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual target = iterator.next();
        if (target->team == individual->team)
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
        // let's go attack somebody!
        PerceivedIndividual target = closest_hostiles[random_int(closest_hostiles.length())];
        List<Coord> path;
        find_path(individual->location, target->location, individual, path);
        if (path.length() > 0) {
            Coord direction = path[0] - individual->location;
            if (path[0] == target->location)
                return {Action::ATTACK, direction};
            else
                return {Action::MOVE, direction};
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
