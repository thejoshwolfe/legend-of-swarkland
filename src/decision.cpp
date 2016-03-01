#include <serial.hpp>
#include "decision.hpp"

#include "path_finding.hpp"
#include "item.hpp"
#include "event.hpp"
#include "input.hpp"

static int start_waiting_event_count = -1;
static int previous_waiting_hp;
static int previous_waiting_mp;
void start_auto_wait() {
    assert_str(player_actor() == you(), "TODO: implement auto wait for multiple player actors");
    start_waiting_event_count = you()->life()->knowledge.remembered_events.length();
    previous_waiting_hp = you()->life()->hitpoints;
    previous_waiting_mp = you()->life()->mana;
}

void assess_auto_wait_situation(List<uint256> * output_scary_individuals, List<StatusEffect::Id> * output_annoying_status_effects, bool * output_stop_for_other_reasons) {
    Thing actor = player_actor();
    Life * life = actor->life();
    PerceivedThing self = life->knowledge.perceived_things.get(actor->id);
    // we're scared of people who can see us
    PerceivedThing target;
    for (auto iterator = life->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        if (target->thing_type != ThingType_INDIVIDUAL)
            continue;
        if (target == self)
            continue; // i'm not afraid of myself
        if (target->location.kind != Location::MAP)
            continue; // don't know where you are
        // even if we don't have normal vision,
        // if we think they're in a position where they can see us, we should be afraid.
        bool they_can_see_us = is_open_line_of_sight(target->location.coord, self->location.coord, life->knowledge.tiles);
        // also, if they've just come around a corner, and we have an advantageous position to see them first,
        // let's jump at that opportunity.
        bool we_can_see_them = is_open_line_of_sight(self->location.coord, target->location.coord, life->knowledge.tiles);
        if (they_can_see_us || we_can_see_them)
            output_scary_individuals->append(target->id);
    }
    if (life->hitpoints < previous_waiting_hp) {
        // probably poison.
        // let the user intervene whenever your hp drops.
        *output_stop_for_other_reasons = true;
    } else if (life->hitpoints > previous_waiting_hp) {
        // that's what i like to see
        if (life->hitpoints == actor->max_hitpoints()) {
            // this calls for a celebration.
            // even if there's more stuff going on, like blindness, break now.
            *output_stop_for_other_reasons = true;
        }
    }
    if (life->mana > previous_waiting_mp) {
        if (life->mana == actor->max_mana()) {
            *output_stop_for_other_reasons = true;
        }
    }

    if (life->knowledge.remembered_events.length() > start_waiting_event_count) {
        // wake up! something happened.
        *output_stop_for_other_reasons = true;
    }

    // is there any reason to wait any longer?
    if (life->hitpoints < actor->max_hitpoints())
        output_annoying_status_effects->append(StatusEffect::SPEED); // TODO: wrong. this should be more like "low hp" or something
    if (life->mana < actor->max_mana())
        output_annoying_status_effects->append(StatusEffect::SPEED); // TODO: wrong. this should be more like "low mp" or something
    if (has_status(self, StatusEffect::POISON))
        output_annoying_status_effects->append(StatusEffect::POISON);
    if (has_status(self, StatusEffect::CONFUSION))
        output_annoying_status_effects->append(StatusEffect::CONFUSION);
    if (has_status(self, StatusEffect::BLINDNESS))
        output_annoying_status_effects->append(StatusEffect::BLINDNESS);
    if (has_status(self, StatusEffect::SLOWING))
        output_annoying_status_effects->append(StatusEffect::SLOWING);
    // TODO: wait for polymorph
}
static int auto_wait_animation_index = 0;
Action get_player_decision(Thing actor) {
    game->player_actor_id = actor->id;
    Action action = read_decision_from_save_file();
    if (action.id == Action::UNDECIDED) {
        if (headless_mode) {
            // that's all folks
            return Action::undecided();
        }
        // ok, ask the real player
        action = current_player_decision;
        if (action.id == Action::AUTO_WAIT) {
            List<uint256> scary_individuals;
            List<StatusEffect::Id> annoying_status_effects;
            bool stop_for_other_reasons;
            assess_auto_wait_situation(&scary_individuals, &annoying_status_effects, &stop_for_other_reasons);
            if (scary_individuals.length() == 0 && annoying_status_effects.length() > 0 && !stop_for_other_reasons) {
                // auto-wait has decided
                auto_wait_animation_index = 1 - auto_wait_animation_index;
                if (auto_wait_animation_index == 0) {
                    // hold that thought. let's draw the screen first.
                    return Action::undecided();
                }
                action = Action::wait();
                record_decision_to_save_file(action);
                return action;
            }
            // auto-wait has finished
            action = Action::undecided();
        }
        current_player_decision = Action::undecided();

        if (action.id == Action::MOVE && !can_move(actor)) {
            // wait instead
            action = Action::wait();
        }

        if (action.id != Action::UNDECIDED && !validate_action(actor, action)) {
            // derp
            action = Action::undecided();
        }
    }
    if (action.id != Action::UNDECIDED)
        record_decision_to_save_file(action);
    return action;
}

static int rate_interest_in_target(PerceivedThing self, PerceivedThing target) {
    int score = ordinal_distance(self->location.coord, target->location.coord);
    // picking up items is less attractive if there are hostiles nearby
    if (target->thing_type != ThingType_INDIVIDUAL)
        score += 1;
    return score;
}

static const int confident_zap_distance = beam_length_average - beam_length_error_margin + 1;
static const int confident_throw_distance = throw_distance_average - throw_distance_error_margin;
static bool is_clear_projectile_shot(Thing actor, PerceivedThing target, int confident_distance) {
    int distnace = ordinal_distance(target->location.coord, actor->location.coord);
    if (distnace > confident_distance)
        return false; // out of range
    Coord vector = target->location.coord - actor->location.coord;
    Coord abs_vector = abs(vector);
    if (!(vector.x * vector.y == 0 || abs_vector.x == abs_vector.y))
        return false; // not a straight line
    Coord step = sign(vector);
    const MapMatrix<TileType> & tiles = actor->life()->knowledge.tiles;
    for (Coord cursor = actor->location.coord + step; cursor != target->location.coord; cursor += step)
        if (!is_open_space(tiles[cursor]))
            return false;
    return true;
}

static inline Action move_or_wait(Thing actor, Coord direction) {
    if (!can_move(actor))
        return Action::wait();
    return Action::move(direction);
}

Action get_ai_decision(Thing actor) {
    PerceivedThing self = actor->life()->knowledge.perceived_things.get(actor->id);
    bool uses_items = individual_uses_items(actor);
    bool advanced_strategy = individual_is_clever(actor);
    List<PerceivedThing> inventory;
    // only care about inventory if we use items
    if (uses_items)
        find_items_in_inventory(actor, actor->id, &inventory);
    List<AbilityId> abilities;
    get_abilities(actor, &abilities);

    List<PerceivedThing> things_of_interest;
    PerceivedThing target;
    for (auto iterator = actor->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        if (target->location.kind != Location::MAP)
            continue; // can't access you
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL:
                if (target->id == game->you_id)
                    break; // get him!
                if (target->is_placeholder)
                    break; // uh... get him?
                // you're cool
                continue;
            case ThingType_WAND:
                if (!uses_items)
                    continue; // don't care
                if (target->wand_info()->used_count == -1)
                    continue; // pshhh. dead wands are no use.
                break;
            case ThingType_POTION:
                if (!uses_items)
                    continue; // don't care
                break;
            case ThingType_BOOK:
                if (!advanced_strategy)
                    continue; // can't read
                break;

            case ThingType_COUNT:
                unreachable();
        }
        if (things_of_interest.length() == 0) {
            // found something to do
            things_of_interest.append(target);
        } else {
            int beat_this_distance = rate_interest_in_target(self, things_of_interest[0]);
            int how_about_this = rate_interest_in_target(self, target);
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
        // sort and then chose randomly. why?
        // because we don't want hashtable traversal order to be part of what makes a replay deterministic.
        sort<PerceivedThing, compare_perceived_things>(things_of_interest.raw(), things_of_interest.length());
        PerceivedThing target = things_of_interest[random_int(things_of_interest.length(), nullptr)];
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL: {
                // we are aggro!!

                // use items?
                List<Action> defense_actions;
                List<Action> buff_actions;
                List<Action> low_priority_buff_actions;
                List<Action> range_attack_actions;
                Coord vector = target->location.coord - actor->location.coord;
                Coord direction = sign(vector);
                for (int i = 0; i < inventory.length(); i++) {
                    PerceivedThing item = inventory[i];
                    switch (item->thing_type) {
                        case ThingType_INDIVIDUAL:
                            unreachable();
                        case ThingType_WAND:
                            if (item->wand_info()->used_count == -1)
                                break; // empty wand.
                            switch (actor->life()->knowledge.wand_identities[item->wand_info()->description_id]) {
                                case WandId_WAND_OF_CONFUSION:
                                    if (has_status(target, StatusEffect::CONFUSION))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_SLOWING:
                                    if (has_status(target, StatusEffect::SLOWING))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_BLINDING:
                                    if (has_status(target, StatusEffect::BLINDNESS))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_DIGGING:
                                case WandId_WAND_OF_FORCE:
                                    // idk how to use these well
                                    break;
                                case WandId_WAND_OF_MAGIC_MISSILE:
                                case WandId_WAND_OF_MAGIC_BULLET:
                                    if (is_clear_projectile_shot(actor, target, confident_zap_distance)) {
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
                                case WandId_WAND_OF_INVISIBILITY:
                                    if (!has_status(actor, StatusEffect::INVISIBILITY)) {
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
                                    if (is_clear_projectile_shot(actor, target, confident_zap_distance)) {
                                        // um. sure.
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_COUNT:
                                    unreachable();
                            }
                            break;
                        case ThingType_POTION:
                            switch (actor->life()->knowledge.potion_identities[item->potion_info()->description_id]) {
                                case PotionId_POTION_OF_HEALING:
                                    if (actor->life()->hitpoints < actor->max_hitpoints() / 2)
                                        defense_actions.append(Action::quaff(item->id));
                                    break;
                                case PotionId_POTION_OF_POISON:
                                    if (has_status(target, StatusEffect::POISON))
                                        break; // already poisoned
                                    if (!is_clear_projectile_shot(actor, target, confident_throw_distance))
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
                                        int information_age = (int)(game->time_counter - target->last_seen_time);
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
                                    if (target->is_placeholder) {
                                        // you can't hide
                                        buff_actions.append(Action::quaff(item->id));
                                    }
                                    break;
                                case PotionId_POTION_OF_BLINDNESS:
                                    if (has_status(target, StatusEffect::BLINDNESS))
                                        break; // already blind
                                    if (!is_clear_projectile_shot(actor, target, confident_throw_distance))
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
                        case ThingType_BOOK:
                            // TODO use books
                            break;

                        case ThingType_COUNT:
                            unreachable();
                    }
                }
                for (int i = 0; i < abilities.length(); i++){
                    AbilityId ability_id = abilities[i];
                    if (!is_ability_ready(actor, ability_id))
                        continue;
                    switch (ability_id) {
                        case AbilityId_SPIT_BLINDING_VENOM:
                            if (advanced_strategy && has_status(target, StatusEffect::BLINDNESS))
                                break; // already blind
                            if (!is_clear_projectile_shot(actor, target, confident_throw_distance))
                                break; // too far
                            range_attack_actions.append(Action::ability(ability_id, direction));
                            break;
                        case AbilityId_COUNT:
                            unreachable();
                    }
                }
                if (defense_actions.length() + buff_actions.length() + range_attack_actions.length() > 0) {
                    // should we go through with it?
                    if (advanced_strategy || random_int(3, nullptr) == 0) {
                        if (defense_actions.length())
                            return defense_actions[random_int(defense_actions.length(), nullptr)];
                        if (buff_actions.length())
                            return buff_actions[random_int(buff_actions.length(), nullptr)];
                        if (range_attack_actions.length())
                            return range_attack_actions[random_int(range_attack_actions.length(), nullptr)];
                    }
                    // nah. let's save the items for later.
                }

                // move/attack
                List<Coord> path;
                find_path(actor->location.coord, target->location.coord, actor, &path);
                if (path.length() > 0) {
                    Coord direction = path[0] - actor->location.coord;
                    if (path[0] == target->location.coord) {
                        return Action::attack(direction);
                    } else {
                        if (find_perceived_individual_at(actor, path[0]) != nullptr) {
                            // there's someone in the way
                            return Action::wait();
                        }
                        return move_or_wait(actor, direction);
                    }
                } else {
                    // we must be stuck in a crowd
                    return Action::wait();
                }
            }
            case ThingType_WAND:
            case ThingType_POTION:
            case ThingType_BOOK: {
                // gimme that
                if (target->location.coord == actor->location.coord)
                    return Action::position_item(target->id, InventorySlot_INSIDE);

                List<Coord> path;
                find_path(actor->location.coord, target->location.coord, actor, &path);
                if (path.length() > 0) {
                    Coord next_location = path[0];
                    if (do_i_think_i_can_move_here(actor, next_location)) {
                        Coord direction = next_location - actor->location.coord;
                        return move_or_wait(actor, direction);
                    }
                    // someone friendly is in the way, or something.
                    return Action::wait();
                } else {
                    // we must be stuck in a crowd
                    return Action::wait();
                }
            }

            case ThingType_COUNT:
                unreachable();
        }
    }

    // idk what to do
    if (!can_move(actor))
        return Action::wait();
    List<Coord> open_directions;
    for (int i = 0; i < 8; i++)
        if (do_i_think_i_can_move_here(actor, actor->location.coord + directions[i]))
            open_directions.append(directions[i]);
    if (open_directions.length() > 0)
        return Action::move(open_directions[random_int(open_directions.length(), nullptr)]);
    // we must be stuck in a crowd
    return Action::wait();
}
