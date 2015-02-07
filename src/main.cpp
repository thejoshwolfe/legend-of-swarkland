#include "swarkland.hpp"
#include "display.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "util.hpp"
#include "geometry.hpp"
#include "decision.hpp"
#include "tas.hpp"

#include <stdbool.h>

#include <SDL2/SDL.h>

static bool request_shutdown = false;

static Action move_or_attack(Coord direction) {
    Action action = {Action::MOVE, direction};
    // convert moving into attacking if it's pointed at an observed monster.
    if (find_perceived_individual_at(you, you->location + action.coord) != NULL)
        action.type = Action::ATTACK;
    return action;
}
static Action on_key_down(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            request_shutdown = true;
            break;

        case SDL_SCANCODE_KP_1:
            return move_or_attack({-1, 1});
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2:
            return move_or_attack({0, 1});
        case SDL_SCANCODE_KP_3:
            return move_or_attack({1, 1});
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4:
            return move_or_attack({-1, 0});
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6:
            return move_or_attack({1, 0});
        case SDL_SCANCODE_KP_7:
            return move_or_attack({-1, -1});
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8:
            return move_or_attack({0, -1});
        case SDL_SCANCODE_KP_9:
            return move_or_attack({1, -1});

        case SDL_SCANCODE_SPACE:
            return Action::wait();

        case SDL_SCANCODE_Z:
            return Action::zap();

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
            cheatcode_spectate(get_mouse_tile());
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

static void read_input() {
    SDL_Event event;
    Action action = Action::undecided();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                action = on_key_down(event);
                break;
            case SDL_MOUSEMOTION:
                on_mouse_motion();
                break;
            case SDL_QUIT:
                request_shutdown = true;
                break;
        }
    }
    current_player_decision = action;
}

static void process_argv(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-script") == 0) {
            if (i + 1 >= argc)
                panic("expected argument");
            i++;
            char * filename = argv[i];
            tas_set_output_script(filename);
        } else if (strcmp(argv[i], "--script") == 0) {
            if (i + 1 >= argc)
                panic("expected argument");
            i++;
            char * filename = argv[i];
            tas_set_input_script(filename);
        } else {
            panic("unrecognized parameter");
        }
    }
}

int compare_int(int a, int b) {
    return a - b;
}

int main(int argc, char * argv[]) {
    process_argv(argc, argv);
    init_random();

    display_init();
    swarkland_init();
    init_decisions();

    while (!request_shutdown) {
        read_input();

        run_the_game();

        render();

        SDL_Delay(17); // 60Hz or whatever
    }

    display_finish();
    return 0;
}
