#include "load_image.h"
#include "util.h"

#include <stdbool.h>

#include <rucksack.h>
#include <SDL2/SDL.h>

int main(int argc, char * argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    struct RuckSackBundle * bundle;
    if (rucksack_bundle_open("build/resources.bundle", &bundle) != RuckSackErrorNone) {
        panic("error opening resource bundle");
    }
    SDL_Window * window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
    if (!window) {
        panic("window create failed");
    }
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        panic("renderer create failed");
    }

    struct RuckSackFileEntry * entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (!entry) {
        panic("spritesheet not found in bundle");
    }

    struct RuckSackTexture * rs_texture;
    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone) {
        panic("open texture failed");
    }

    SDL_Texture * texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    struct RuckSackImage ** spritesheet_images = panic_malloc(image_count, sizeof(struct RuckSackImage*));
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    if (image_count != 1) {
        panic("code only handles 1 image at the moment");
    }
    struct RuckSackImage *guy_image = spritesheet_images[0];

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
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

        SDL_Rect source_rect;
        source_rect.x = guy_image->x;
        source_rect.y = guy_image->y;
        source_rect.w = guy_image->width;
        source_rect.h = guy_image->height;
        SDL_Rect dest_rect;
        dest_rect.x = 100; // arbitrary value
        dest_rect.y = 100; // arbitrary value
        dest_rect.w = guy_image->width;
        dest_rect.h = guy_image->height;
        SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);

        SDL_RenderPresent(renderer);
        SDL_Delay(17); // 60Hz or whatever
    }

    free(spritesheet_images);
    rucksack_texture_destroy(rs_texture);

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

