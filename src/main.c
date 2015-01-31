#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

static void panic(const char *str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }

    SDL_Window *window = SDL_CreateWindow("Legend of Swarkland",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
    if (!window) {
        panic("window create failed");
    }

    SDL_Delay(3000);

    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
