#include "decision.hpp"

#include "path_finding.hpp"
#include "tas.hpp"
#include "item.hpp"
#include "event.hpp"

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

static int confident_zap_distance = beam_length_average - beam_length_error_margin + 1;
static int confident_throw_distance = throw_distance_average - throw_distance_error_margin;
static bool is_clear_line_of_sight(Thing actor, Coord location, int confident_distance) {
    int distnace = ordinal_distance(location, actor->location);
    if (distnace > confident_distance)
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
    const Species * actor_species = actor->life()->species();
    bool advanced_strategy = actor_species->advanced_strategy;

    List<PerceivedThing> things_of_interest;
    PerceivedThing target;
    for (auto iterator = actor->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL:
                if (target->life()->team == actor->life()->team)
                    continue; // you're cool
                break;
            case ThingType_WAND:
                if (!actor_species->uses_wands)
                    continue; // don't care
                if (target->location == Coord::nowhere())
                    continue; // somebody's already got it.
                break;
            case ThingType_POTION:
                if (!actor_species->uses_potions)
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

                // use items?
                List<Action> defense_actions;
                List<Action> buff_actions;
                List<Action> low_priority_buff_actions;
                List<Action> range_attack_actions;
                Coord vector = target->location - actor->location;
                Coord direction = sign(vector);
                for (int i = 0; i < inventory.length(); i++) {
                    Thing item = inventory[i];
                    switch (item->thing_type) {
                        case ThingType_INDIVIDUAL:
                            unreachable();
                        case ThingType_WAND:
                            if (!actor_species->uses_wands)
                                break;
                            switch (actor->life()->knowledge.wand_identities[item->wand_info()->description_id]) {
                                case WandId_WAND_OF_CONFUSION:
                                    if (has_status(target, StatusEffect::CONFUSION))
                                        break; // already confused.
                                    if (is_clear_line_of_sight(actor, target->location, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_DIGGING:
                                    // i don't understand digging.
                                    break;
                                case WandId_WAND_OF_STRIKING:
                                    if (is_clear_line_of_sight(actor, target->location, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_SPEED:
                                    if (!has_status(actor, StatusEffect::SPEED)) {
                                        // gotta go fast!
                                        buff_actions.append(Action::zap(item->id, {0, 0}));
                                    }
                                    break;
                                case WandId_WAND_OF_REMEDY:
                                    if (has_status(actor, StatusEffect::CONFUSION) || has_status(actor, StatusEffect::POISON) || has_status(actor, StatusEffect::BLINDNESS)) {
                                        // sweet sweet soothing
                                        defense_actions.append(Action::zap(item->id, {0, 0}));
                                    }
                                    break;
                                case WandId_UNKNOWN:
                                    if (is_clear_line_of_sight(actor, target->location, confident_zap_distance)) {
                                        // um. sure.
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_COUNT:
                                    unreachable();
                            }
                            break;
                        case ThingType_POTION:
                            if (!actor_species->uses_potions)
                                break;
                            switch (actor->life()->knowledge.potion_identities[item->potion_info()->description_id]) {
                                case PotionId_POTION_OF_HEALING:
                                    if (actor->life()->hitpoints < actor->life()->max_hitpoints() / 2)
                                        defense_actions.append(Action::quaff(item->id));
                                    break;
                                case PotionId_POTION_OF_POISON:
                                    if (has_status(target, StatusEffect::POISON))
                                        break; // already poisoned
                                    if (!is_clear_line_of_sight(actor, target->location, confident_throw_distance))
                                        break; // too far
                                    range_attack_actions.append(Action::throw_(item->id, direction));
                                    break;
                                case PotionId_POTION_OF_ETHEREAL_VISION:
                                    if (has_status(actor, StatusEffect::COGNISCOPY))
                                        break; // that's even better
                                    if (has_status(actor, StatusEffect::ETHEREAL_VISION))
                                        break;
                                    // do we think she's gone invisible?
                                    if (!can_see_thing(actor, target->id)) {
                                        // we've gotta be close
                                        int information_age = (int)(time_counter - target->last_seen_time);
                                        int max_steps = information_age / 12;
                                        int effective_radius = ethereal_radius - max_steps;
                                        if (effective_radius <= 0 || euclidean_distance_squared(Coord{0, 0}, vector) > effective_radius * effective_radius)
                                            break; // probably not there anymore
                                        // i have you now
                                        low_priority_buff_actions.append(Action::quaff(item->id));
                                    }
                                    break;
                                case PotionId_POTION_OF_COGNISCOPY:
                                    if (has_status(actor, StatusEffect::COGNISCOPY))
                                        break;
                                    // do we think she's gone invisible?
                                    if (target->life()->species_id == SpeciesId_UNSEEN) {
                                        // you can't hide
                                        buff_actions.append(Action::quaff(item->id));
                                    }
                                    break;
                                case PotionId_POTION_OF_BLINDNESS:
                                    if (has_status(target, StatusEffect::BLINDNESS))
                                        break; // already blind
                                    if (!is_clear_line_of_sight(actor, target->location, confident_throw_distance))
                                        break; // too far
                                    range_attack_actions.append(Action::throw_(item->id, direction));
                                    break;
                                case PotionId_POTION_OF_INVISIBILITY:
                                    if (has_status(actor, StatusEffect::INVISIBILITY))
                                        break; // already invisible
                                    if (has_status(target, StatusEffect::COGNISCOPY) || has_status(target, StatusEffect::ETHEREAL_VISION))
                                        break; // no point
                                    // now you see me...
                                    buff_actions.append(Action::quaff(item->id));
                                    break;
                                case PotionId_UNKNOWN:
                                    // nah. i'm afraid of unknown potions.
                                    break;
                                case PotionId_COUNT:
                                    unreachable();
                            }
                            break;
                    }
                }
                if (defense_actions.length() + buff_actions.length() + range_attack_actions.length() > 0) {
                    // should we go through with it?
                    if (advanced_strategy || random_int(3) == 0) {
                        if (defense_actions.length())
                            return defense_actions[random_int(defense_actions.length())];
                        if (buff_actions.length())
                            return buff_actions[random_int(buff_actions.length())];
                        if (range_attack_actions.length())
                            return range_attack_actions[random_int(range_attack_actions.length())];
                    }
                    // nah. let's save the items for later.
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
