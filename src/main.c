#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <rucksack.h>
#include <SDL2/SDL.h>
#include <FreeImage.h>

static void panic(const char *str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

static void *panic_malloc(size_t count, size_t size) {
    void *mem = malloc(size * count);
    if (!mem)
        panic("out of memory");
    return mem;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    struct RuckSackBundle *bundle;
    if (rucksack_bundle_open("build/resources.bundle", &bundle) != RuckSackErrorNone) {
        panic("error opening resource bundle");
    }
    SDL_Window *window = SDL_CreateWindow("Legend of Swarkland",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
    if (!window) {
        panic("window create failed");
    }

    struct RuckSackFileEntry *entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (!entry) {
        panic("spritesheet not found in bundle");
    }

    struct RuckSackTexture *rs_texture;
    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone) {
        panic("open texture failed");
    }

    long size = rucksack_texture_size(rs_texture);
    unsigned char *image_buffer = panic_malloc(size, sizeof(char));
    if (rucksack_texture_read(rs_texture, image_buffer) != RuckSackErrorNone) {
        panic("read texture failed");
    }

    FIMEMORY *fi_mem = FreeImage_OpenMemory(image_buffer, size);
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(fi_mem, 0);

    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif)) {
        panic("unable to decode spritesheet image");
    }

    FIBITMAP *bmp = FreeImage_LoadFromMemory(fif, fi_mem, 0);
    if (!FreeImage_HasPixels(bmp)) {
        panic("spritesheet image has no pixels");
    }

    int spritesheet_width = FreeImage_GetWidth(bmp);
    int spritesheet_height = FreeImage_GetHeight(bmp);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STATIC, spritesheet_width, spritesheet_height);

    SDL_UpdateTexture(texture, NULL, FreeImage_GetBits(bmp), spritesheet_width * 4);

    FreeImage_Unload(bmp);
    FreeImage_CloseMemory(fi_mem);

    bool running = true;
    while (running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    running = false;
                    break;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                running = false;
                break;
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(17);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (rucksack_bundle_close(bundle) != RuckSackErrorNone) {
        panic("error closing resource bundle");
    }
    rucksack_finish();
    SDL_Quit();
    return 0;
}


