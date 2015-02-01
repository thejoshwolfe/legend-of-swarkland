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
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        request_shutdown = true;
                        break;
                    case SDL_SCANCODE_KP_1:
                        you_move(-1, 1);
                        break;
                    case SDL_SCANCODE_KP_3:
                        you_move(1, 1);
                        break;
                    case SDL_SCANCODE_KP_7:
                        you_move(-1, -1);
                        break;
                    case SDL_SCANCODE_KP_9:
                        you_move(1, -1);
                        break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_KP_4:
                        you_move(-1, 0);
                        break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_KP_8:
                        you_move(0, -1);
                        break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_KP_6:
                        you_move(1, 0);
                        break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_KP_2:
                        you_move(0, 1);
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
}

int main(int argc, char * argv[]) {
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
