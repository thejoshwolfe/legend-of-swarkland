#include "input.hpp"

#include "display.hpp"
#include "swarkland.hpp"
#include "decision.hpp"

InputMode input_mode = InputMode_MAIN;
int inventory_cursor;
Thing chosen_item;
List<Action::Id> menu_items;
int menu_cursor;

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

static Action move_or_attack(Coord direction) {
    // convert moving into attacking if it's pointed at an observed monster.
    if (find_perceived_individual_at(you, you->location + direction) != nullptr)
        return Action::attack(direction);
    return Action::move(direction);
}
static Coord get_direction_from_event(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_KP_1:
            return {-1, 1};
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2:
            return {0, 1};
        case SDL_SCANCODE_KP_3:
            return {1, 1};
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4:
            return {-1, 0};
        case SDL_SCANCODE_KP_5:
            return {0, 0};
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6:
            return {1, 0};
        case SDL_SCANCODE_KP_7:
            return {-1, -1};
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8:
            return {0, -1};
        case SDL_SCANCODE_KP_9:
            return {1, -1};
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
            case SDLK_k:
                return Action::cheatcode_kill_everybody_in_the_world();
            case SDLK_p:
                return Action::cheatcode_polymorph();
            case SDLK_s:
                cheatcode_spectate();
                break;
            case SDLK_g:
                return Action::cheatcode_generate_monster();
            case SDLK_w:
                return Action::cheatcode_create_item();
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
            case SDL_SCANCODE_KP_1:
            case SDL_SCANCODE_KP_2:
            case SDL_SCANCODE_KP_3:
            case SDL_SCANCODE_KP_4:
            case SDL_SCANCODE_KP_6:
            case SDL_SCANCODE_KP_7:
            case SDL_SCANCODE_KP_8:
            case SDL_SCANCODE_KP_9:
                return move_or_attack(get_direction_from_event(event));
            case SDL_SCANCODE_SPACE:
                return Action::wait();
            case SDL_SCANCODE_TAB: {
                List<Thing> inventory;
                find_items_in_inventory(you->id, &inventory);
                if (inventory.length() > 0) {
                    inventory_cursor = clamp(inventory_cursor, 0, inventory.length() - 1);
                    input_mode = InputMode_INVENTORY_CHOOSE_ITEM;
                }
                return Action::undecided();
            }
            case SDL_SCANCODE_COMMA: {
                // TODO: do this with numpad 5
                List<PerceivedThing> items;
                find_perceived_items_at(you, you->location, &items);
                if (items.length() == 0)
                    break;
                // grab the top one (or something)
                return Action::pickup(items[0]->id);
            }

            case SDL_SCANCODE_PERIOD:
                // TODO: do this with numpad 5
                if (actual_map_tiles[you->location].tile_type != TileType_STAIRS_DOWN)
                    break;
                return Action::go_down();

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

        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_KP_9: {
            // move the cursor
            List<Thing> inventory;
            find_items_in_inventory(you->id, &inventory);
            Coord location = inventory_index_to_location(inventory_cursor) + get_direction_from_event(event);
            if (0 <= location.x && location.x < inventory_layout_width && 0 <= location.y) {
                int new_index = inventory_location_to_index(location);
                if (new_index < inventory.length())
                    inventory_cursor = new_index;
            }
            break;
        }
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5: {
            // accept
            List<Thing> inventory;
            find_items_in_inventory(you->id, &inventory);
            chosen_item = inventory[inventory_cursor];

            assert(menu_items.length() == 0);
            switch (chosen_item->thing_type) {
                case ThingType_INDIVIDUAL:
                    unreachable();
                case ThingType_WAND:
                    menu_items.append(Action::ZAP);
                    break;
                case ThingType_POTION:
                    menu_items.append(Action::QUAFF);
                    break;
            }
            menu_items.append(Action::DROP);
            menu_items.append(Action::THROW);
            menu_cursor = 0;

            input_mode = InputMode_INVENTORY_CHOOSE_ACTION;
            break;
        }

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_choose_action(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_INVENTORY_CHOOSE_ITEM;
            menu_items.clear();
            break;

        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_KP_8:
            // move the cursor
            menu_cursor = (menu_cursor + get_direction_from_event(event).y + menu_items.length()) % menu_items.length();
            break;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_KP_5:
            // accept
            switch (menu_items[menu_cursor]) {
                case Action::DROP: {
                    uint256 id = chosen_item->id;
                    chosen_item = nullptr;
                    input_mode = InputMode_MAIN;
                    menu_items.clear();
                    return Action::drop(id);
                }
                case Action::QUAFF: {
                    uint256 id = chosen_item->id;
                    chosen_item = nullptr;
                    input_mode = InputMode_MAIN;
                    menu_items.clear();
                    return Action::quaff(id);
                }
                case Action::THROW:
                    input_mode = InputMode_THROW_CHOOSE_DIRECTION;
                    menu_items.clear();
                    break;
                case Action::ZAP:
                    input_mode = InputMode_ZAP_CHOOSE_DIRECTION;
                    menu_items.clear();
                    break;

                case Action::WAIT:
                case Action::MOVE:
                case Action::ATTACK:
                case Action::PICKUP:
                case Action::GO_DOWN:
                case Action::CHEATCODE_HEALTH_BOOST:
                case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
                case Action::CHEATCODE_POLYMORPH:
                case Action::CHEATCODE_GENERATE_MONSTER:
                case Action::CHEATCODE_CREATE_ITEM:
                case Action::CHEATCODE_IDENTIFY:
                case Action::CHEATCODE_GO_DOWN:
                case Action::CHEATCODE_GAIN_LEVEL:
                case Action::COUNT:
                case Action::UNDECIDED:
                    unreachable();
            }
            break;

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

        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_KP_9:
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_UP: {
            List<Thing> inventory;
            find_items_in_inventory(you->id, &inventory);
            uint256 item_id = inventory[inventory_cursor]->id;
            switch (input_mode) {
                case InputMode_THROW_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::throw_(item_id, get_direction_from_event(event));
                case InputMode_ZAP_CHOOSE_DIRECTION:
                    input_mode = InputMode_MAIN;
                    return Action::zap(item_id, get_direction_from_event(event));
                default:
                    break;
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
        case InputMode_INVENTORY_CHOOSE_ITEM:
            return on_key_down_choose_item(event);
        case InputMode_INVENTORY_CHOOSE_ACTION:
            return on_key_down_choose_action(event);
        case InputMode_THROW_CHOOSE_DIRECTION:
        case InputMode_ZAP_CHOOSE_DIRECTION:
            return on_key_down_choose_direction(event);
    }
    return Action::undecided();
}

void read_input() {
    SDL_Event event;
    Action action = Action::undecided();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                action = on_key_down(event);
                break;
            case SDL_QUIT:
                request_shutdown = true;
                break;
        }
    }
    current_player_decision = action;
}
