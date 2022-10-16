#include "SDL.h"
#include <stdbool.h>

int WinMain() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *screen = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 300, 73, SDL_WINDOW_OPENGL);
    if (!screen) {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);
    if (!renderer) {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        return 1;
    }

    //int zig_bmp = @embedFile("zig.bmp");
    //int rw = SDL_RWFromConstMem(
    //    @ptrCast(*const c_void, &zig_bmp[0]),
    //    @intCast(c_int, zig_bmp.len),
    //);
    //if (rw == 0) {
    //    SDL_Log("Unable to get RWFromConstMem: %s", SDL_GetError());
    //    return 1;
    //}

    //const zig_surface = SDL_LoadBMP_RW(rw, 0);
    //if (zig_surface == 0) {
    //    SDL_Log("Unable to load bmp: %s", SDL_GetError());
    //    return 1;
    //}

    //const zig_texture = SDL_CreateTextureFromSurface(renderer, zig_surface) orelse {
    //    SDL_Log("Unable to create texture from surface: %s", SDL_GetError());
    //    return 1;
    //};

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                default:
                    break;
            }
        }

        SDL_RenderClear(renderer);
        //SDL_RenderCopy(renderer, zig_texture, 0, 0);
        SDL_RenderPresent(renderer);

        SDL_Delay(17);
    }
    //SDL_DestroyTexture(zig_texture);
    //SDL_FreeSurface(zig_surface);
    //SDL_RWclose(rw);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();
    return 0;
}

