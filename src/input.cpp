#include "input.hpp"

#include "display.hpp"
#include "swarkland.hpp"
#include "decision.hpp"

InputMode input_mode = InputMode_MAIN;
static uint256 chosen_item;
int inventory_cursor;

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
bool input_mode_is_choose_item() {
    switch (input_mode) {
        case InputMode_ZAP_CHOOSE_ITEM:
        case InputMode_DROP_CHOOSE_ITEM:
        case InputMode_THROW_CHOOSE_ITEM:
            return true;
        case InputMode_MAIN:
        case InputMode_THROW_CHOOSE_DIRECTION:
        case InputMode_ZAP_CHOOSE_DIRECTION:
            return false;
    }
    panic("input mode");
}
bool input_mode_is_choose_direction() {
    switch (input_mode) {
        case InputMode_THROW_CHOOSE_DIRECTION:
        case InputMode_ZAP_CHOOSE_DIRECTION:
            return true;
        case InputMode_MAIN:
        case InputMode_ZAP_CHOOSE_ITEM:
        case InputMode_DROP_CHOOSE_ITEM:
        case InputMode_THROW_CHOOSE_ITEM:
            return false;
    }
    panic("input mode");
}

static InputMode get_item_choosing_action_mode(const SDL_Event & event) {
    switch (event.key.keysym.sym) {
        case SDLK_d:
            return InputMode_DROP_CHOOSE_ITEM;
        case SDLK_t:
            return InputMode_THROW_CHOOSE_ITEM;
        case SDLK_z:
            return InputMode_ZAP_CHOOSE_ITEM;
        default:
            break;
    }
    panic("input mode key");
}

static Action move_or_attack(Coord direction) {
    Action action = Action::move(direction);
    // convert moving into attacking if it's pointed at an observed monster.
    if (find_perceived_individual_at(you, you->location + action.coord) != NULL)
        action.type = Action::ATTACK;
    return action;
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
            case SDLK_i:
                return Action::cheatcode_invisibility();
            case SDLK_g:
                return Action::cheatcode_generate_monster();
            case SDLK_w:
                return Action::cheatcode_create_item();

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
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_RIGHT:
            case SDL_SCANCODE_UP:
                return move_or_attack(get_direction_from_event(event));
            case SDL_SCANCODE_SPACE:
                return Action::wait();
            default:
                break;
        }
        switch (event.key.keysym.sym) {
            case SDLK_d:
            case SDLK_t:
            case SDLK_z: {
                List<Thing> inventory;
                find_items_in_inventory(you->id, &inventory);
                if (inventory.length() > 0) {
                    inventory_cursor = clamp(inventory_cursor, 0, inventory.length() - 1);
                    input_mode = get_item_choosing_action_mode(event);
                }
                return Action::undecided();
            }
            case SDLK_COMMA: {
                List<PerceivedThing> items;
                find_perceived_things_at(you, you->location, &items);
                if (items.length() == 0)
                    break;
                // grab the top one (or something)
                return Action::pickup(items[0]->id);
            }

            case SDLK_GREATER:
            case SDLK_PERIOD: // TODO: how do i check for > without looking at shift?
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

        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_UP: {
            // move the cursor
            List<Thing> inventory;
            find_items_in_inventory(you->id, &inventory);
            inventory_cursor = clamp(inventory_cursor + get_direction_from_event(event).y, 0, inventory.length() - 1);
            return Action::undecided();
        }

        default:
            break;
    }
    switch (event.key.keysym.sym) {
        case SDLK_d:
        case SDLK_t:
        case SDLK_z: {
            if (input_mode != get_item_choosing_action_mode(event))
                break;
            // doit
            List<Thing> inventory;
            find_items_in_inventory(you->id, &inventory);
            uint256 item_id = inventory[inventory_cursor]->id;
            switch (input_mode) {
                case InputMode_DROP_CHOOSE_ITEM:
                    input_mode = InputMode_MAIN;
                    return Action::drop(item_id);
                case InputMode_THROW_CHOOSE_ITEM:
                    chosen_item = item_id;
                    input_mode = InputMode_THROW_CHOOSE_DIRECTION;
                    return Action::undecided();
                case InputMode_ZAP_CHOOSE_ITEM:
                    chosen_item = item_id;
                    input_mode = InputMode_ZAP_CHOOSE_DIRECTION;
                    return Action::undecided();
                default:
                    break;
            }
            panic("input mode");
        }
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
            panic("input mode");
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
        case InputMode_DROP_CHOOSE_ITEM:
        case InputMode_THROW_CHOOSE_ITEM:
        case InputMode_ZAP_CHOOSE_ITEM:
            return on_key_down_choose_item(event);
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
