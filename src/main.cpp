#include "swarkland.hpp"
#include "display.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "util.hpp"
#include "geometry.hpp"

#include <stdbool.h>

#include <SDL2/SDL.h>

static bool request_shutdown = false;

static void step_game() {
    SDL_Event event;
    Coord requested_move(0, 0);
    bool do_nothing = false;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        request_shutdown = true;
                        break;

                    case SDL_SCANCODE_KP_1:
                        requested_move = Coord(-1, 1);
                        break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_KP_2:
                        requested_move = Coord(0, 1);
                        break;
                    case SDL_SCANCODE_KP_3:
                        requested_move = Coord(1, 1);
                        break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_KP_4:
                        requested_move = Coord(-1, 0);
                        break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_KP_6:
                        requested_move = Coord(1, 0);
                        break;
                    case SDL_SCANCODE_KP_7:
                        requested_move = Coord(-1, -1);
                        break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_KP_8:
                        requested_move = Coord(0, -1);
                        break;
                    case SDL_SCANCODE_KP_9:
                        requested_move = Coord(1, -1);
                        break;

                    case SDL_SCANCODE_SPACE:
                        do_nothing = true;
                        break;

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

                    default:
                        break;
                }
                break;
            case SDL_QUIT:
                request_shutdown = true;
                break;
        }
    }

    while (you->is_alive) {
        if (you->movement_points >= you->species->movement_cost) {
            // you can move. do you choose to?
            if (take_action(do_nothing, requested_move)) {
                // chose to move
                you->movement_points = 0;
                // resume time.
            } else {
                // stop time until the player moves
                break;
            }
            requested_move = Coord(0, 0);
            do_nothing = false;
        }

        advance_time();
    }
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
    swarkland_finish();
    return 0;
}
