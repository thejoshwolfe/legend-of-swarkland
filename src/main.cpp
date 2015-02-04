#include "swarkland.hpp"
#include "display.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "util.hpp"
#include "geometry.hpp"

#include <stdbool.h>

#include <SDL2/SDL.h>

static bool request_shutdown = false;

static Action on_key_down(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            request_shutdown = true;
            break;

        case SDL_SCANCODE_KP_1:
            return {Action::MOVE, {-1, 1}};
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2:
            return {Action::MOVE, {0, 1}};
        case SDL_SCANCODE_KP_3:
            return {Action::MOVE, {1, 1}};
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4:
            return {Action::MOVE, {-1, 0}};
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6:
            return {Action::MOVE, {1, 0}};
        case SDL_SCANCODE_KP_7:
            return {Action::MOVE, {-1, -1}};
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8:
            return {Action::MOVE, {0, -1}};
        case SDL_SCANCODE_KP_9:
            return {Action::MOVE, {1, -1}};

        case SDL_SCANCODE_SPACE:
            return {Action::WAIT, {0, 0}};

        case SDL_SCANCODE_V:
            cheatcode_full_visibility = !cheatcode_full_visibility;
            break;
        case SDL_SCANCODE_H:
            you->hitpoints += 100;
            break;
        case SDL_SCANCODE_K:
            cheatcode_kill_everybody_in_the_world();
            break;
        case SDL_SCANCODE_P:
            cheatcode_polymorph();
            break;
        case SDL_SCANCODE_S:
            cheatcode_spectate(get_mouse_tile());
            break;
        case SDL_SCANCODE_I:
            you->invisible = !you->invisible;
            break;
        case SDL_SCANCODE_G:
            spawn_a_monster(SpeciesId_COUNT);
            break;

        default:
            break;
    }
    return {Action::UNDECIDED, {0, 0}};
}

static Action peek_input_command() {
    SDL_Event event;
    Action action = {Action::UNDECIDED, {0, 0}};
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
    return action;
}
static void step_game() {
    // run the game until it's our turn
    while (you->is_alive && you->movement_points < you->species->movement_cost) {
        advance_time();
    }
    // time to decide what to do
    Action action = peek_input_command();
    if (action.type == Action::UNDECIDED || !you->is_alive)
        return; // this happens about 60 times per second
    if (action.type == Action::MOVE) {
        // convert moving into attacking if it's pointed at an observed monster.
        if (find_perceived_individual_at(you, you->location + action.coord) != NULL)
            action.type = Action::ATTACK;
    }
    take_action(you, action);
}

int main(int argc, char * argv[]) {
    init_random();
    display_init();
    swarkland_init();

    while (!request_shutdown) {
        step_game();

        render();

        SDL_Delay(17); // 60Hz or whatever
    }

    display_finish();
    return 0;
}
