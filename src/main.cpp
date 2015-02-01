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
                    case SDL_SCANCODE_KP_3:
                        requested_move = Coord(1, 1);
                        break;
                    case SDL_SCANCODE_KP_7:
                        requested_move = Coord(-1, -1);
                        break;
                    case SDL_SCANCODE_KP_9:
                        requested_move = Coord(1, -1);
                        break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_KP_4:
                        requested_move = Coord(-1, 0);
                        break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_KP_8:
                        requested_move = Coord(0, -1);
                        break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_KP_6:
                        requested_move = Coord(1, 0);
                        break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_KP_2:
                        requested_move = Coord(0, 1);
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
            if (requested_move.x == 0 && requested_move.y == 0)
                break; // stop time until the player moves
            // choose to move
            you->movement_points = 0;
            you_move(requested_move);
            requested_move = Coord(0, 0);
            // resume time.
        }

        advance_time();
    }
}

int main(int argc, char * argv[]) {
    display_init();
    swarkland_init();
    init_random();

    while (!request_shutdown) {
        step_game();

        render();

        SDL_Delay(17); // 60Hz or whatever
    }

    display_finish();
    swarkland_finish();
    return 0;
}
