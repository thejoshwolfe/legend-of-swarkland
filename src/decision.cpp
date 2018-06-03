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
    start_waiting_event_count = you()->life()->knowledge.perceived_events.length();
    previous_waiting_hp = you()->life()->hitpoints;
    previous_waiting_mp = you()->life()->mana;
}

void assess_auto_wait_situation(List<uint256> * output_scary_individuals, List<StatusEffect::Id> * output_annoying_status_effects, bool * output_stop_for_other_reasons) {
    *output_stop_for_other_reasons = false;
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
        Nullable<Coord> maybe_target_coord = get_maybe_standing_or_floor_coord(target->location);
        if (maybe_target_coord == nullptr)
            continue; // don't know where you are
        // even if we don't have normal vision,
        // if we think they're in a position where they can see us, we should be afraid.
        bool they_can_see_us = is_open_line_of_sight(*maybe_target_coord, actor->location.standing(), life->knowledge.tiles);
        // also, if they've just come around a corner, and we have an advantageous position to see them first,
        // let's jump at that opportunity.
        bool we_can_see_them = is_open_line_of_sight(actor->location.standing(), *maybe_target_coord, life->knowledge.tiles);
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

    if (life->knowledge.perceived_events.length() > start_waiting_event_count) {
        // wake up! something happened.
        *output_stop_for_other_reasons = true;
    }

    // is there any reason to wait any longer?
    if (life->hitpoints < actor->max_hitpoints())
        output_annoying_status_effects->append(StatusEffect::SPEED); // TODO: wrong. this should be more like "low hp" or something
    if (life->mana < actor->max_mana())
        output_annoying_status_effects->append(StatusEffect::SPEED); // TODO: wrong. this should be more like "low mp" or something
    if (has_status_apparently(self, StatusEffect::POISON))
        output_annoying_status_effects->append(StatusEffect::POISON);
    if (has_status_apparently(self, StatusEffect::CONFUSION))
        output_annoying_status_effects->append(StatusEffect::CONFUSION);
    if (has_status_apparently(self, StatusEffect::BLINDNESS))
        output_annoying_status_effects->append(StatusEffect::BLINDNESS);
    if (has_status_apparently(self, StatusEffect::SLOWING))
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

static int rate_interest_in_target(Thing actor, PerceivedThing target) {
    int score = ordinal_distance(actor->location.standing(), get_standing_or_floor_coord(target->location));
    // picking up items is less attractive if there are hostiles nearby
    if (target->thing_type != ThingType_INDIVIDUAL)
        score += 1;
    return score;
}

static int rate_polymorph_value(SpeciesId species_id) {
    if (species_id == SpeciesId_UNSEEN) {
        // guess that unseen things are level 3.
        return 3;
    } else if (species_id == SpeciesId_SHAPESHIFTER) {
        // never try to become a shapeshifter, and always shapeshift into something else.
        return -1;
    } else if (species_id == SpeciesId_HUMAN) {
        // they're better than they look
        return 3;
    } else {
        return specieses[species_id].min_level;
    }
}

static const int confident_zap_distance = beam_length_average - beam_length_error_margin + 1;
static bool is_clear_projectile_shot(Thing actor, Coord location, int confident_distance) {
    int distnace = ordinal_distance(location, actor->location.standing());
    if (distnace > confident_distance)
        return false; // out of range
    Coord vector = location - actor->location.standing();
    Coord abs_vector = abs(vector);
    if (!(vector.x * vector.y == 0 || abs_vector.x == abs_vector.y))
        return false; // not a straight line
    Coord step = sign(vector);
    const MapMatrix<TileType> & tiles = actor->life()->knowledge.tiles;
    for (Coord cursor = actor->location.standing() + step; cursor != location; cursor += step)
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
    bool uses_items = individual_uses_items(actor);
    bool advanced_strategy = individual_is_clever(actor);
    List<PerceivedThing> inventory;
    // only care about inventory if we use items
    if (uses_items)
        find_items_in_inventory(actor, actor->id, &inventory);
    List<AbilityId> abilities;
    get_abilities(actor, &abilities);

    List<PerceivedThing> things_of_interest;
    List<Action> high_priority_actions;
    PerceivedThing target;
    for (auto iterator = actor->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        if (target->id == actor->id)
            continue; // hi, that's myself.
        Nullable<Coord> maybe_target_coord = get_maybe_standing_or_floor_coord(target->location);
        if (maybe_target_coord == nullptr)
            continue; // can't access you
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL:
                for (int i = 0; i < abilities.length(); i++) {
                    AbilityId ability_id = abilities[i];
                    if (!is_ability_ready(actor, ability_id))
                        continue;
                    switch (ability_id) {
                        case AbilityId_ASSUME_FORM: {
                            if (!is_clear_projectile_shot(actor, *maybe_target_coord, infinite_range))
                                break; // not aligned
                            // do we want to change into this thing?
                            int current_value = rate_polymorph_value(actor->physical_species_id());
                            int new_value = rate_polymorph_value(target->life()->species_id);
                            if (new_value < current_value)
                                break; // don't downgrade
                            Coord direction = sign(*maybe_target_coord - actor->location.standing());
                            if (new_value > current_value) {
                                // always upgrade
                                high_priority_actions.append(Action::ability(ability_id, direction));
                            } else {
                                // sideways grade? maybe...
                                if (random_int(20, nullptr) == 0)
                                    high_priority_actions.append(Action::ability(ability_id, direction));
                            }
                            break;
                        }
                        case AbilityId_SPIT_BLINDING_VENOM:
                        case AbilityId_THROW_TAR:
                        case AbilityId_LUNGE_ATTACK:
                            // handled below
                            break;
                        case AbilityId_COUNT:
                            unreachable();
                    }
                }

                if (target->id == game->you_id)
                    break; // get him!
                if (target->is_placeholder)
                    break; // uh... get him?
                // ignore him
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
            case ThingType_WEAPON:
                if (!uses_items)
                    continue; // don't care
                break;

            case ThingType_COUNT:
                unreachable();
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

    if (high_priority_actions.length() > 0) {
        // don't even consider other things
        return high_priority_actions[random_int(high_priority_actions.length(), nullptr)];
    }

    if (things_of_interest.length() > 0) {
        // sort and then chose randomly. why?
        // because we don't want hashtable traversal order to be part of what makes a replay deterministic.
        sort<PerceivedThing, compare_perceived_things_by_id>(things_of_interest.raw(), things_of_interest.length());
        PerceivedThing target = things_of_interest[random_int(things_of_interest.length(), nullptr)];
        Coord target_coord = get_standing_or_floor_coord(target->location);
        switch (target->thing_type) {
            case ThingType_INDIVIDUAL: {
                // we are aggro!!

                // use items/abilities?
                List<Action> high_priority_range_attack_actions;
                List<Action> defense_actions;
                List<Action> buff_actions;
                List<Action> low_priority_buff_actions;
                List<Action> range_attack_actions;
                Coord vector = target_coord - actor->location.standing();
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
                                    if (has_status_apparently(target, StatusEffect::CONFUSION))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_SLOWING:
                                    if (has_status_apparently(target, StatusEffect::SLOWING))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_BLINDING:
                                    if (has_status_apparently(target, StatusEffect::BLINDNESS))
                                        break; // already confused.
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
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
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::zap(item->id, direction));
                                    }
                                    break;
                                case WandId_WAND_OF_SPEED:
                                    if (!has_status_effectively(actor, StatusEffect::SPEED)) {
                                        // gotta go fast!
                                        buff_actions.append(Action::zap(item->id, {0, 0}));
                                    }
                                    break;
                                case WandId_WAND_OF_INVISIBILITY:
                                    if (!has_status_effectively(actor, StatusEffect::INVISIBILITY)) {
                                        buff_actions.append(Action::zap(item->id, {0, 0}));
                                    }
                                    break;
                                case WandId_WAND_OF_REMEDY:
                                    if (has_status_effectively(actor, StatusEffect::CONFUSION) || has_status_effectively(actor, StatusEffect::POISON) || has_status_effectively(actor, StatusEffect::BLINDNESS)) {
                                        // sweet sweet soothing
                                        defense_actions.append(Action::zap(item->id, {0, 0}));
                                    }
                                    break;
                                case WandId_UNKNOWN:
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
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
                                    if (has_status_apparently(target, StatusEffect::POISON))
                                        break; // already poisoned
                                    if (!is_clear_projectile_shot(actor, target_coord, get_throw_range_window(item).x))
                                        break; // too far
                                    range_attack_actions.append(Action::throw_(item->id, direction));
                                    break;
                                case PotionId_POTION_OF_ETHEREAL_VISION:
                                    if (has_status_effectively(actor, StatusEffect::COGNISCOPY))
                                        break; // that's even better
                                    if (has_status_effectively(actor, StatusEffect::ETHEREAL_VISION))
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
                                    if (has_status_effectively(actor, StatusEffect::COGNISCOPY))
                                        break;
                                    // do we think she's gone invisible?
                                    if (target->is_placeholder) {
                                        // you can't hide
                                        buff_actions.append(Action::quaff(item->id));
                                    }
                                    break;
                                case PotionId_POTION_OF_BLINDNESS:
                                    if (has_status_apparently(target, StatusEffect::BLINDNESS))
                                        break; // already blind
                                    if (!is_clear_projectile_shot(actor, target_coord, get_throw_range_window(item).x))
                                        break; // too far
                                    range_attack_actions.append(Action::throw_(item->id, direction));
                                    break;
                                case PotionId_POTION_OF_INVISIBILITY:
                                    if (has_status_effectively(actor, StatusEffect::INVISIBILITY))
                                        break; // already invisible
                                    if (has_status_apparently(target, StatusEffect::COGNISCOPY) || has_status_apparently(target, StatusEffect::ETHEREAL_VISION))
                                        break; // no point
                                    // now you see me...
                                    buff_actions.append(Action::quaff(item->id));
                                    break;
                                case PotionId_POTION_OF_BURROWING:
                                case PotionId_POTION_OF_LEVITATION:
                                    // idk how to use this.
                                    break;
                                case PotionId_UNKNOWN:
                                    // nah. i'm afraid of unknown potions.
                                    break;
                                case PotionId_COUNT:
                                    unreachable();
                            }
                            break;
                        case ThingType_BOOK: {
                            if (!advanced_strategy)
                                break; // too dumb to use books
                            BookId book_id = actor->life()->knowledge.book_identities[item->book_info()->description_id];
                            int mana_cost = book_id == BookId_UNKNOWN ? 8 : get_mana_cost(book_id);
                            bool will_it_hurt_me = (mana_cost > actor->life()->mana);
                            if (will_it_hurt_me)
                                break; // not worth
                            switch (book_id) {
                                case BookId_SPELLBOOK_OF_MAGIC_BULLET:
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
                                        // get him!
                                        range_attack_actions.append(Action::read_book(item->id, direction));
                                    }
                                    break;
                                case BookId_SPELLBOOK_OF_SPEED:
                                    if (!has_status_effectively(actor, StatusEffect::SPEED)) {
                                        // gotta go fast!
                                        buff_actions.append(Action::read_book(item->id, {0, 0}));
                                    }
                                    break;
                                case BookId_UNKNOWN:
                                    if (is_clear_projectile_shot(actor, target_coord, confident_zap_distance)) {
                                        // um. sure.
                                        range_attack_actions.append(Action::read_book(item->id, direction));
                                    }
                                    break;
                                case BookId_SPELLBOOK_OF_FORCE:
                                case BookId_SPELLBOOK_OF_MAPPING:
                                case BookId_SPELLBOOK_OF_ASSUME_FORM:
                                    break; // not sure how to use yet
                                case BookId_COUNT:
                                    unreachable();
                            }
                            break;
                        }
                        case ThingType_WEAPON:
                            // We're not going to throw it. Just attack if it's equipped.
                            break;
                        case ThingType_COUNT:
                            unreachable();
                    }
                }
                for (int i = 0; i < abilities.length(); i++){
                    AbilityId ability_id = abilities[i];
                    switch (ability_id) {
                        case AbilityId_SPIT_BLINDING_VENOM:
                            if (!is_ability_ready(actor, ability_id))
                                break;
                            if (advanced_strategy && has_status_apparently(target, StatusEffect::BLINDNESS))
                                break; // already affected
                            if (!is_clear_projectile_shot(actor, target_coord, get_ability_range_window(ability_id).x))
                                break; // too far
                            range_attack_actions.append(Action::ability(ability_id, direction));
                            break;
                        case AbilityId_THROW_TAR:
                            if (!is_ability_ready(actor, ability_id))
                                break;
                            if (advanced_strategy && has_status_apparently(target, StatusEffect::SLOWING))
                                break; // already affected
                            if (!is_clear_projectile_shot(actor, target_coord, get_ability_range_window(ability_id).x))
                                break; // too far
                            range_attack_actions.append(Action::ability(ability_id, direction));
                            break;
                        case AbilityId_LUNGE_ATTACK: {
                            if (ordinal_distance(Coord{0, 0}, vector) <= 1)
                                break; // too close
                            Action wait_or_attack;
                            if (!is_ability_ready(actor, ability_id)) {
                                // we need to hold still to charge this ability
                                wait_or_attack = Action::wait();
                            } else if (!is_clear_projectile_shot(actor, target_coord, lunge_attack_range + 1)) {
                                // we'll bonk on something
                                wait_or_attack = Action::wait();
                            } else if (advanced_strategy && !is_safe_space(actor->life()->knowledge.tiles[target_coord - direction], is_touching_ground(actor))) {
                                // we'd fall into lava!
                                wait_or_attack = Action::wait();
                            } else {
                                // strike!
                                wait_or_attack = Action::ability(ability_id, direction);
                            }
                            high_priority_range_attack_actions.append(wait_or_attack);
                            break;
                        }
                        case AbilityId_ASSUME_FORM: {
                            // handled above
                            break;
                        }
                        case AbilityId_COUNT:
                            unreachable();
                    }
                }
                if (high_priority_range_attack_actions.length() + defense_actions.length() + buff_actions.length() + range_attack_actions.length() > 0) {
                    // should we go through with it?
                    if (high_priority_range_attack_actions.length() > 0 || advanced_strategy || random_int(3, nullptr) == 0) {
                        if (high_priority_range_attack_actions.length())
                            return high_priority_range_attack_actions[random_int(high_priority_range_attack_actions.length(), nullptr)];
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
                find_path(actor->location.standing(), target_coord, actor, &path);
                if (path.length() > 0) {
                    Coord direction = path[0] - actor->location.standing();
                    if (path[0] == target_coord) {
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
            case ThingType_BOOK:
            case ThingType_WEAPON: {
                // gimme that
                if (target_coord == actor->location.standing())
                    return Action::pickup(target->id);

                List<Coord> path;
                find_path(actor->location.standing(), target_coord, actor, &path);
                if (path.length() > 0) {
                    Coord next_location = path[0];
                    if (do_i_think_i_can_move_here(actor, next_location)) {
                        Coord direction = next_location - actor->location.standing();
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
        if (do_i_think_i_can_move_here(actor, actor->location.standing() + directions[i]))
            open_directions.append(directions[i]);
    if (open_directions.length() > 0)
        return Action::move(open_directions[random_int(open_directions.length(), nullptr)]);
    // we must be stuck in a crowd
    return Action::wait();
}
