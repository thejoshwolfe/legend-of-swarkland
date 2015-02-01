#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"
#include "string.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static const SDL_Rect main_map_area = { 0, 0, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, 32 };
static const SDL_Rect entire_window_area = { 0, 0, status_box_area.w, status_box_area.y + status_box_area.h };

static SDL_Window * window;
static SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static struct RuckSackBundle * bundle;
static struct RuckSackTexture * rs_texture;
static struct RuckSackImage ** spritesheet_images;

static struct RuckSackImage * images[SpeciesId_COUNT];

static TTF_Font * status_box_font;

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

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, 0);
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

    sprite_sheet_texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    spritesheet_images = new struct RuckSackImage*[image_count];
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    images[SpeciesId_HUMAN] = find_image(spritesheet_images, image_count, "img/human.png");
    images[SpeciesId_OGRE] = find_image(spritesheet_images, image_count, "img/ogre.png");
    images[SpeciesId_DOG] = find_image(spritesheet_images, image_count, "img/dog.png");
    images[SpeciesId_GELATINOUS_CUBE] = find_image(spritesheet_images, image_count, "img/gelatinous_cube.png");
    images[SpeciesId_DUST_VORTEX] = find_image(spritesheet_images, image_count, "img/dust_vortex.png");

    TTF_Init();

    // TODO: absolute paths? really?
    status_box_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 16);
}

void display_finish() {
    TTF_Quit();

    delete[] spritesheet_images;
    rucksack_texture_destroy(rs_texture);

    SDL_DestroyTexture(sprite_sheet_texture);
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

static void render_text(String text, int x, int y) {
    // this seems like an awful lot of setup and tear down for every little string
    SDL_Color color = { 0xff, 0xff, 0xff };
    char * str = new char[text.size() + 1];
    text.copy(str, 0, text.size());
    str[text.size()] = '\0';

    SDL_Surface * surface = TTF_RenderText_Solid(status_box_font, str, color);
    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest_rect;
    dest_rect.x = x;
    dest_rect.y = y;
    dest_rect.w = surface->w;
    dest_rect.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    delete[] str;
}

void render() {
    SDL_RenderClear(renderer);

    // main map
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (individual->is_alive)
            render_tile(renderer, sprite_sheet_texture, images[individual->species->species_id], individual->location);
    }

    // status box
    render_text(String("HP: ") + toString(you->hitpoints), status_box_area.x, status_box_area.y);

    SDL_RenderPresent(renderer);
}

