#include "input.hpp"

#include "display.hpp"
#include "swarkland.hpp"
#include "decision.hpp"

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

InputMode input_mode = InputMode_MAIN;
uint256 chosen_item;
int inventory_cursor;

static InputMode get_item_choosing_action_mode(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_D:
            return InputMode_DROP_CHOOSE_ITEM;
        case SDL_SCANCODE_T:
            return InputMode_THROW_CHOOSE_ITEM;
        case SDL_SCANCODE_Z:
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
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            request_shutdown = true;
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
        case SDL_SCANCODE_UP:
            return move_or_attack(get_direction_from_event(event));

        case SDL_SCANCODE_SPACE:
            return Action::wait();

        case SDL_SCANCODE_D:
        case SDL_SCANCODE_T:
        case SDL_SCANCODE_Z: {
            List<Thing> inventory;
            find_items_in_inventory(you, &inventory);
            if (inventory.length() > 0) {
                inventory_cursor = clamp(inventory_cursor, 0, inventory.length() - 1);
                input_mode = get_item_choosing_action_mode(event);
            }
            return Action::undecided();
        }
        case SDL_SCANCODE_COMMA: {
            List<PerceivedThing> items;
            find_perceived_things_at(you, you->location, &items);
            if (items.length() == 0)
                break;
            // grab the top one (or something)
            return Action::pickup(items[0]->id);
        }

        case SDL_SCANCODE_V:
            cheatcode_full_visibility = !cheatcode_full_visibility;
            break;
        case SDL_SCANCODE_H:
            return Action::cheatcode_health_boost();
        case SDL_SCANCODE_K:
            return Action::cheatcode_kill_everybody_in_the_world();
        case SDL_SCANCODE_P:
            return Action::cheatcode_polymorph();
        case SDL_SCANCODE_S:
            cheatcode_spectate();
            break;
        case SDL_SCANCODE_I:
            return Action::cheatcode_invisibility();
        case SDL_SCANCODE_G:
            return Action::cheatcode_generate_monster();

        default:
            break;
    }
    return Action::undecided();
}
static Action on_key_down_choose_item(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            input_mode = InputMode_MAIN;
            break;

        case SDL_SCANCODE_D:
        case SDL_SCANCODE_T:
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_RETURN: {
            if (input_mode != get_item_choosing_action_mode(event))
                break;
            // doit
            List<Thing> inventory;
            find_items_in_inventory(you, &inventory);
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

        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_KP_8:
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_UP: {
            // move the cursor
            List<Thing> inventory;
            find_items_in_inventory(you, &inventory);
            inventory_cursor = clamp(inventory_cursor + get_direction_from_event(event).y, 0, inventory.length() - 1);
            return Action::undecided();
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
            find_items_in_inventory(you, &inventory);
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
