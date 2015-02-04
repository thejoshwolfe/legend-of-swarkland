#include "swarkland.hpp"
#include "display.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "util.hpp"
#include "geometry.hpp"
#include "decision.hpp"

#include <stdbool.h>

#include <SDL2/SDL.h>

static bool request_shutdown = false;

static Action move_or_attack(Coord direction) {
    Action action = {Action::MOVE, direction};
    if (action.type == Action::MOVE) {
        // convert moving into attacking if it's pointed at an observed monster.
        if (find_perceived_individual_at(you, you->location + action.coord) != NULL)
            action.type = Action::ATTACK;
    }
    return action;
}
static Action on_key_down(const SDL_Event & event) {
    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
            request_shutdown = true;
            break;

        case SDL_SCANCODE_KP_1:
            return move_or_attack(Coord{-1, 1});
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2:
            return move_or_attack(Coord{0, 1});
        case SDL_SCANCODE_KP_3:
            return move_or_attack(Coord{1, 1});
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4:
            return move_or_attack(Coord{-1, 0});
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6:
            return move_or_attack(Coord{1, 0});
        case SDL_SCANCODE_KP_7:
            return move_or_attack(Coord{-1, -1});
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8:
            return move_or_attack(Coord{0, -1});
        case SDL_SCANCODE_KP_9:
            return move_or_attack(Coord{1, -1});

        case SDL_SCANCODE_SPACE:
            return Action::wait();

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
            spawn_a_monster(SpeciesId_COUNT, Team_BAD_GUYS, DecisionMakerType_AI);
            break;

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
            case SDL_QUIT:
                request_shutdown = true;
                break;
        }
    }
    current_player_decision = action;
}

int main(int argc, char * argv[]) {
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
