#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"
#include "byte_buffer.hpp"
#include "item.hpp"
#include "input.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// screen layout
static const SDL_Rect message_area = { 0, 0, map_size.x * tile_size, 2 * tile_size };
const SDL_Rect main_map_area = { 0, message_area.y + message_area.h, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, 32 };
static const SDL_Rect hp_area = { 0, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect kills_area = { hp_area.x + hp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect status_area = { kills_area.x + kills_area.w, status_box_area.y, status_box_area.w - (kills_area.x + kills_area.w), status_box_area.h };
static const SDL_Rect inventory_area = { main_map_area.x + main_map_area.w, 2 * tile_size, 5 * tile_size, status_box_area.y + status_box_area.h };
static const SDL_Rect entire_window_area = { 0, 0, inventory_area.x + inventory_area.w, status_box_area.y + status_box_area.h };


static SDL_Window * window;
static SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static struct RuckSackBundle * bundle;
static struct RuckSackTexture * rs_texture;
static struct RuckSackImage ** spritesheet_images;

static struct RuckSackImage * species_images[SpeciesId_COUNT];
static struct RuckSackImage * floor_images[8];
static struct RuckSackImage * wall_images[8];
static struct RuckSackImage * wand_images[WandDescriptionId_COUNT];

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
    species_images[SpeciesId_PINK_BLOB] = find_image(spritesheet_images, image_count, "img/pink_blob.png");
    species_images[SpeciesId_AIR_ELEMENTAL] = find_image(spritesheet_images, image_count, "img/air_elemental.png");

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

    wand_images[WandDescriptionId_BONE_WAND] = find_image(spritesheet_images, image_count, "img/bone_wand.png");
    wand_images[WandDescriptionId_GOLD_WAND] = find_image(spritesheet_images, image_count, "img/gold_wand.png");
    wand_images[WandDescriptionId_PLASTIC_WAND] = find_image(spritesheet_images, image_count, "img/plastic_wand.png");
}

void display_init(const char * resource_bundle_path) {
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

    if (rucksack_bundle_open_read(resource_bundle_path, &bundle) != RuckSackErrorNone) {
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

    struct RuckSackFileEntry * font_entry = rucksack_bundle_find_file(bundle, "font/OpenSans-Regular.ttf", -1);
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

static inline bool rect_contains(SDL_Rect rect, Coord point) {
    return rect.x <= point.x && point.x < rect.x + rect.w &&
           rect.y <= point.y && point.y < rect.y + rect.h;
}

static Individual get_spectate_individual() {
    return cheatcode_spectator != NULL ? cheatcode_spectator : you;
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

static const SDL_Color white = {0xff, 0xff, 0xff};
// the text will be aligned to the bottom of the area
static void render_text(const char * str, SDL_Rect area) {
    if (*str == '\0')
        return; // it's actually an error to try to render the empty string
    SDL_Surface * surface = TTF_RenderText_Blended_Wrapped(status_box_font, str, white, area.w);
    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);

    // holy shit. the goddamn box is too tall.
    // each new line added during the wrap adds some extra blank space at the bottom.
    // this forumla here was determined through experimentation.
    // piece of shit.
    int real_surface_h = 1 + (int)(surface->h - (float)surface->h / 12.5f);

    SDL_Rect source_rect;
    source_rect.w = min(surface->w, area.w);
    source_rect.h = min(real_surface_h, area.h);
    // align the bottom
    source_rect.x = 0;
    source_rect.y = real_surface_h - source_rect.h;

    SDL_Rect dest_rect;
    dest_rect.x = area.x;
    dest_rect.y = area.y;
    dest_rect.w = source_rect.w;
    dest_rect.h = source_rect.h;
    SDL_RenderFillRect(renderer, &dest_rect);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_NONE);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

Coord get_mouse_tile(SDL_Rect area) {
    Coord pixels = get_mouse_pixels();
    if (!rect_contains(area, pixels))
        return Coord::nowhere();
    pixels.x -= area.x;
    pixels.y -= area.y;
    Coord tile_coord = {pixels.x / tile_size, pixels.y / tile_size};
    return tile_coord;
}

const char * get_species_name(SpeciesId species_id) {
    switch (species_id) {
        case SpeciesId_HUMAN:
            return "human";
        case SpeciesId_OGRE:
            return "ogre";
        case SpeciesId_DOG:
            return "dog";
        case SpeciesId_PINK_BLOB:
            return "pink blob";
        case SpeciesId_AIR_ELEMENTAL:
            return "air elemental";
        default:
            panic("individual description");
    }
}
void get_individual_description(Individual observer, uint256 target_id, ByteBuffer * output) {
    if (observer->id == target_id) {
        output->append("you");
        return;
    }
    PerceivedIndividual target = observer->knowledge.perceived_individuals.get(target_id, NULL);
    if (target == NULL) {
        output->append("it");
        return;
    }
    output->append("a ");
    if (target->status_effects.invisible)
        output->append("invisible ");
    if (target->status_effects.confused_timeout > 0)
        output->append("confused ");
    output->append(get_species_name(target->species_id));
}

static void popup_help(Coord upper_left_corner, const char * str) {
    // eh... we're going to round this to tile boundaries.
    // we need a better solution for this or something
    upper_left_corner.x = tile_size * (upper_left_corner.x / tile_size);
    upper_left_corner.y = tile_size * (upper_left_corner.y / tile_size);
    SDL_Rect rect;
    rect.x = upper_left_corner.x;
    rect.y = upper_left_corner.y;
    rect.w = entire_window_area.w - rect.x;
    rect.h = entire_window_area.h - rect.y;
    render_text(str, rect);
}

void render() {
    Individual spectate_from = get_spectate_individual();

    SDL_RenderClear(renderer);

    // main map
    // render the terrain
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile tile = spectate_from->knowledge.tiles[cursor];
            if (cheatcode_full_visibility)
                tile = actual_map_tiles[cursor];
            if (tile.tile_type == TileType_UNKNOWN)
                continue;
            Uint8 alpha;
            if (spectate_from->knowledge.tile_is_visible[cursor].any()) {
                // it's in our direct line of sight
                if (input_mode == InputMode_ZAP_CHOOSE_DIRECTION) {
                    // actually, let's only show the cardinal directions
                    Coord vector = spectate_from->location - cursor;
                    if (vector.x * vector.y == 0) {
                        // cardinal direction
                        alpha = 255;
                    } else if (abs(vector.x) == abs(vector.y)) {
                        // diagnoal
                        alpha = 255;
                    } else {
                        alpha = 128;
                    }
                } else {
                    alpha = 255;
                }
            } else {
                alpha = 128;
            }
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
            if (individual->status_effects.invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species_id], individual->location);
        }
    } else {
        // full visibility
        for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
            Individual individual = iterator.next();
            if (!individual->is_alive)
                continue;
            Uint8 alpha;
            if (individual->status_effects.invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species_id], individual->location);
        }
    }

    // status box
    {
        ByteBuffer status_text;
        status_text.format("HP: %d", spectate_from->hitpoints);
        render_text(status_text.raw(), hp_area);

        status_text.resize(0);
        status_text.format("Kills: %d", spectate_from->kill_counter);
        render_text(status_text.raw(), kills_area);

        status_text.resize(0);
        if (spectate_from->status_effects.invisible)
            status_text.append("invisible ");
        if (spectate_from->status_effects.confused_timeout > 0)
            status_text.append("confused ");
        render_text(status_text.raw(), status_area);
    }

    // message area
    {
        bool expand_message_box = rect_contains(message_area, get_mouse_pixels());
        ByteBuffer all_the_text;
        List<RememberedEvent> & events = spectate_from->knowledge.remembered_events;
        for (int i = 0; i < events.length(); i++) {
            RememberedEvent event = events[i];
            if (event != NULL) {
                // append something
                if (i > 0) {
                    // maybe sneak in a delimiter
                    if (events[i - 1] == NULL)
                        all_the_text.append("\n");
                    else
                        all_the_text.append("  ");
                }
                all_the_text.append(event->bytes);
            }
        }
        SDL_Rect current_message_area;
        if (expand_message_box) {
            current_message_area = entire_window_area;
        } else {
            current_message_area = message_area;
        }
        render_text(all_the_text.raw(), current_message_area);
    }

    // inventory pane
    {
        List<Item> & inventory = spectate_from->inventory;
        Coord location = {map_size.x, 0};
        for (int i = 0; i < inventory.length(); i++) {
            Item & item = inventory[i];
            render_tile(renderer, sprite_sheet_texture, wand_images[item.description_id], location);
            location.y += 1;
        }
    }

    // popup help for hovering over things
    Coord mouse_hover_map_tile = get_mouse_tile(main_map_area);
    if (mouse_hover_map_tile != Coord::nowhere()) {
        PerceivedIndividual target = find_perceived_individual_at(spectate_from, mouse_hover_map_tile);
        if (target != NULL) {
            ByteBuffer description;
            get_individual_description(spectate_from, target->id, &description);
            popup_help(get_mouse_pixels() + Coord{tile_size, tile_size}, description.raw());
        }
    }
    Coord mouse_hover_inventory_tile = get_mouse_tile(inventory_area);
    if (mouse_hover_inventory_tile.x == 0) {
        int inventory_index = mouse_hover_inventory_tile.y;
        if (0 <= inventory_index && inventory_index < spectate_from->inventory.length()) {
            ByteBuffer description;
            get_item_description(spectate_from, spectate_from->id, spectate_from->inventory[inventory_index], &description);
            popup_help(get_mouse_pixels() + Coord{tile_size, tile_size}, description.raw());
        }
    }

    SDL_RenderPresent(renderer);
}
