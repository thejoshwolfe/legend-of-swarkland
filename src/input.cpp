#include "input.hpp"

#include "display.hpp"
#include "swarkland.hpp"
#include "decision.hpp"
#include "serial.hpp"

InputMode input_mode = InputMode_MAIN;
int inventory_cursor;
static PerceivedThing chosen_item;
List<Action::Id> inventory_menu_items;
int inventory_menu_cursor;
int ability_cursor;
static AbilityId chosen_ability;
int floor_menu_cursor;
int cheatcode_polymorph_choose_species_menu_cursor;
int cheatcode_wish_choose_thing_type_menu_cursor;
int cheatcode_wish_choose_wand_id_menu_cursor;
int cheatcode_wish_choose_potion_id_menu_cursor;
int cheatcode_wish_choose_book_id_menu_cursor;
int cheatcode_generate_monster_choose_species_menu_cursor;
int cheatcode_generate_monster_choose_decision_maker_menu_cursor;
Coord cheatcode_generate_monster_choose_location_cursor = {map_size.x / 2, map_size.y / 2};

Action current_player_decision = Action::undecided();
bool request_shutdown = false;

static bool seen_the_mouse_for_realz = false;

Coord get_mouse_pixels() {
    Coord result;
    SDL_GetMouseState(&result.x, &result.y);
    if (!seen_the_mouse_for_realz) {
        // the mouse is reported at {0, 0} until we see it for realz,
        // so don't believe it until we're sure.
        if (result == Coord{0, 0})
            return Coord::nowhere();
        seen_the_mouse_for_realz = true;
    }
    return result;
}

void get_floor_actions(List<Action> * actions) {
    Thing actor = player_actor();
    PerceivedThing self = actor->life()->knowledge.perceived_things.get(actor->id);
    // pick up
    List<PerceivedThing> items;
    find_perceived_items_at(actor, self->location.coord, &items);
    for (int i = 0; i < items.length(); i++)
        actions->append(Action::pickup(items[i]->id));

    // go down
    if (actor->life()->knowledge.tiles[self->location.coord] == TileType_STAIRS_DOWN)
        actions->append(Action::go_down());
}

static Action move_or_attack(Coord direction) {
    // convert moving into attacking if it's pointed at an observed monster.
    if (find_perceived_individual_at(player_actor(), player_actor()->location.coord + direction) != nullptr)
        return Action::attack(direction);
    return Action::move(direction);
}
static Coord get_direction_from_event(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_Q:
            return {-1, -1};
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
            return {0, -1};
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_E:
            return {1, -1};
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_A:
            return {-1, 0};
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            return {0, 0};
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_D:
            return {1, 0};
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_Z:
            return {-1, 1};
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            return {0, 1};
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_C:
            return {1, 1};
        default:
            panic("invalid direcetion");
    }
}

static Action on_key_down_main(const SDL_Event & event) {
    if (event.key.keysym.mod & KMOD_CTRL) {
        // cheatcodes
        switch (event.key.keysym.sym) {
            case SDLK_v:
                cheatcode_full_visibility = !cheatcode_full_visibility;
                break;
            case SDLK_h:
                return Action::cheatcode_health_boost();
            case SDLK_k: {
                Thing individual = find_individual_at(get_mouse_tile(main_map_area));
                if (individual != nullptr)
                    return Action::cheatcode_kill(individual->id);
                return Action::undecided();
            }
            case SDLK_p:
                input_mode = InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES;
                break;
            case SDLK_s: {
                Coord location = get_mouse_tile(main_map_area);
                cheatcode_spectate(location);
                break;
            }
            case SDLK_g:
                input_mode = InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES;
                break;
            case SDLK_w:
                input_mode = InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE;
                break;
            case SDLK_o:
                return Action::cheatcode_identify();
            case SDLK_PERIOD:
                return Action::cheatcode_go_down();
            case SDLK_e:
                return Action::cheatcode_gain_level();

            default:
                break;
        }
    } else {
        // normal
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_KP_7:
            case SDL_SCANCODE_Q:
            case SDL_SCANCODE_KP_8:
            case SDL_SCANCODE_W:
            case SDL_SCANCODE_KP_9:
            case SDL_SCANCODE_E:
            case SDL_SCANCODE_KP_4:
            case SDL_SCANCODE_A:
            case SDL_SCANCODE_KP_6:
            case SDL_SCANCODE_D:
            case SDL_SCANCODE_KP_1:
            case SDL_SCANCODE_Z:
            case SDL_SCANCODE_KP_2:
            case SDL_SCANCODE_X:
            case SDL_SCANCODE_KP_3:
            case SDL_SCANCODE_C:
                return move_or_attack(get_direction_from_event(event));
            case SDL_SCANCODE_SPACE:
                return Action::wait();
            case SDL_SCANCODE_TAB: {
                List<PerceivedThing> inventory;
                find_items_in_inventory(player_actor(), player_actor()->id, &inventory);
                if (inventory.length() > 0) {
                    inventory_cursor = clamp(inventory_cursor, 0, inventory.length() - 1);
                    input_mode = InputMode_INVENTORY_CHOOSE_ITEM;
                }
                break;
            }
            case SDL_SCANCODE_R:
                // this might end up doing nothing
                start_auto_wait();
                return Action::auto_wait();
            case SDL_SCANCODE_ESCAPE:
                // this can cancel auto wait
                return Action::undecided();
            case SDL_SCANCODE_V: {
                List<AbilityId> abilities;
                get_abilities(player_actor(), &abilities);
                if (abilities.length() > 0) {
                    ability_cursor = clamp(ability_cursor, 0, abilities.length() - 1);
                    input_mode = InputMode_CHOOSE_ABILITY;
                }
                break;
            }
            case SDL_SCANCODE_KP_5:
            case SDL_SCANCODE_S: {
                List<Action> actions;
                get_floor_actions(&actions);
                if (actions.length() == 0)
                    break;
                if (actions.length() == 1)
                    return actions[0];
                // menu
                input_mode = InputMode_FLOOR_CHOOSE_ACTION;
                floor_menu_cursor = clamp(floor_menu_cursor, 0, actions.length() - 1);
                break;
            }

            default:
                break;
        }
    }
    return Action::undecided();
}
static Action on_key_down_replay(const SDL_Event & event) {
    if (event.key.keysym.mod & KMOD_CTRL) {
        // cheatcodes
        switch (event.key.keysym.sym) {
            case SDLK_v:
                cheatcode_full_visibility = !cheatcode_full_visibility;
                break;
            case SDLK_s: {
                Coord location = get_mouse_tile(main_map_area);
                cheatcode_spectate(location);
                break;
            }

            default:
                break;
        }
    } else {
        // normal
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_LEFT:
                if (!replay_paused) {
                    // slower
                    replay_adjust_delay(1);
                }
                break;
            case SDL_SCANCODE_RIGHT:
                if (replay_paused) {
                    replay_advance = true;
                } else {
                    // faster
                    replay_adjust_delay(-1);
                }
                break;
            case SDL_SCANCODE_UP:
                replay_resume();
                break;
            case SDL_SCANCODE_DOWN:
                replay_pause();
                break;

            default:
                break;
        }
    }
    return Action::undecided();
}
static Action on_key_down_choose_item(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_Q:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_E:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_C: {
            // move the cursor
            List<PerceivedThing> inventory;
            find_items_in_inventory(player_actor(), player_actor()->id, &inventory);
            Coord location = inventory_index_to_location(inventory_cursor) + get_direction_from_event(event);
            if (0 <= location.x && location.x < inventory_layout_width && 0 <= location.y) {
                int new_index = inventory_location_to_index(location);
                if (new_index < inventory.length())
                    inventory_cursor = new_index;
            }
            break;
        }
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S: {
            // accept
            List<PerceivedThing> inventory;
            find_items_in_inventory(player_actor(), player_actor()->id, &inventory);
            chosen_item = inventory[inventory_cursor];

            assert(inventory_menu_items.length() == 0);
            switch (chosen_item->thing_type) {
                case ThingType_INDIVIDUAL:
                    unreachable();
                case ThingType_WAND:
                    inventory_menu_items.append(Action::ZAP);
                    break;
                case ThingType_POTION:
                    inventory_menu_items.append(Action::QUAFF);
                    break;
                case ThingType_BOOK:
                    inventory_menu_items.append(Action::READ_BOOK);
                    break;

                case ThingType_COUNT:
                    unreachable();
            }
            inventory_menu_items.append(Action::THROW);
            inventory_menu_items.append(Action::DROP);
            inventory_menu_cursor = 0;

            input_mode = InputMode_INVENTORY_CHOOSE_ACTION;
            break;
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_choose_ability(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_Q:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_E:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_C: {
            // move the cursor
            List<AbilityId> abilities;
            get_abilities(player_actor(), &abilities);
            Coord location = inventory_index_to_location(ability_cursor) + get_direction_from_event(event);
            if (0 <= location.x && location.x < inventory_layout_width && 0 <= location.y) {
                int new_index = inventory_location_to_index(location);
                if (new_index < abilities.length())
                    ability_cursor = new_index;
            }
            break;
        }
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_V:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S: {
            // accept
            List<AbilityId> abilities;
            get_abilities(player_actor(), &abilities);
            chosen_ability = abilities[ability_cursor];
            if (is_ability_ready(player_actor(), chosen_ability)) {
                input_mode = InputMode_ABILITY_CHOOSE_DIRECTION;
            }
            break;
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_inventory_choose_action(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_INVENTORY_CHOOSE_ITEM;
            inventory_menu_items.clear();
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            inventory_menu_cursor = (inventory_menu_cursor + get_direction_from_event(event).y + inventory_menu_items.length()) % inventory_menu_items.length();
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            switch (inventory_menu_items[inventory_menu_cursor]) {
                case Action::DROP: {
                    uint256 id = chosen_item->id;
                    chosen_item = nullptr;
                    input_mode = InputMode_MAIN;
                    inventory_menu_items.clear();
                    return Action::drop(id);
                }
                case Action::QUAFF: {
                    uint256 id = chosen_item->id;
                    chosen_item = nullptr;
                    input_mode = InputMode_MAIN;
                    inventory_menu_items.clear();
                    return Action::quaff(id);
                }
                case Action::THROW:
                    input_mode = InputMode_THROW_CHOOSE_DIRECTION;
                    inventory_menu_items.clear();
                    break;
                case Action::ZAP:
                    input_mode = InputMode_ZAP_CHOOSE_DIRECTION;
                    inventory_menu_items.clear();
                    break;
                case Action::READ_BOOK:
                    input_mode = InputMode_READ_BOOK_CHOOSE_DIRECTION;
                    inventory_menu_items.clear();
                    break;

                case Action::WAIT:
                case Action::MOVE:
                case Action::ATTACK:
                case Action::PICKUP:
                case Action::GO_DOWN:
                case Action::ABILITY:
                case Action::CHEATCODE_HEALTH_BOOST:
                case Action::CHEATCODE_KILL:
                case Action::CHEATCODE_POLYMORPH:
                case Action::CHEATCODE_GENERATE_MONSTER:
                case Action::CHEATCODE_WISH:
                case Action::CHEATCODE_IDENTIFY:
                case Action::CHEATCODE_GO_DOWN:
                case Action::CHEATCODE_GAIN_LEVEL:
                case Action::COUNT:
                case Action::UNDECIDED:
                case Action::AUTO_WAIT:
                    unreachable();
            }
            break;

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_floor_choose_action(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X: {
            // move the cursor
            List<Action> actions;
            get_floor_actions(&actions);
            floor_menu_cursor = (floor_menu_cursor + get_direction_from_event(event).y + actions.length()) % actions.length();
            break;
        }
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S: {
            // accept
            List<Action> actions;
            get_floor_actions(&actions);
            input_mode = InputMode_MAIN;
            return actions[floor_menu_cursor];
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_polymorph_choose_species(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_polymorph_choose_species_menu_cursor = (cheatcode_polymorph_choose_species_menu_cursor + get_direction_from_event(event).y + SpeciesId_COUNT) % SpeciesId_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S: {
            // accept
            input_mode = InputMode_MAIN;
            SpeciesId species_id = (SpeciesId)cheatcode_polymorph_choose_species_menu_cursor;
            return Action::cheatcode_polymorph(species_id);
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_wish_choose_thing_type(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_wish_choose_thing_type_menu_cursor = (cheatcode_wish_choose_thing_type_menu_cursor + get_direction_from_event(event).y + (ThingType_COUNT - 1)) % (ThingType_COUNT - 1);
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            switch ((ThingType)(cheatcode_wish_choose_thing_type_menu_cursor + 1)) {
                case ThingType_INDIVIDUAL:
                    unreachable();
                case ThingType_WAND:
                    input_mode = InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID;
                    break;
                case ThingType_POTION:
                    input_mode = InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID;
                    break;
                case ThingType_BOOK:
                    input_mode = InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID;
                    break;

                case ThingType_COUNT:
                    unreachable();
            }
            break;

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_wish_choose_wand_id(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_wish_choose_wand_id_menu_cursor = (cheatcode_wish_choose_wand_id_menu_cursor + get_direction_from_event(event).y + WandId_COUNT) % WandId_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_MAIN;
            return Action::cheatcode_wish((WandId)cheatcode_wish_choose_wand_id_menu_cursor);

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_wish_choose_potion_id(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_wish_choose_potion_id_menu_cursor = (cheatcode_wish_choose_potion_id_menu_cursor + get_direction_from_event(event).y + PotionId_COUNT) % PotionId_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_MAIN;
            return Action::cheatcode_wish((PotionId)cheatcode_wish_choose_potion_id_menu_cursor);

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_wish_choose_book_id(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_wish_choose_book_id_menu_cursor = (cheatcode_wish_choose_book_id_menu_cursor + get_direction_from_event(event).y + BookId_COUNT) % BookId_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_MAIN;
            return Action::cheatcode_wish((BookId)cheatcode_wish_choose_book_id_menu_cursor);

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_generate_monster_choose_species(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_generate_monster_choose_species_menu_cursor = (cheatcode_generate_monster_choose_species_menu_cursor + get_direction_from_event(event).y + SpeciesId_COUNT) % SpeciesId_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER;
            break;

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_generate_monster_choose_decision_maker(const SDL_Event & event) {
    // this function name is pretty awesome. i wonder when i'll make a generic menu system...
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES;
            break;

        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
            // move the cursor
            cheatcode_generate_monster_choose_decision_maker_menu_cursor = (cheatcode_generate_monster_choose_decision_maker_menu_cursor + get_direction_from_event(event).y + DecisionMakerType_COUNT) % DecisionMakerType_COUNT;
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION;
            break;

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_cheatcode_generate_monster_choose_location(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER;
            break;

        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_Q:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_E:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_C: {
            // move the cursor
            Coord new_location = cheatcode_generate_monster_choose_location_cursor + get_direction_from_event(event);
            if (is_in_bounds(new_location))
                cheatcode_generate_monster_choose_location_cursor = new_location;
            break;
        }
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
            // accept
            input_mode = InputMode_MAIN;
            return Action::cheatcode_generate_monster(
                (SpeciesId)cheatcode_generate_monster_choose_species_menu_cursor,
                (DecisionMakerType)cheatcode_generate_monster_choose_decision_maker_menu_cursor,
                cheatcode_generate_monster_choose_location_cursor);

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_choose_direction(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_Q:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_E:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_C: {
            switch (input_mode) {
                case InputMode_THROW_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::throw_(chosen_item->id, get_direction_from_event(event));
                case InputMode_ZAP_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::zap(chosen_item->id, get_direction_from_event(event));
                case InputMode_READ_BOOK_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::read_book(chosen_item->id, get_direction_from_event(event));
                case InputMode_ABILITY_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::ability(chosen_ability, get_direction_from_event(event));
                case InputMode_MAIN:
                case InputMode_REPLAY:
                case InputMode_INVENTORY_CHOOSE_ITEM:
                case InputMode_CHOOSE_ABILITY:
                case InputMode_INVENTORY_CHOOSE_ACTION:
                case InputMode_FLOOR_CHOOSE_ACTION:
                case InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES:
                case InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE:
                case InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID:
                case InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID:
                case InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID:
                case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES:
                case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER:
                case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION:
                    unreachable();
            }
            unreachable();
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down(const SDL_Event & event) {
    switch (input_mode) {
        case InputMode_MAIN:
            return on_key_down_main(event);
        case InputMode_REPLAY:
            return on_key_down_replay(event);
        case InputMode_INVENTORY_CHOOSE_ITEM:
            return on_key_down_choose_item(event);
        case InputMode_CHOOSE_ABILITY:
            return on_key_down_choose_ability(event);
        case InputMode_INVENTORY_CHOOSE_ACTION:
            return on_key_down_inventory_choose_action(event);
        case InputMode_FLOOR_CHOOSE_ACTION:
            return on_key_down_floor_choose_action(event);
        case InputMode_THROW_CHOOSE_DIRECTION:
        case InputMode_ZAP_CHOOSE_DIRECTION:
        case InputMode_READ_BOOK_CHOOSE_DIRECTION:
        case InputMode_ABILITY_CHOOSE_DIRECTION:
            return on_key_down_choose_direction(event);

        case InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES:
            return on_key_down_cheatcode_polymorph_choose_species(event);
        case InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE:
            return on_key_down_cheatcode_wish_choose_thing_type(event);
        case InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID:
            return on_key_down_cheatcode_wish_choose_wand_id(event);
        case InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID:
            return on_key_down_cheatcode_wish_choose_potion_id(event);
        case InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID:
            return on_key_down_cheatcode_wish_choose_book_id(event);
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES:
            return on_key_down_cheatcode_generate_monster_choose_species(event);
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER:
            return on_key_down_cheatcode_generate_monster_choose_decision_maker(event);
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION:
            return on_key_down_cheatcode_generate_monster_choose_location(event);
    }
    return Action::undecided();
}

void read_input() {
    assert(!headless_mode);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                current_player_decision = on_key_down(event);
                break;
            case SDL_QUIT:
                request_shutdown = true;
                break;
        }
    }
}
