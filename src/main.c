#include "load_image.h"
#include "util.h"

#include <stdbool.h>

#include <rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;
static const int map_size_x = 55;
static const int map_size_y = 30;

static void render_tile(SDL_Renderer * renderer, SDL_Texture * texture, struct RuckSackImage * guy_image, int x, int y) {
    SDL_Rect source_rect;
    source_rect.x = guy_image->x;
    source_rect.y = guy_image->y;
    source_rect.w = guy_image->width;
    source_rect.h = guy_image->height;

    SDL_Rect dest_rect;
    dest_rect.x = x * tile_size;
    dest_rect.y = y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);
}

static struct RuckSackImage * find_image(struct RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i< image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}

static int you_x = 4;
static int you_y = 4;
static int bad_x = 10;
static int bad_y = 10;
static void move_the_bad(void) {
    int dx = sign(bad_x - you_x);
    int dy = sign(bad_y - you_y);
    bad_x -= dx;
    bad_y -= dy;
}
static void move(int dx, int dy){
    you_x = clamp(you_x + dx, 0, map_size_x - 1);
    you_y = clamp(you_y + dy, 0, map_size_y - 1);

    move_the_bad();
}

int main(int argc, char * argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    SDL_Window * window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, map_size_x * tile_size, map_size_y * tile_size, 0);
    if (!window) {
        panic("window create failed");
    }
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        panic("renderer create failed");
    }

    struct RuckSackBundle * bundle;
    if (rucksack_bundle_open("build/resources.bundle", &bundle) != RuckSackErrorNone) {
        panic("error opening resource bundle");
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
    struct RuckSackImage * guy_image =  find_image(spritesheet_images, image_count, "img/guy.png");
    struct RuckSackImage * bad_image =  find_image(spritesheet_images, image_count, "img/bad.png");

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
                        case SDL_SCANCODE_LEFT:
                            move(-1, 0);
                            break;
                        case SDL_SCANCODE_UP:
                            move(0, -1);
                            break;
                        case SDL_SCANCODE_RIGHT:
                            move(1, 0);
                            break;
                        case SDL_SCANCODE_DOWN:
                            move(0, 1);
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

        render_tile(renderer, texture, guy_image, you_x, you_y);
        render_tile(renderer, texture, bad_image, bad_x, bad_y);

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
