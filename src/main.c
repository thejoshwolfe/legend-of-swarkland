#include "load_image.h"
#include "util.h"
#include "geometry.h"

#include <stdbool.h>

#include <rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;
static const Coord map_size = { 55, 30 };

static void render_tile(SDL_Renderer * renderer, SDL_Texture * texture, struct RuckSackImage * guy_image, Coord coord) {
    SDL_Rect source_rect;
    source_rect.x = guy_image->x;
    source_rect.y = guy_image->y;
    source_rect.w = guy_image->width;
    source_rect.h = guy_image->height;

    SDL_Rect dest_rect;
    dest_rect.x = coord.x * tile_size;
    dest_rect.y = coord.y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);
}

static struct RuckSackImage * find_image(struct RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}

static void attack(int damage, int * hitpoints, bool * alive) {
    *hitpoints -= damage;
    if (*hitpoints <= 0) {
        *hitpoints = 0;
        *alive = false;
    }
}

static Coord you_position = { 4, 4 };
static Coord badguy_position = { 10, 10 };
static int you_attack = 3;
static int you_hitpoints = 10;
static bool you_alive = true;
static int badguy_attack = 2;
static int badguy_hitpoints = 10;
static bool badguy_alive = true;
static void move_the_bad(void) {
    int dx = sign(you_position.x - badguy_position.x);
    int dy = sign(you_position.y - badguy_position.y);
    Coord new_position = { clamp(badguy_position.x + dx, 0, map_size.x - 1), clamp(badguy_position.y + dy, 0, map_size.y - 1) };
    if (new_position.x == you_position.x && new_position.y == you_position.y) {
        attack(badguy_attack, &you_hitpoints, &you_alive);
    } else {
        badguy_position = new_position;
    }
}
static void you_move(int dx, int dy) {
    Coord new_position = { clamp(you_position.x + dx, 0, map_size.x - 1), clamp(you_position.y + dy, 0, map_size.y - 1) };
    if (new_position.x == badguy_position.x && new_position.y == badguy_position.y) {
        attack(you_attack, &badguy_hitpoints, &badguy_alive);
    } else {
        you_position = new_position;
    }

    move_the_bad();
}

int main(int argc, char * argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    SDL_Window * window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, map_size.x * tile_size, map_size.y * tile_size, 0);
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
    struct RuckSackImage * guy_image = find_image(spritesheet_images, image_count, "img/guy.png");
    struct RuckSackImage * bad_image = find_image(spritesheet_images, image_count, "img/bad.png");

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
                            you_move(-1, 0);
                            break;
                        case SDL_SCANCODE_UP:
                            you_move(0, -1);
                            break;
                        case SDL_SCANCODE_RIGHT:
                            you_move(1, 0);
                            break;
                        case SDL_SCANCODE_DOWN:
                            you_move(0, 1);
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

        if (you_alive)
            render_tile(renderer, texture, guy_image, you_position);
        if (badguy_alive)
            render_tile(renderer, texture, bad_image, badguy_position);

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
