#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"
#include "byte_buffer.hpp"
#include "item.hpp"
#include "input.hpp"
#include "string.hpp"
#include "text.hpp"
#include "resources.hpp"
#include "event.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

// screen layout
static const SDL_Rect message_area = { 0, 0, map_size.x * tile_size, 2 * tile_size };
const SDL_Rect main_map_area = { 0, message_area.y + message_area.h, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, tile_size };
static const SDL_Rect hp_area = { 0, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect xp_area = { hp_area.x + hp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect dungeon_level_area = { xp_area.x + xp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect status_area = { dungeon_level_area.x + dungeon_level_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect time_area = { status_area.x + status_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect inventory_area = { main_map_area.x + main_map_area.w, 2 * tile_size, inventory_layout_width * tile_size, (map_size.y - 4) * tile_size };
static const SDL_Rect tutorial_area = { inventory_area.x, inventory_area.y + inventory_area.h, 5 * tile_size, 4 * tile_size };
static const SDL_Rect version_area = { status_box_area.x + status_box_area.w, status_box_area.y, 5 * tile_size, tile_size };
static const SDL_Rect entire_window_area = { 0, 0, inventory_area.x + inventory_area.w, status_box_area.y + status_box_area.h };


static SDL_Window * window;
SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static RuckSackBundle * bundle;
static RuckSackTexture * rs_texture;
static RuckSackImage ** spritesheet_images;

static RuckSackImage * species_images[SpeciesId_COUNT];
static RuckSackImage * unseen_individual_image;
static RuckSackImage * wand_images[WandDescriptionId_COUNT];
static RuckSackImage * unseen_wand_image;
static RuckSackImage * potion_images[PotionDescriptionId_COUNT];
static RuckSackImage * unseen_potion_image;
static Array<RuckSackImage *, 8> dirt_floor_images;
static Array<RuckSackImage *, 6> marble_floor_images;
static Array<RuckSackImage *, 8> wall_images;
static RuckSackImage * stairs_down_image;
static RuckSackImage * equipment_image;

TTF_Font * status_box_font;
static unsigned char *font_buffer;
static SDL_RWops *font_rw_ops;
static String version_string = new_string();

static RuckSackImage * find_image(RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}
static void load_images(RuckSackImage ** spritesheet_images, long image_count) {
    species_images[SpeciesId_HUMAN] = find_image(spritesheet_images, image_count, "img/individual/human.png");
    species_images[SpeciesId_OGRE] = find_image(spritesheet_images, image_count, "img/individual/ogre.png");
    species_images[SpeciesId_LICH] = find_image(spritesheet_images, image_count, "img/individual/lich.png");
    species_images[SpeciesId_DOG] = find_image(spritesheet_images, image_count, "img/individual/dog.png");
    species_images[SpeciesId_PINK_BLOB] = find_image(spritesheet_images, image_count, "img/individual/pink_blob.png");
    species_images[SpeciesId_AIR_ELEMENTAL] = find_image(spritesheet_images, image_count, "img/individual/air_elemental.png");
    species_images[SpeciesId_ANT] = find_image(spritesheet_images, image_count, "img/individual/ant.png");
    species_images[SpeciesId_BEE] = find_image(spritesheet_images, image_count, "img/individual/bee.png");
    species_images[SpeciesId_BEETLE] = find_image(spritesheet_images, image_count, "img/individual/beetle.png");
    species_images[SpeciesId_SCORPION] = find_image(spritesheet_images, image_count, "img/individual/scorpion.png");
    species_images[SpeciesId_SNAKE] = find_image(spritesheet_images, image_count, "img/individual/snake.png");
    unseen_individual_image = find_image(spritesheet_images, image_count, "img/individual/unseen_individual.png");

    dirt_floor_images[0] = find_image(spritesheet_images, image_count, "img/map/dirt_floor0.png");
    dirt_floor_images[1] = find_image(spritesheet_images, image_count, "img/map/dirt_floor1.png");
    dirt_floor_images[2] = find_image(spritesheet_images, image_count, "img/map/dirt_floor2.png");
    dirt_floor_images[3] = find_image(spritesheet_images, image_count, "img/map/dirt_floor3.png");
    dirt_floor_images[4] = find_image(spritesheet_images, image_count, "img/map/dirt_floor4.png");
    dirt_floor_images[5] = find_image(spritesheet_images, image_count, "img/map/dirt_floor5.png");
    dirt_floor_images[6] = find_image(spritesheet_images, image_count, "img/map/dirt_floor6.png");
    dirt_floor_images[7] = find_image(spritesheet_images, image_count, "img/map/dirt_floor7.png");

    marble_floor_images[0] = find_image(spritesheet_images, image_count, "img/map/marble_floor0.png");
    marble_floor_images[1] = find_image(spritesheet_images, image_count, "img/map/marble_floor1.png");
    marble_floor_images[2] = find_image(spritesheet_images, image_count, "img/map/marble_floor2.png");
    marble_floor_images[3] = find_image(spritesheet_images, image_count, "img/map/marble_floor3.png");
    marble_floor_images[4] = find_image(spritesheet_images, image_count, "img/map/marble_floor4.png");
    marble_floor_images[5] = find_image(spritesheet_images, image_count, "img/map/marble_floor5.png");

    wall_images[0] = find_image(spritesheet_images, image_count, "img/map/brick_brown0.png");
    wall_images[1] = find_image(spritesheet_images, image_count, "img/map/brick_brown1.png");
    wall_images[2] = find_image(spritesheet_images, image_count, "img/map/brick_brown2.png");
    wall_images[3] = find_image(spritesheet_images, image_count, "img/map/brick_brown3.png");
    wall_images[4] = find_image(spritesheet_images, image_count, "img/map/brick_brown4.png");
    wall_images[5] = find_image(spritesheet_images, image_count, "img/map/brick_brown5.png");
    wall_images[6] = find_image(spritesheet_images, image_count, "img/map/brick_brown6.png");
    wall_images[7] = find_image(spritesheet_images, image_count, "img/map/brick_brown7.png");

    stairs_down_image = find_image(spritesheet_images, image_count, "img/map/stairs_down.png");

    wand_images[WandDescriptionId_BONE_WAND] = find_image(spritesheet_images, image_count, "img/wand/bone_wand.png");
    wand_images[WandDescriptionId_GOLD_WAND] = find_image(spritesheet_images, image_count, "img/wand/gold_wand.png");
    wand_images[WandDescriptionId_PLASTIC_WAND] = find_image(spritesheet_images, image_count, "img/wand/plastic_wand.png");
    wand_images[WandDescriptionId_COPPER_WAND] = find_image(spritesheet_images, image_count, "img/wand/copper_wand.png");
    wand_images[WandDescriptionId_PURPLE_WAND] = find_image(spritesheet_images, image_count, "img/wand/purple_wand.png");
    unseen_wand_image = find_image(spritesheet_images, image_count, "img/wand/unseen_wand.png");

    potion_images[PotionDescriptionId_BLUE_POTION] = find_image(spritesheet_images, image_count, "img/potion/blue_potion.png");
    potion_images[PotionDescriptionId_GREEN_POTION] = find_image(spritesheet_images, image_count, "img/potion/green_potion.png");
    potion_images[PotionDescriptionId_RED_POTION] = find_image(spritesheet_images, image_count, "img/potion/red_potion.png");
    potion_images[PotionDescriptionId_YELLOW_POTION] = find_image(spritesheet_images, image_count, "img/potion/yellow_potion.png");
    potion_images[PotionDescriptionId_ORANGE_POTION] = find_image(spritesheet_images, image_count, "img/potion/orange_potion.png");
    potion_images[PotionDescriptionId_PURPLE_POTION] = find_image(spritesheet_images, image_count, "img/potion/purple_potion.png");
    unseen_potion_image = find_image(spritesheet_images, image_count, "img/potion/unseen_potion.png");

    equipment_image = find_image(spritesheet_images, image_count, "img/equipment.png");
}

void display_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        panic("unable to init SDL");

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, 0);
    if (window == nullptr)
        panic("window create failed");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
        panic("renderer create failed");

    if (rucksack_bundle_open_read_mem(get_binary_resources_start(), get_binary_resources_size(), &bundle) != RuckSackErrorNone)
        panic("error opening resource bundle");
    RuckSackFileEntry * entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (entry == nullptr)
        panic("spritesheet not found in bundle");

    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone)
        panic("open texture failed");

    sprite_sheet_texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    spritesheet_images = allocate<RuckSackImage*>(image_count);
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    load_images(spritesheet_images, image_count);

    TTF_Init();

    RuckSackFileEntry * font_entry = rucksack_bundle_find_file(bundle, "font/DejaVuSansMono.ttf", -1);
    if (font_entry == nullptr)
        panic("font not found in bundle");
    long font_file_size = rucksack_file_size(font_entry);
    font_buffer = allocate<unsigned char>(font_file_size);
    rucksack_file_read(font_entry, font_buffer);
    font_rw_ops = SDL_RWFromMem(font_buffer, font_file_size);
    if (font_rw_ops == nullptr)
        panic("sdl rwops fail");
    status_box_font = TTF_OpenFontRW(font_rw_ops, 0, 13);
    TTF_SetFontHinting(status_box_font, TTF_HINTING_LIGHT);

    RuckSackFileEntry * version_entry = rucksack_bundle_find_file(bundle, "version", -1);
    if (version_entry == nullptr)
        panic("version not found in bundle");
    ByteBuffer buffer;
    buffer.resize(rucksack_file_size(version_entry));
    rucksack_file_read(version_entry, (unsigned char *)buffer.raw());
    version_string->decode(buffer);
}

void display_finish() {
    TTF_Quit();

    SDL_RWclose(font_rw_ops);
    destroy(font_buffer, 0);

    destroy(spritesheet_images, 0);
    rucksack_texture_close(rs_texture);

    SDL_DestroyTexture(sprite_sheet_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (rucksack_bundle_close(bundle) != RuckSackErrorNone) {
        panic("error closing resource bundle");
    }
    SDL_Quit();
}

static inline bool rect_contains(SDL_Rect rect, Coord point) {
    return rect.x <= point.x && point.x < rect.x + rect.w &&
           rect.y <= point.y && point.y < rect.y + rect.h;
}

static Thing get_spectate_individual() {
    return cheatcode_spectator != nullptr ? cheatcode_spectator : you;
}

static void render_tile(SDL_Renderer * renderer, SDL_Texture * texture, RuckSackImage * image, int alpha, Coord coord) {
    SDL_Rect source_rect;
    source_rect.x = image->x;
    source_rect.y = image->y;
    source_rect.w = image->width;
    source_rect.h = image->height;

    SDL_Rect dest_rect;
    dest_rect.x = main_map_area.x + coord.x * tile_size;
    dest_rect.y = main_map_area.y + coord.y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, nullptr, SDL_FLIP_VERTICAL);
}

// {0, 0, w, h}
static inline SDL_Rect get_texture_bounds(SDL_Texture * texture) {
    SDL_Rect result = {0, 0, 0, 0};
    Uint32 format;
    int access;
    SDL_QueryTexture(texture, &format, &access, &result.w, &result.h);
    return result;
}

static void set_color(SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}
static void render_texture(SDL_Texture * texture, SDL_Rect source_rect, SDL_Rect output_area, int horizontal_align, int vertical_align) {
    SDL_Rect dest_rect;
    if (horizontal_align < 0) {
        dest_rect.x = output_area.x + output_area.w - source_rect.w;
    } else {
        dest_rect.x = output_area.x;
    }
    if (vertical_align < 0) {
        dest_rect.y = output_area.y + output_area.h - source_rect.h;
    } else {
        dest_rect.y = output_area.y;
    }
    dest_rect.w = source_rect.w;
    dest_rect.h = source_rect.h;
    set_color(black);
    SDL_RenderFillRect(renderer, &dest_rect);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, nullptr, SDL_FLIP_NONE);
}
static void render_div(Div div, SDL_Rect output_area, int horizontal_align, int vertical_align) {
    div->set_max_size(output_area.w, output_area.h);
    SDL_Texture * texture = div->get_texture(renderer);
    if (texture == nullptr)
        return;
    SDL_Rect source_rect = get_texture_bounds(texture);
    render_texture(texture, source_rect, output_area, horizontal_align, vertical_align);
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

static const char * get_species_name_str(SpeciesId species_id) {
    switch (species_id) {
        case SpeciesId_HUMAN:
            return "human";
        case SpeciesId_OGRE:
            return "ogre";
        case SpeciesId_LICH:
            return "lich king";
        case SpeciesId_PINK_BLOB:
            return "pink blob";
        case SpeciesId_AIR_ELEMENTAL:
            return "air elemental";
        case SpeciesId_DOG:
            return "dog";
        case SpeciesId_ANT:
            return "ant";
        case SpeciesId_BEE:
            return "bee";
        case SpeciesId_BEETLE:
            return "beetle";
        case SpeciesId_SCORPION:
            return "scorpion";
        case SpeciesId_SNAKE:
            return "snake";

        case SpeciesId_UNSEEN:
            return "something";

        case SpeciesId_COUNT:
            panic("not a real species");
    }
    panic("individual description");
}
Span get_species_name(SpeciesId species_id) {
    return new_span(get_species_name_str(species_id), light_brown, black);
}
// ends with " " if non-blank
static Span get_status_description(const List<StatusEffect::StatusEffectType> & status_effects) {
    Span result = new_span();
    // this algorithmic complexity is a little derpy,
    // but we need the order of words to be consistent.
    if (has_status(status_effects, StatusEffect::CONFUSION))
        result->append("confused ");
    if (has_status(status_effects, StatusEffect::SPEED))
        result->append("fast ");
    if (has_status(status_effects, StatusEffect::ETHEREAL_VISION))
        result->append("ethereal-visioned ");
    if (has_status(status_effects, StatusEffect::COGNISCOPY))
        result->append("cogniscopic ");
    if (has_status(status_effects, StatusEffect::BLINDNESS))
        result->append("blind ");
    if (has_status(status_effects, StatusEffect::POISON))
        result->append("poisoned ");
    if (has_status(status_effects, StatusEffect::INVISIBILITY))
        result->append("invisible ");
    result->set_color(pink, black);
    return result;
}
static Span get_status_description(const List<StatusEffect> & status_effects) {
    List<StatusEffect::StatusEffectType> tmp_effect_list;
    for (int i = 0; i < status_effects.length(); i++)
        tmp_effect_list.append(status_effects[i].type);
    return get_status_description(tmp_effect_list);
}
static const char * get_wand_description_str(Thing observer, PerceivedThing item) {
    WandDescriptionId description_id = item->wand_info()->description_id;
    WandId true_id = description_id != WandDescriptionId_UNSEEN ? observer->life()->knowledge.wand_identities[description_id] : WandId_UNKNOWN;
    if (true_id != WandId_UNKNOWN) {
        switch (true_id) {
            case WandId_WAND_OF_CONFUSION:
                return "wand of confusion";
            case WandId_WAND_OF_DIGGING:
                return "wand of digging";
            case WandId_WAND_OF_STRIKING:
                return "wand of striking";
            case WandId_WAND_OF_SPEED:
                return "wand of speed";
            case WandId_WAND_OF_REMEDY:
                return "wand of remedy";

            case WandId_COUNT:
            case WandId_UNKNOWN:
                panic("not a real wand id");
        }
        panic("item id");
    } else {
        switch (description_id) {
            case WandDescriptionId_BONE_WAND:
                return "bone wand";
            case WandDescriptionId_GOLD_WAND:
                return "gold wand";
            case WandDescriptionId_PLASTIC_WAND:
                return "plastic wand";
            case WandDescriptionId_COPPER_WAND:
                return "copper wand";
            case WandDescriptionId_PURPLE_WAND:
                return "purple wand";

            case WandDescriptionId_UNSEEN:
                return "wand";

            case WandDescriptionId_COUNT:
                panic("not a real description id");
        }
        panic("description id");
    }
}
static const char * get_potion_description_str(Thing observer, PerceivedThing item) {
    PotionDescriptionId description_id = item->potion_info()->description_id;
    PotionId true_id = description_id != PotionDescriptionId_UNSEEN ? observer->life()->knowledge.potion_identities[description_id] : PotionId_UNKNOWN;
    if (true_id != PotionId_UNKNOWN) {
        switch (true_id) {
            case PotionId_POTION_OF_HEALING:
                return "potion of healing";
            case PotionId_POTION_OF_POISON:
                return "potion of poison";
            case PotionId_POTION_OF_ETHEREAL_VISION:
                return "potion of ethereal vision";
            case PotionId_POTION_OF_COGNISCOPY:
                return "potion of cogniscopy";
            case PotionId_POTION_OF_BLINDNESS:
                return "potion of blindness";
            case PotionId_POTION_OF_INVISIBILITY:
                return "potion of invisibility";

            case PotionId_COUNT:
            case PotionId_UNKNOWN:
                panic("not a real id");
        }
        panic("item id");
    } else {
        switch (description_id) {
            case PotionDescriptionId_BLUE_POTION:
                return "blue potion";
            case PotionDescriptionId_GREEN_POTION:
                return "green potion";
            case PotionDescriptionId_RED_POTION:
                return "red potion";
            case PotionDescriptionId_YELLOW_POTION:
                return "yellow potion";
            case PotionDescriptionId_ORANGE_POTION:
                return "orange potion";
            case PotionDescriptionId_PURPLE_POTION:
                return "purple potion";

            case PotionDescriptionId_UNSEEN:
                return "potion";

            case PotionDescriptionId_COUNT:
                panic("not a real description id");
        }
        panic("description id");
    }
}
Span get_thing_description(Thing observer, uint256 target_id) {
    if (observer->id == target_id)
        return new_span("you", light_blue, black);

    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id);

    Span result = new_span("a ");
    result->append(get_status_description(target->status_effects));

    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            result->append(get_species_name(target->life()->species_id));
            return result;
        case ThingType_WAND:
            result->append(new_span(get_wand_description_str(observer, target), light_green, black));
            return result;
        case ThingType_POTION:
            result->append(new_span(get_potion_description_str(observer, target), light_green, black));
            return result;
    }
    panic("thing type");
}

static void popup_help(SDL_Rect area, Coord tile_in_area, Div div) {
    Coord upper_left_corner = Coord{area.x, area.y} + Coord{tile_in_area.x * tile_size, tile_in_area.y * tile_size};
    Coord lower_right_corner = upper_left_corner + Coord{tile_size, tile_size};
    int horizontal_align = upper_left_corner.x < entire_window_area.w/2 ? 1 : -1;
    int vertical_align = upper_left_corner.y < entire_window_area.h/2 ? 1 : -1;
    SDL_Rect rect;
    if (horizontal_align < 0) {
        rect.x = 0;
        rect.w = upper_left_corner.x;
    } else {
        rect.x = lower_right_corner.x;
        rect.w = entire_window_area.w - lower_right_corner.x;
    }
    if (vertical_align < 0) {
        rect.y = 0;
        rect.h = upper_left_corner.y;
    } else {
        rect.y = lower_right_corner.y;
        rect.h = entire_window_area.h - lower_right_corner.y;
    }
    render_div(div, rect, horizontal_align, vertical_align);
}

template<typename T>
static RuckSackImage * get_image_for_thing(Reference<T> thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            if (thing->life()->species_id == SpeciesId_UNSEEN)
                return unseen_individual_image;
            return species_images[thing->life()->species_id];
        case ThingType_WAND:
            if (thing->wand_info()->description_id == WandDescriptionId_UNSEEN)
                return unseen_wand_image;
            return wand_images[thing->wand_info()->description_id];
        case ThingType_POTION:
            if (thing->potion_info()->description_id == PotionDescriptionId_UNSEEN)
                return unseen_potion_image;
            return potion_images[thing->potion_info()->description_id];
    }
    panic("thing type");
}
static RuckSackImage * get_image_for_tile(Tile tile) {
    switch (tile.tile_type) {
        case TileType_UNKNOWN:
            return nullptr;
        case TileType_DIRT_FLOOR:
            return dirt_floor_images[tile.aesthetic_index % dirt_floor_images.length()];
        case TileType_MARBLE_FLOOR:
            return marble_floor_images[tile.aesthetic_index % marble_floor_images.length()];
        case TileType_WALL:
        case TileType_BORDER_WALL:
            return wall_images[tile.aesthetic_index % wall_images.length()];
        case TileType_STAIRS_DOWN:
            return stairs_down_image;
        case TileType_COUNT:
            panic("not a real tile type");
    }
    panic("tile type");
}

static int previous_events_length = 0;
static int previous_event_forget_counter = 0;
static uint256 previous_spectator_id = uint256::zero();
static Div tutorial_div = new_div();
static Div version_div = new_div();
static Div events_div = new_div();
static Div hp_div = new_div();
static Div xp_div = new_div();
static Div dungeon_level_div = new_div();
static Div status_div = new_div();
static Div time_div = new_div();
static Div keyboard_hover_div = new_div();
static Div mouse_hover_div = new_div();
static Div inventory_menu_div = new_div();
static List<Action::Id> last_inventory_menu_items;
static int last_inventory_menu_cursor;
static Div floor_menu_div = new_div();
static List<Action> last_floor_menu_actions;
static int last_floor_menu_cursor;

static Div get_tutorial_div_content(Thing spectate_from, const List<Thing> & my_inventory) {
    List<const char *> lines;
    if (!youre_still_alive) {
        lines.append("Alt+F4: quit");
    } else {
        switch (input_mode) {
            case InputMode_MAIN: {
                List<Action> floor_actions;
                get_floor_actions(spectate_from, &floor_actions);

                lines.append("numpad: move/attack");
                if (my_inventory.length() > 0)
                    lines.append("Tab: inventory...");
                if (floor_actions.length() == 0) {
                } else if (floor_actions.length() == 1) {
                    Action::Id action_id = floor_actions[0].id;
                    if (action_id == Action::PICKUP) {
                        lines.append("numpad 5: pick up");
                    } else if (action_id == Action::GO_DOWN) {
                        lines.append("numpad 5: go down");
                    } else {
                        unreachable();
                    }
                } else {
                    lines.append("numpad 5: action...");
                }
                break;
            }
            case InputMode_INVENTORY_CHOOSE_ITEM:
                lines.append("numpad: move cursor");
                lines.append("Tab: action...");
                lines.append("Esc: cancel");
                break;
            case InputMode_INVENTORY_CHOOSE_ACTION:
                lines.append("numpad: move cursor");
                switch (inventory_menu_items[inventory_menu_cursor]) {
                    case Action::DROP:
                    case Action::QUAFF:
                        lines.append("Tab: accept");
                        break;
                    case Action::THROW:
                    case Action::ZAP:
                        lines.append("Tab: accept...");
                        break;

                    case Action::WAIT:
                    case Action::MOVE:
                    case Action::ATTACK:
                    case Action::PICKUP:
                    case Action::GO_DOWN:
                    case Action::CHEATCODE_HEALTH_BOOST:
                    case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
                    case Action::CHEATCODE_POLYMORPH:
                    case Action::CHEATCODE_GENERATE_MONSTER:
                    case Action::CHEATCODE_CREATE_ITEM:
                    case Action::CHEATCODE_IDENTIFY:
                    case Action::CHEATCODE_GO_DOWN:
                    case Action::CHEATCODE_GAIN_LEVEL:
                    case Action::COUNT:
                    case Action::UNDECIDED:
                        unreachable();
                }
                lines.append("Esc: back");
                break;
            case InputMode_FLOOR_CHOOSE_ACTION:
                lines.append("numpad: move cursor");
                lines.append("Tab: accept");
                lines.append("Esc: back");
                break;
            case InputMode_THROW_CHOOSE_DIRECTION:
            case InputMode_ZAP_CHOOSE_DIRECTION:
                lines.append("numpad: direction");
                lines.append("numpad 5: yourself");
                lines.append("Esc: cancel");
                break;
        }
    }
    lines.append("mouse: what's this");

    Div div = new_div();
    for (int i = 0; i < lines.length(); i++) {
        if (i > 0)
            div->append_newline();
        div->append(new_span(lines[i]));
    }
    return div;
}
static Span render_percent(int numerator, int denominator) {
    numerator = clamp(numerator, 0, denominator);
    String string = new_string();
    string->format("%d%s", numerator * 100 / denominator, "%");
    Span span = new_span(string);
    if (numerator < denominator)
        span->set_color(amber, black);
    else
        span->set_color(white, dark_green);
    return span;
}
static Div get_time_display(Thing spectate_from) {
    Div result = new_div();
    Life * life = spectate_from->life();
    int movement_cost = get_movement_cost(spectate_from);
    if (!(movement_cost <= action_cost && life->last_movement_time + movement_cost <= time_counter)) {
        Span movement_span = new_span();
        movement_span->format("move: %s", render_percent(time_counter - life->last_movement_time, movement_cost));
        result->append(movement_span);
    }
    result->append_newline();
    {
        String time_string = new_string();
        time_string->format("time: %d", time_counter / 12);
        result->append(new_span(time_string));
    }

    return result;
}

static Uint8 get_thing_alpha(Thing observer, PerceivedThing thing) {
    if (has_status(thing, StatusEffect::INVISIBILITY))
        return 0x7f;
    if (!can_see_thing(observer, thing->id)) {
        int64_t knowledge_age = time_counter - thing->last_seen_time;
        if (knowledge_age < 12 * 10) {
            // fresh
            return 0x7f;
        } else {
            // stale
            return 0x3f;
        }
    }
    return 0xff;
}

static const char * get_action_text(Action::Id action_id) {
    switch (action_id) {
        case Action::DROP:
            return "drop";
        case Action::QUAFF:
            return "quaff";
        case Action::THROW:
            return "throw";
        case Action::ZAP:
            return "zap";

        case Action::WAIT:
        case Action::MOVE:
        case Action::ATTACK:
        case Action::PICKUP:
        case Action::GO_DOWN:
        case Action::CHEATCODE_HEALTH_BOOST:
        case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
        case Action::CHEATCODE_POLYMORPH:
        case Action::CHEATCODE_GENERATE_MONSTER:
        case Action::CHEATCODE_CREATE_ITEM:
        case Action::CHEATCODE_IDENTIFY:
        case Action::CHEATCODE_GO_DOWN:
        case Action::CHEATCODE_GAIN_LEVEL:
        case Action::COUNT:
        case Action::UNDECIDED:
            unreachable();
    }
    unreachable();
}

static Span render_action(Thing actor, Action action) {
    Span result = new_span();
    switch (action.id) {
        case Action::PICKUP:
            result->format("pick up %g%s", get_thing_description(actor, action.item()), stairs_down_image);
            return result;
        case Action::GO_DOWN:
            result->format("go down");
            return result;

        case Action::DROP:
        case Action::QUAFF:
        case Action::THROW:
        case Action::ZAP:
        case Action::WAIT:
        case Action::MOVE:
        case Action::ATTACK:
        case Action::CHEATCODE_HEALTH_BOOST:
        case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
        case Action::CHEATCODE_POLYMORPH:
        case Action::CHEATCODE_GENERATE_MONSTER:
        case Action::CHEATCODE_CREATE_ITEM:
        case Action::CHEATCODE_IDENTIFY:
        case Action::CHEATCODE_GO_DOWN:
        case Action::CHEATCODE_GAIN_LEVEL:
        case Action::COUNT:
        case Action::UNDECIDED:
            unreachable();
    }
    unreachable();
}

void render() {
    Thing spectate_from = get_spectate_individual();
    List<Thing> my_inventory;
    find_items_in_inventory(you->id, &my_inventory);

    set_color(black);
    SDL_RenderClear(renderer);

    // tutorial
    tutorial_div->set_content(get_tutorial_div_content(you, my_inventory));
    render_div(tutorial_div, tutorial_area, 1, 1);
    {
        Span blurb_span = new_span("v", gray, black);
        blurb_span->append(version_string);
        version_div->set_content(blurb_span);
        render_div(version_div, version_area, -1, -1);
    }

    // main map
    {
        int direction_distance_min = -1;
        int direction_distance_max = -1;
        switch (input_mode) {
            case InputMode_MAIN:
            case InputMode_INVENTORY_CHOOSE_ITEM:
            case InputMode_INVENTORY_CHOOSE_ACTION:
            case InputMode_FLOOR_CHOOSE_ACTION:
                break;
            case InputMode_THROW_CHOOSE_DIRECTION:
                direction_distance_min = throw_distance_average - throw_distance_error_margin;
                direction_distance_max = throw_distance_average + throw_distance_error_margin;
                break;
            case InputMode_ZAP_CHOOSE_DIRECTION:
                direction_distance_min = beam_length_average - beam_length_error_margin;
                direction_distance_max = beam_length_average + beam_length_error_margin;
                break;
        }
        // render the terrain
        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
                Tile tile = spectate_from->life()->knowledge.tiles[cursor];
                if (cheatcode_full_visibility)
                    tile = actual_map_tiles[cursor];
                RuckSackImage * image = get_image_for_tile(tile);
                if (image == nullptr)
                    continue;
                Uint8 alpha;
                if (spectate_from->life()->knowledge.tile_is_visible[cursor].any()) {
                    // it's in our direct line of sight
                    if (direction_distance_min != -1) {
                        // actually, let's only show the 8 directions
                        Coord vector = spectate_from->location - cursor;
                        if (vector.x * vector.y == 0 || abs(vector.x) == abs(vector.y)) {
                            // ordinal aligned
                            int distance = ordinal_distance(spectate_from->location, cursor);
                            if (distance <= direction_distance_min)
                                alpha = 0xff;
                            else if (distance <= direction_distance_max)
                                alpha = 0xaf;
                            else
                                alpha = 0x7f;
                        } else {
                            alpha = 0x7f;
                        }
                    } else {
                        alpha = 0xff;
                    }
                } else {
                    alpha = 0x7f;
                }
                render_tile(renderer, sprite_sheet_texture, image, alpha, cursor);
            }
        }
    }

    // render the things
    if (!cheatcode_full_visibility) {
        // not cheating
        List<PerceivedThing> things;
        PerceivedThing thing;
        for (auto iterator = spectate_from->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
            if (thing->location == Coord::nowhere())
                continue;
            things.append(thing);
        }
        sort<PerceivedThing, compare_perceived_things_by_type_and_z_order>(things.raw(), things.length());
        // only render 1 of each type of thing in each location on the map
        MapMatrix<bool> item_pile_rendered;
        item_pile_rendered.set_all(false);
        for (int i = 0; i < things.length(); i++) {
            PerceivedThing thing = things[i];
            if (thing->thing_type != ThingType_INDIVIDUAL) {
                if (item_pile_rendered[thing->location])
                    continue;
                item_pile_rendered[thing->location] = true;
            }
            Uint8 alpha = get_thing_alpha(spectate_from, thing);
            render_tile(renderer, sprite_sheet_texture, get_image_for_thing(thing), alpha, thing->location);

            List<PerceivedThing> inventory;
            find_items_in_inventory(spectate_from, thing, &inventory);
            if (inventory.length() > 0)
                render_tile(renderer, sprite_sheet_texture, equipment_image, alpha, thing->location);
        }
    } else {
        // full visibility
        Thing thing;
        for (auto iterator = actual_things.value_iterator(); iterator.next(&thing);) {
            // this exposes hashtable iteration order, but it's a cheatcode, so whatever.
            if (!thing->still_exists)
                continue;
            if (thing->location == Coord::nowhere())
                continue;
            Uint8 alpha;
            if (has_status(thing, StatusEffect::INVISIBILITY)|| !spectate_from->life()->knowledge.tile_is_visible[thing->location].any())
                alpha = 0x7f;
            else
                alpha = 0xff;
            render_tile(renderer, sprite_sheet_texture, get_image_for_thing(thing), alpha, thing->location);

            List<Thing> inventory;
            find_items_in_inventory(thing->id, &inventory);
            if (inventory.length() > 0)
                render_tile(renderer, sprite_sheet_texture, equipment_image, alpha, thing->location);
        }
    }

    // status box
    {
        String hp_string = new_string();
        int hp = spectate_from->life()->hitpoints;
        int max_hp = spectate_from->life()->max_hitpoints();
        hp_string->format("HP: %d/%d", hp, max_hp);
        Span hp_span = new_span(hp_string);
        if (hp <= max_hp / 3)
            hp_span->set_color(white, red);
        else if (hp < max_hp)
            hp_span->set_color(black, amber);
        else
            hp_span->set_color(white, dark_green);
        hp_div->set_content(hp_span);
        render_div(hp_div, hp_area, 1, 1);
    }
    {
        Div div = new_div();
        String string = new_string();
        string->format("XP Level: %d", spectate_from->life()->experience_level());
        div->append(new_span(string));
        div->append_newline();
        string->clear();
        string->format("XP:       %d/%d", spectate_from->life()->experience, spectate_from->life()->next_level_up());
        div->append(new_span(string));
        xp_div->set_content(div);
        render_div(xp_div, xp_area, 1, 1);
    }
    {
        String string = new_string();
        string->format("Dungeon Level: %d", dungeon_level);
        dungeon_level_div->set_content(new_span(string));
        render_div(dungeon_level_div, dungeon_level_area, 1, 1);
    }
    {
        status_div->set_content(get_status_description(spectate_from->status_effects));
        render_div(status_div, status_area, 1, 1);
    }
    {
        time_div->set_content(get_time_display(spectate_from));
        render_div(time_div, time_area, 1, 1);
    }

    // message area
    {
        bool expand_message_box = rect_contains(message_area, get_mouse_pixels());
        List<RememberedEvent> & events = spectate_from->life()->knowledge.remembered_events;
        bool refresh_events = previous_event_forget_counter != spectate_from->life()->knowledge.event_forget_counter || previous_spectator_id != spectate_from->id;
        if (refresh_events) {
            previous_events_length = 0;
            previous_event_forget_counter = spectate_from->life()->knowledge.event_forget_counter;
            previous_spectator_id = spectate_from->id;
            events_div->clear();
        }
        for (int i = previous_events_length; i < events.length(); i++) {
            RememberedEvent event = events[i];
            if (event != nullptr) {
                // append something
                if (i > 0) {
                    // maybe sneak in a delimiter
                    if (events[i - 1] == nullptr)
                        events_div->append_newline();
                    else
                        events_div->append_spaces(2);
                }
                events_div->append(event->span);
            }
        }
        previous_events_length = events.length();
        if (expand_message_box) {
            // truncate from the bottom
            events_div->set_max_size(entire_window_area.w, entire_window_area.h);
            SDL_Texture * texture = events_div->get_texture(renderer);
            if (texture != nullptr) {
                SDL_Rect source_rect = get_texture_bounds(texture);
                int overflow = source_rect.h - entire_window_area.h;
                if (overflow > 0) {
                    source_rect.y += overflow;
                    source_rect.h -= overflow;
                }
                render_texture(texture, source_rect, entire_window_area, 1, 1);
            }
        } else {
            render_div(events_div, message_area, 1, -1);
        }
    }

    // inventory pane
    {
        bool render_popup_help = false;
        bool render_action_menu = false;
        SDL_Color inventory_cursor_color = SDL_Color{0, 0, 0, 0};
        switch (input_mode) {
            case InputMode_MAIN:
            case InputMode_FLOOR_CHOOSE_ACTION:
                break;
            case InputMode_INVENTORY_CHOOSE_ITEM:
                render_popup_help = true;
                inventory_cursor_color = amber;
                break;
            case InputMode_INVENTORY_CHOOSE_ACTION:
                render_action_menu = true;
                inventory_cursor_color = gray;
                break;
            case InputMode_THROW_CHOOSE_DIRECTION:
            case InputMode_ZAP_CHOOSE_DIRECTION:
                inventory_cursor_color = gray;
                break;
        }
        // cursor is background
        if (inventory_cursor_color.a != 0) {
            Coord cursor_location = inventory_index_to_location(inventory_cursor);
            SDL_Rect cursor_rect;
            cursor_rect.x = inventory_area.x + cursor_location.x * tile_size;
            cursor_rect.y = inventory_area.y + cursor_location.y * tile_size;
            cursor_rect.w = tile_size;
            cursor_rect.h = tile_size;
            set_color(inventory_cursor_color);
            SDL_RenderFillRect(renderer, &cursor_rect);
        }
        for (int i = 0; i < my_inventory.length(); i++) {
            Coord location = inventory_index_to_location(i);
            location.x += map_size.x;
            Thing item = my_inventory[i];
            render_tile(renderer, sprite_sheet_texture, get_image_for_thing(item), 0xff, location);
        }
        // popup help
        if (render_popup_help) {
            keyboard_hover_div->set_content(get_thing_description(you, my_inventory[inventory_cursor]->id));
            popup_help(inventory_area, inventory_index_to_location(inventory_cursor), keyboard_hover_div);
        }
        // action menu
        if (render_action_menu) {
            if (!(last_inventory_menu_items == inventory_menu_items && last_inventory_menu_cursor == inventory_menu_cursor)) {
                // rerender the text
                last_inventory_menu_cursor = inventory_menu_cursor;
                last_inventory_menu_items.clear();
                last_inventory_menu_items.append_all(inventory_menu_items);

                inventory_menu_div->clear();
                for (int i = 0; i < inventory_menu_items.length(); i++) {
                    if (i > 0)
                        inventory_menu_div->append_newline();
                    Span item_span = new_span(get_action_text(inventory_menu_items[i]));
                    if (i == inventory_menu_cursor) {
                        item_span->set_color(black, amber);
                    } else {
                        item_span->set_color(white, black);
                    }
                    inventory_menu_div->append(item_span);
                }
            }
            popup_help(inventory_area, inventory_index_to_location(inventory_cursor), inventory_menu_div);
        }
    }

    {
        bool show_floor_menu = false;
        bool show_map_popup = true;
        switch (input_mode) {
            case InputMode_MAIN:
            case InputMode_INVENTORY_CHOOSE_ITEM:
            case InputMode_INVENTORY_CHOOSE_ACTION:
            case InputMode_THROW_CHOOSE_DIRECTION:
            case InputMode_ZAP_CHOOSE_DIRECTION:
                break;
            case InputMode_FLOOR_CHOOSE_ACTION:
                show_floor_menu = true;
                show_map_popup = false;
                break;
        }
        // floor action menu
        if (show_floor_menu) {
            List<Action> floor_actions;
            get_floor_actions(spectate_from, &floor_actions);
            if (!(last_floor_menu_actions == floor_actions && last_floor_menu_cursor == floor_menu_cursor)) {
                // rerender floor menu div
                last_floor_menu_actions.clear();
                last_floor_menu_actions.append_all(floor_actions);
                last_floor_menu_cursor = floor_menu_cursor;

                floor_menu_div->clear();
                for (int i = 0; i < floor_actions.length(); i++) {
                    if (i > 0)
                        floor_menu_div->append_newline();
                    Span item_span = render_action(spectate_from, floor_actions[i]);
                    if (i == floor_menu_cursor) {
                        item_span->set_color(black, amber);
                    } else {
                        item_span->set_color(white, black);
                    }
                    floor_menu_div->append(item_span);
                }
            }
            popup_help(main_map_area, spectate_from->location, floor_menu_div);
        }
        // popup help for hovering over things
        if (show_map_popup) {
            Coord mouse_hover_map_tile = get_mouse_tile(main_map_area);
            if (mouse_hover_map_tile != Coord::nowhere()) {
                List<PerceivedThing> things;
                find_perceived_things_at(spectate_from, mouse_hover_map_tile, &things);
                if (things.length() != 0) {
                    Div content = new_div();
                    for (int i = 0; i < things.length(); i++) {
                        PerceivedThing target = things[i];
                        if (i > 0 )
                            content->append_newline();
                        Span thing_and_carrying = new_span();
                        thing_and_carrying->append(get_thing_description(spectate_from, target->id));
                        List<PerceivedThing> inventory;
                        find_items_in_inventory(spectate_from, target, &inventory);
                        if (inventory.length() > 0) {
                            thing_and_carrying->append(" carrying:");
                            content->append(thing_and_carrying);
                            for (int j = 0; j < inventory.length(); j++) {
                                content->append_newline();
                                content->append_spaces(4);
                                content->append(get_thing_description(spectate_from, inventory[j]->id));
                            }
                        } else {
                            content->append(thing_and_carrying);
                        }
                    }
                    mouse_hover_div->set_content(content);
                    popup_help(main_map_area, mouse_hover_map_tile, mouse_hover_div);
                }
            }
            Coord mouse_hover_inventory_tile = get_mouse_tile(inventory_area);
            if (0 <= mouse_hover_inventory_tile.x && mouse_hover_inventory_tile.x <= inventory_layout_width && 0 <= mouse_hover_inventory_tile.y) {
                int inventory_index = inventory_location_to_index(mouse_hover_inventory_tile);
                if (inventory_index < my_inventory.length()) {
                    mouse_hover_div->set_content(get_thing_description(you, my_inventory[inventory_index]->id));
                    popup_help(inventory_area, mouse_hover_inventory_tile, mouse_hover_div);
                }
            }
        }
    }

    SDL_RenderPresent(renderer);
}
