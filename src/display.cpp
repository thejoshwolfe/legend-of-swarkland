#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// screen layout
static const SDL_Rect message_area = { 0, 0, map_size.x * tile_size, 2 * tile_size };
static const SDL_Rect main_map_area = { 0, message_area.y + message_area.h, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, 32 };
static const SDL_Rect hp_area = { 0, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect kills_area = { hp_area.x + hp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect entire_window_area = { 0, 0, status_box_area.w, status_box_area.y + status_box_area.h };


static SDL_Window * window;
static SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static struct RuckSackBundle * bundle;
static struct RuckSackTexture * rs_texture;
static struct RuckSackImage ** spritesheet_images;

static struct RuckSackImage * species_images[SpeciesId_COUNT];
static struct RuckSackImage * floor_images[8];
static struct RuckSackImage * wall_images[8];

static TTF_Font * status_box_font;
static unsigned char *font_buffer;
static SDL_RWops *font_rw_ops;

static struct RuckSackImage * find_image(struct RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}
static void load_images(struct RuckSackImage ** spritesheet_images, long image_count) {
    species_images[SpeciesId_HUMAN] = find_image(spritesheet_images, image_count, "img/human.png");
    species_images[SpeciesId_OGRE] = find_image(spritesheet_images, image_count, "img/ogre.png");
    species_images[SpeciesId_DOG] = find_image(spritesheet_images, image_count, "img/dog.png");
    species_images[SpeciesId_GELATINOUS_CUBE] = find_image(spritesheet_images, image_count, "img/gelatinous_cube.png");
    species_images[SpeciesId_DUST_VORTEX] = find_image(spritesheet_images, image_count, "img/dust_vortex.png");

    floor_images[0] = find_image(spritesheet_images, image_count, "img/grey_dirt0.png");
    floor_images[1] = find_image(spritesheet_images, image_count, "img/grey_dirt1.png");
    floor_images[2] = find_image(spritesheet_images, image_count, "img/grey_dirt2.png");
    floor_images[3] = find_image(spritesheet_images, image_count, "img/grey_dirt3.png");
    floor_images[4] = find_image(spritesheet_images, image_count, "img/grey_dirt4.png");
    floor_images[5] = find_image(spritesheet_images, image_count, "img/grey_dirt5.png");
    floor_images[6] = find_image(spritesheet_images, image_count, "img/grey_dirt6.png");
    floor_images[7] = find_image(spritesheet_images, image_count, "img/grey_dirt7.png");

    wall_images[0] = find_image(spritesheet_images, image_count, "img/brick_brown0.png");
    wall_images[1] = find_image(spritesheet_images, image_count, "img/brick_brown1.png");
    wall_images[2] = find_image(spritesheet_images, image_count, "img/brick_brown2.png");
    wall_images[3] = find_image(spritesheet_images, image_count, "img/brick_brown3.png");
    wall_images[4] = find_image(spritesheet_images, image_count, "img/brick_brown4.png");
    wall_images[5] = find_image(spritesheet_images, image_count, "img/brick_brown5.png");
    wall_images[6] = find_image(spritesheet_images, image_count, "img/brick_brown6.png");
    wall_images[7] = find_image(spritesheet_images, image_count, "img/brick_brown7.png");
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
    load_images(spritesheet_images, image_count);

    TTF_Init();

    struct RuckSackFileEntry *font_entry = rucksack_bundle_find_file(bundle,
            "font/OpenSans-Regular.ttf", -1);
    if (!font_entry) {
        panic("font not found in bundle");
    }
    long font_file_size = rucksack_file_size(font_entry);
    font_buffer = new unsigned char[font_file_size];
    rucksack_file_read(font_entry, font_buffer);
    font_rw_ops = SDL_RWFromMem(font_buffer, font_file_size);
    if (!font_rw_ops) {
        panic("sdl rwops fail");
    }
    status_box_font = TTF_OpenFontRW(font_rw_ops, 0, 16);
}

void display_finish() {
    TTF_Quit();

    SDL_RWclose(font_rw_ops);
    delete[] font_buffer;

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
    dest_rect.x = main_map_area.x + coord.x * tile_size;
    dest_rect.y = main_map_area.y + coord.y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);
}

// the height doesn't matter
static void render_text(const char * str, SDL_Rect area) {
    // this seems like an awful lot of setup and tear down for every little string
    SDL_Color color = { 0xff, 0xff, 0xff };

    SDL_Surface * surface = TTF_RenderText_Blended_Wrapped(status_box_font, str, color, area.w);
    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dest_rect;
    dest_rect.x = area.x;
    dest_rect.y = area.y;
    dest_rect.w = surface->w;
    dest_rect.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

Coord get_mouse_tile() {
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    Coord tile_coord = {x / tile_size, y / tile_size};
    return tile_coord;
}

void render() {
    SDL_RenderClear(renderer);

    // message area
    render_text("the very fast fox jumps over the sleeping dog!", message_area);

    // main map
    // render the terrain
    Individual spectate_from = you;
    if (cheatcode_spectator != NULL)
        spectate_from = cheatcode_spectator;
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile tile = spectate_from->knowledge.tiles[cursor];
            if (cheatcode_full_visibility)
                tile = actual_map_tiles[cursor];
            if (tile.tile_type == TileType_UNKNOWN)
                continue;
            Uint8 alpha = 0;
            if (spectate_from->knowledge.tile_is_visible[cursor].any())
                alpha = 255;
            else
                alpha = 128;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            RuckSackImage * image = (tile.tile_type == TileType_FLOOR ? floor_images : wall_images)[tile.aesthetic_index];
            render_tile(renderer, sprite_sheet_texture, image, cursor);
        }
    }

    // render the individuals
    if (!cheatcode_full_visibility) {
        // not cheating
        for (auto iterator = spectate_from->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
            PerceivedIndividual individual = iterator.next();
            Uint8 alpha;
            if (individual->invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species->species_id], individual->location);
        }
    } else {
        // full visibility
        for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
            Individual individual = iterator.next();
            if (!individual->is_alive)
                continue;
            Uint8 alpha;
            if (individual->invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species_id], individual->location);
        }
    }

    // status box
    ByteBuffer status_text;
    status_text.format("HP: %d", you->hitpoints);
    render_text(status_text.raw(), hp_area);

    status_text.resize(0);
    status_text.format("Kills: %d", you->kill_counter);
    render_text(status_text.raw(), kills_area);

    SDL_RenderPresent(renderer);
}
