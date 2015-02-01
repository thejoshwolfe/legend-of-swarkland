#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>

static SDL_Window * window;
static SDL_Texture * texture;
static SDL_Renderer * renderer;

static struct RuckSackBundle * bundle;
static struct RuckSackTexture * rs_texture;
static struct RuckSackImage ** spritesheet_images;

static struct RuckSackImage * guy_image;
static struct RuckSackImage * bad_image;

static struct RuckSackImage * find_image(struct RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}

void display_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, map_size.x * tile_size, map_size.y * tile_size, 0);
    if (!window) {
        panic("window create failed");
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        panic("renderer create failed");
    }

    if (rucksack_bundle_open("build/resources.bundle", &bundle) != RuckSackErrorNone) {
        panic("error opening resource bundle");
    }
    struct RuckSackFileEntry * entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (!entry) {
        panic("spritesheet not found in bundle");
    }

    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone) {
        panic("open texture failed");
    }

    texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    spritesheet_images = new struct RuckSackImage*[image_count];
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    guy_image = find_image(spritesheet_images, image_count, "img/guy.png");
    bad_image = find_image(spritesheet_images, image_count, "img/bad.png");
}

void display_finish() {
    delete[] spritesheet_images;
    rucksack_texture_destroy(rs_texture);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (rucksack_bundle_close(bundle) != RuckSackErrorNone) {
        panic("error closing resource bundle");
    }
    rucksack_finish();
    SDL_Quit();
}

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

void render() {
    SDL_RenderClear(renderer);

    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (individual->is_alive)
            render_tile(renderer, texture, individual->is_ai ? bad_image : guy_image, individual->location);
    }

    SDL_RenderPresent(renderer);
}

