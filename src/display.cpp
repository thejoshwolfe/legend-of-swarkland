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
#include "decision.hpp"
#include "spritesheet.hpp"
#include "audio.hpp"

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
static const SDL_Rect inventory_area = { main_map_area.x + main_map_area.w, 2 * tile_size, inventory_layout_width * tile_size, (map_size.y - 8) * tile_size };
static const SDL_Rect ability_area = { inventory_area.x, inventory_area.y + inventory_area.h, inventory_layout_width * tile_size, 4 * tile_size };
static const SDL_Rect tutorial_area = { ability_area.x, ability_area.y + ability_area.h, inventory_layout_width * tile_size, 4 * tile_size };
static const SDL_Rect version_area = { status_box_area.x + status_box_area.w, status_box_area.y, inventory_layout_width * tile_size, tile_size };
static const SDL_Rect entire_window_area = { 0, 0, inventory_area.x + inventory_area.w, status_box_area.y + status_box_area.h };

static SDL_Window * window;
SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;
static SDL_Texture * screen_buffer;
// this changes when the window resizes
static SDL_Rect output_rect = entire_window_area;

static const SwarklandImage_ dirt_floor_images[] = {
    sprite_location_dirt_floor0,
    sprite_location_dirt_floor1,
    sprite_location_dirt_floor2,
    sprite_location_dirt_floor3,
    sprite_location_dirt_floor4,
    sprite_location_dirt_floor5,
    sprite_location_dirt_floor6,
    sprite_location_dirt_floor7,
};
static SwarklandImage_ marble_floor_images[] = {
    sprite_location_marble_floor0,
    sprite_location_marble_floor1,
    sprite_location_marble_floor2,
    sprite_location_marble_floor3,
    sprite_location_marble_floor4,
    sprite_location_marble_floor5,
};
static SwarklandImage_ brown_brick_wall_images[] = {
    sprite_location_brown_brick0,
    sprite_location_brown_brick1,
    sprite_location_brown_brick2,
    sprite_location_brown_brick3,
    sprite_location_brown_brick4,
    sprite_location_brown_brick5,
    sprite_location_brown_brick6,
    sprite_location_brown_brick7,
};
static SwarklandImage_ gray_brick_wall_images[] = {
    sprite_location_gray_brick0,
    sprite_location_gray_brick1,
    sprite_location_gray_brick2,
    sprite_location_gray_brick3,
};
static SwarklandImage_ species_images[SpeciesId_COUNT];
static SwarklandImage_ wand_images[WandDescriptionId_COUNT];
static SwarklandImage_ potion_images[PotionDescriptionId_COUNT];
static SwarklandImage_ book_images[BookDescriptionId_COUNT];

TTF_Font * status_box_font;
static SDL_RWops *font_rw_ops;
static String version_string = new_string();

#define fill_with_trash(array) \
    for (int i = 0; i < (int)(get_array_length(array)); i++) \
        array[i] = SwarklandImage_::nowhere()
#define check_no_trash(array) \
    for (int i = 0; i < (int)(get_array_length(array)); i++) \
        assert_str(array[i] != SwarklandImage_::nowhere(), "missed a spot")

static void load_images() {
    // TODO: do all this at compile time
    fill_with_trash(species_images);
    species_images[SpeciesId_HUMAN] = sprite_location_human;
    species_images[SpeciesId_OGRE] = sprite_location_ogre;
    species_images[SpeciesId_LICH] = sprite_location_lich;
    species_images[SpeciesId_SHAPESHIFTER] = sprite_location_shapeshifter;
    species_images[SpeciesId_DOG] = sprite_location_dog;
    species_images[SpeciesId_PINK_BLOB] = sprite_location_pink_blob;
    species_images[SpeciesId_AIR_ELEMENTAL] = sprite_location_air_elemental;
    species_images[SpeciesId_TAR_ELEMENTAL] = sprite_location_tar_elemental;
    species_images[SpeciesId_ANT] = sprite_location_ant;
    species_images[SpeciesId_BEE] = sprite_location_bee;
    species_images[SpeciesId_BEETLE] = sprite_location_beetle;
    species_images[SpeciesId_SCORPION] = sprite_location_scorpion;
    species_images[SpeciesId_SNAKE] = sprite_location_snake;
    species_images[SpeciesId_COBRA] = sprite_location_cobra;
    check_no_trash(species_images);

    fill_with_trash(wand_images);
    wand_images[WandDescriptionId_BONE_WAND] = sprite_location_bone_wand;
    wand_images[WandDescriptionId_GOLD_WAND] = sprite_location_gold_wand;
    wand_images[WandDescriptionId_PLASTIC_WAND] = sprite_location_plastic_wand;
    wand_images[WandDescriptionId_COPPER_WAND] = sprite_location_copper_wand;
    wand_images[WandDescriptionId_PURPLE_WAND] = sprite_location_purple_wand;
    wand_images[WandDescriptionId_SHINY_BONE_WAND] = sprite_location_shiny_bone_wand;
    wand_images[WandDescriptionId_SHINY_GOLD_WAND] = sprite_location_shiny_gold_wand;
    wand_images[WandDescriptionId_SHINY_PLASTIC_WAND] = sprite_location_shiny_plastic_wand;
    wand_images[WandDescriptionId_SHINY_COPPER_WAND] = sprite_location_shiny_copper_wand;
    wand_images[WandDescriptionId_SHINY_PURPLE_WAND] = sprite_location_shiny_purple_wand;
    check_no_trash(wand_images);

    fill_with_trash(potion_images);
    potion_images[PotionDescriptionId_BLUE_POTION] = sprite_location_blue_potion;
    potion_images[PotionDescriptionId_GREEN_POTION] = sprite_location_green_potion;
    potion_images[PotionDescriptionId_RED_POTION] = sprite_location_red_potion;
    potion_images[PotionDescriptionId_YELLOW_POTION] = sprite_location_yellow_potion;
    potion_images[PotionDescriptionId_ORANGE_POTION] = sprite_location_orange_potion;
    potion_images[PotionDescriptionId_PURPLE_POTION] = sprite_location_purple_potion;
    check_no_trash(potion_images);

    fill_with_trash(book_images);
    book_images[BookDescriptionId_PURPLE_BOOK] = sprite_location_purple_book;
    book_images[BookDescriptionId_BLUE_BOOK] = sprite_location_blue_book;
    book_images[BookDescriptionId_RED_BOOK] = sprite_location_red_book;
    book_images[BookDescriptionId_GREEN_BOOK] = sprite_location_green_book;
    book_images[BookDescriptionId_YELLOW_BOOK] = sprite_location_yellow_book;
    check_no_trash(book_images);
}

void init_display() {
    assert(!headless_mode);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        panic("unable to init SDL");

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
        panic("window create failed");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
        panic("renderer create failed");

    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(renderer, &renderer_info);
    if (!(renderer_info.flags & SDL_RENDERER_TARGETTEXTURE))
        panic("rendering to a temporary texture is not supported");

    screen_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, entire_window_area.w, entire_window_area.h);

    sprite_sheet_texture = load_texture(renderer);

    load_images();
    if (false) load_images();

    TTF_Init();

    font_rw_ops = SDL_RWFromMem((void *)get_binary_font_resource_start(), (int)get_binary_font_resource_size());
    if (font_rw_ops == nullptr)
        panic("sdl rwops fail");
    status_box_font = TTF_OpenFontRW(font_rw_ops, 0, 13);
    TTF_SetFontHinting(status_box_font, TTF_HINTING_LIGHT);

    ByteBuffer buffer;
    buffer.append((const char *)get_binary_version_resource_start(), (int)get_binary_version_resource_size());
    version_string->decode(buffer);

    SDL_AudioSpec want;
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 4096;
    want.callback = audio_callback;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &want, 0);
    if (audio_device == 0) {
        panic(SDL_GetError());
    }
    SDL_PauseAudioDevice(audio_device, 0);
}

void display_finish() {
    assert(!headless_mode);

    TTF_Quit();

    SDL_RWclose(font_rw_ops);

    SDL_DestroyTexture(sprite_sheet_texture);
    SDL_DestroyTexture(screen_buffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

static inline bool rect_contains(SDL_Rect rect, Coord point) {
    return rect.x <= point.x && point.x < rect.x + rect.w &&
           rect.y <= point.y && point.y < rect.y + rect.h;
}

static void render_tile(SwarklandImage_ image, int alpha, Coord dest_coord) {
    SDL_Rect source_rect;
    source_rect.x = image.x;
    source_rect.y = image.y;
    source_rect.w = tile_size;
    source_rect.h = tile_size;

    SDL_Rect dest_rect;
    dest_rect.x = main_map_area.x + dest_coord.x * tile_size;
    dest_rect.y = main_map_area.y + dest_coord.y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
    SDL_RenderCopy(renderer, sprite_sheet_texture, &source_rect, &dest_rect);
}

static void fill_rect(SDL_Color color, SDL_Rect * dest_rect) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, dest_rect);
}
static void render_div(Div div, SDL_Rect output_area, int horizontal_align, int vertical_align) {
    div->set_max_width(output_area.w);
    Coord size = div->compute_size(renderer);
    Coord dest_position;
    if (horizontal_align < 0) {
        dest_position.x = output_area.x + output_area.w - size.x;
    } else {
        dest_position.x = output_area.x;
    }
    if (vertical_align < 0) {
        dest_position.y = output_area.y + output_area.h - size.y;
    } else {
        dest_position.y = output_area.y;
    }
    div->render(renderer, dest_position);
}

static Coord pixels_to_game_position(Coord pixels) {
    return Coord{
        (pixels.x - output_rect.x) * entire_window_area.w / output_rect.w,
        (pixels.y - output_rect.y) * entire_window_area.h / output_rect.h,
    };
}
static Coord get_mouse_game_position() {
    return pixels_to_game_position(get_mouse_pixels());
}

Coord get_mouse_tile(SDL_Rect area) {
    assert(!headless_mode);
    Coord pixels = get_mouse_game_position();
    if (!rect_contains(area, pixels))
        return Coord::nowhere();
    pixels.x -= area.x;
    pixels.y -= area.y;
    Coord tile_coord = {pixels.x / tile_size, pixels.y / tile_size};
    return tile_coord;
}

const char * get_species_name_str(SpeciesId species_id) {
    switch (species_id) {
        case SpeciesId_HUMAN:
            return "human";
        case SpeciesId_OGRE:
            return "ogre";
        case SpeciesId_LICH:
            return "lich king";
        case SpeciesId_SHAPESHIFTER:
            return "shapeshifter";
        case SpeciesId_PINK_BLOB:
            return "pink blob";
        case SpeciesId_AIR_ELEMENTAL:
            return "air elemental";
        case SpeciesId_TAR_ELEMENTAL:
            return "tar elemental";
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
        case SpeciesId_COBRA:
            return "cobra";

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
static Span get_status_description(const List<StatusEffect> & status_effects) {
    Span result = new_span();
    // we need the order of words to be consistent.
    List<StatusEffect> remaining_status_effects;
    remaining_status_effects.append_all(status_effects);
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::CONFUSION))
        result->append("confused ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::SPEED))
        result->append("fast ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::SLOWING))
        result->append("slow ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::ETHEREAL_VISION))
        result->append("ethereal-visioned ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::COGNISCOPY))
        result->append("cogniscopic ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::BLINDNESS))
        result->append("blind ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::POISON))
        result->append("poisoned ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::INVISIBILITY))
        result->append("invisible ");
    if (maybe_remove_status(&remaining_status_effects, StatusEffect::POLYMORPH))
        result->append("polymorphed ");
    assert_str(remaining_status_effects.length() == 0, "missed a spot");
    result->set_color(pink, black);
    return result;
}
const char * get_wand_id_str(WandId wand_id) {
    switch (wand_id) {
        case WandId_WAND_OF_CONFUSION:
            return "wand of confusion";
        case WandId_WAND_OF_DIGGING:
            return "wand of digging";
        case WandId_WAND_OF_MAGIC_MISSILE:
            return "wand of magic missile";
        case WandId_WAND_OF_SPEED:
            return "wand of speed";
        case WandId_WAND_OF_REMEDY:
            return "wand of remedy";
        case WandId_WAND_OF_BLINDING:
            return "wand of blinding";
        case WandId_WAND_OF_FORCE:
            return "wand of force";
        case WandId_WAND_OF_INVISIBILITY:
            return "wand of invisibility";
        case WandId_WAND_OF_MAGIC_BULLET:
            return "wand of magic bullet";
        case WandId_WAND_OF_SLOWING:
            return "wand of slowing";

        case WandId_UNKNOWN:
            return "wand";

        case WandId_COUNT:
            unreachable();
    }
    unreachable();
}
static const char * get_wand_description_str(Thing observer, PerceivedThing item) {
    WandDescriptionId description_id = item->wand_info()->description_id;
    WandId true_id = description_id != WandDescriptionId_UNSEEN ? observer->life()->knowledge.wand_identities[description_id] : WandId_UNKNOWN;
    if (true_id != WandId_UNKNOWN) {
        return get_wand_id_str(true_id);
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
            case WandDescriptionId_SHINY_BONE_WAND:
                return "shiny bone wand";
            case WandDescriptionId_SHINY_GOLD_WAND:
                return "shiny gold wand";
            case WandDescriptionId_SHINY_PLASTIC_WAND:
                return "shiny plastic wand";
            case WandDescriptionId_SHINY_COPPER_WAND:
                return "shiny copper wand";
            case WandDescriptionId_SHINY_PURPLE_WAND:
                return "shiny purple wand";

            case WandDescriptionId_UNSEEN:
                return "wand";

            case WandDescriptionId_COUNT:
                panic("not a real description id");
        }
        panic("description id");
    }
}
static Span get_used_count_description(int used_count) {
    if (used_count == -1)
        return new_span("(empty)", white, red);
    String string = new_string();
    string->format("(-%d)", used_count);
    Span span = new_span(string);
    span->set_color(black, amber);
    return span;
}
const char * get_potion_id_str(PotionId potion_id) {
    switch (potion_id) {
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
            unreachable();;
    }
    unreachable();;
}
const char * get_book_id_str(BookId book_id) {
    switch (book_id) {
        case BookId_SPELLBOOK_OF_MAGIC_BULLET:
            return "spellbook of magic bullet";
        case BookId_SPELLBOOK_OF_SPEED:
            return "spellbook of speed";
        case BookId_SPELLBOOK_OF_MAPPING:
            return "spellbook of mapping";
        case BookId_SPELLBOOK_OF_FORCE:
            return "spellbook of force";
        case BookId_SPELLBOOK_OF_ASSUME_FORM:
            return "spellbook of assume form";

        case BookId_COUNT:
        case BookId_UNKNOWN:
            unreachable();;
    }
    unreachable();;
}
static const char * get_potion_description_str(Thing observer, PerceivedThing item) {
    PotionDescriptionId description_id = item->potion_info()->description_id;
    PotionId true_id = description_id != PotionDescriptionId_UNSEEN ? observer->life()->knowledge.potion_identities[description_id] : PotionId_UNKNOWN;
    if (true_id != PotionId_UNKNOWN) {
        return get_potion_id_str(true_id);
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
                unreachable();
        }
        unreachable();
    }
}
static const char * get_book_description_str(Thing observer, PerceivedThing item) {
    BookDescriptionId description_id = item->book_info()->description_id;
    BookId true_id = description_id != BookDescriptionId_UNSEEN ? observer->life()->knowledge.book_identities[description_id] : BookId_UNKNOWN;
    if (true_id != BookId_UNKNOWN) {
        return get_book_id_str(true_id);
    } else {
        switch (description_id) {
            case BookDescriptionId_PURPLE_BOOK:
                return "purple book";
            case BookDescriptionId_BLUE_BOOK:
                return "blue book";
            case BookDescriptionId_RED_BOOK:
                return "red book";
            case BookDescriptionId_GREEN_BOOK:
                return "green book";
            case BookDescriptionId_YELLOW_BOOK:
                return "yellow book";

            case BookDescriptionId_UNSEEN:
                return "book";

            case BookDescriptionId_COUNT:
                unreachable();
        }
        unreachable();
    }
}
static Span get_thing_description(Thing observer, uint256 target_id, bool verbose) {
    if (observer->id == target_id)
        return new_span("you", light_blue, black);

    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id);

    Span result = new_span("a ");
    result->append(get_status_description(target->status_effects));

    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            result->append(get_species_name(target->life()->species_id));
            return result;
        case ThingType_WAND: {
            result->append(new_span(get_wand_description_str(observer, target), light_green, black));
            int used_count = target->wand_info()->used_count;
            if (verbose && used_count != 0) {
                result->append(" ");
                result->append(get_used_count_description(used_count));
            }
            return result;
        }
        case ThingType_POTION:
            result->append(new_span(get_potion_description_str(observer, target), light_green, black));
            return result;
        case ThingType_BOOK:
            result->append(new_span(get_book_description_str(observer, target), light_green, black));
            return result;

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
Span get_thing_description(Thing observer, uint256 target_id) {
    return get_thing_description(observer, target_id, false);
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

static SwarklandImage_ get_image_for_thing(PerceivedThing thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            if (thing->life()->species_id == SpeciesId_UNSEEN)
                return sprite_location_unseen_individual;
            return species_images[thing->life()->species_id];
        case ThingType_WAND:
            if (thing->wand_info()->description_id == WandDescriptionId_UNSEEN)
                return sprite_location_unseen_wand;
            return wand_images[thing->wand_info()->description_id];
        case ThingType_POTION:
            if (thing->potion_info()->description_id == PotionDescriptionId_UNSEEN)
                return sprite_location_unseen_potion;
            return potion_images[thing->potion_info()->description_id];
        case ThingType_BOOK:
            if (thing->book_info()->description_id == BookDescriptionId_UNSEEN)
                return sprite_location_unseen_book;
            return book_images[thing->book_info()->description_id];

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
static SwarklandImage_ get_image_for_thing(Thing thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            return species_images[thing->physical_species_id()];
        case ThingType_WAND:
            return wand_images[game->actual_wand_descriptions[thing->wand_info()->wand_id]];
        case ThingType_POTION:
            return potion_images[game->actual_potion_descriptions[thing->potion_info()->potion_id]];
        case ThingType_BOOK:
            return book_images[game->actual_book_descriptions[thing->book_info()->book_id]];

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
static SwarklandImage_ get_image_for_tile(TileType tile_type, uint32_t aesthetic_index) {
    switch (tile_type) {
        case TileType_UNKNOWN:
            return SwarklandImage_::nowhere();
        case TileType_DIRT_FLOOR:
            return dirt_floor_images[aesthetic_index % get_array_length(dirt_floor_images)];
        case TileType_MARBLE_FLOOR:
            return marble_floor_images[aesthetic_index % get_array_length(marble_floor_images)];
        case TileType_UNKNOWN_FLOOR:
            return sprite_location_unknown_floor;
        case TileType_BROWN_BRICK_WALL:
        case TileType_BORDER_WALL:
            return brown_brick_wall_images[aesthetic_index % get_array_length(brown_brick_wall_images)];
        case TileType_GRAY_BRICK_WALL:
            return gray_brick_wall_images[aesthetic_index % get_array_length(gray_brick_wall_images)];
        case TileType_UNKNOWN_WALL:
            return sprite_location_unknown_wall;
        case TileType_STAIRS_DOWN:
            return sprite_location_stairs_down;
        case TileType_COUNT:
            unreachable();
    }
    unreachable();
}
static SwarklandImage_ get_image_for_ability(AbilityId ability_id) {
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            return sprite_location_cobra_venom;
        case AbilityId_THROW_TAR:
            return sprite_location_tar_elemental_throw;
        case AbilityId_ASSUME_FORM:
            return sprite_location_shapeshifter;
        case AbilityId_COUNT:
            unreachable();
    }
    unreachable();
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
static Div cheatcode_polymorph_choose_species_menu_div = new_div();
static int last_cheatcode_polymorph_choose_species_menu_cursor = -1;
static Div cheatcode_wish_choose_thing_type_menu_div = new_div();
static int last_cheatcode_wish_choose_thing_type_menu_cursor = -1;
static Div cheatcode_wish_choose_wand_id_menu_div = new_div();
static int last_cheatcode_wish_choose_wand_id_menu_cursor = -1;
static Div cheatcode_wish_choose_potion_id_menu_div = new_div();
static int last_cheatcode_wish_choose_potion_id_menu_cursor = -1;
static Div cheatcode_wish_choose_book_id_menu_div = new_div();
static int last_cheatcode_wish_choose_book_id_menu_cursor = -1;
static Div cheatcode_generate_monster_choose_species_menu_div = new_div();
static int last_cheatcode_generate_monster_choose_species_menu_cursor = -1;
static Div cheatcode_generate_monster_choose_decision_maker_menu_div = new_div();
static int last_cheatcode_generate_monster_choose_decision_maker_menu_cursor = -1;

static Div get_tutorial_div_content(Thing spectate_from, bool has_inventory, bool has_abilities) {
    List<const char *> lines;
    if (!you()->still_exists) {
        lines.append("Alt+F4: quit");
    } else {
        switch (input_mode) {
            case InputMode_MAIN: {
                List<Action> floor_actions;
                get_floor_actions(spectate_from, &floor_actions);

                lines.append("qweadzxc: move/hit");
                if (has_inventory)
                    lines.append("Tab: inventory...");
                if (has_abilities)
                    lines.append("v: ability...");
                if (floor_actions.length() == 0) {
                } else if (floor_actions.length() == 1) {
                    Action::Id action_id = floor_actions[0].id;
                    if (action_id == Action::PICKUP) {
                        lines.append("s: pick up");
                    } else if (action_id == Action::GO_DOWN) {
                        lines.append("s: go down");
                    } else {
                        unreachable();
                    }
                } else {
                    lines.append("s: action...");
                }

                List<uint256> scary_individuals;
                List<StatusEffect::Id> annoying_status_effects;
                bool stop_for_other_reasons;
                assess_auto_wait_situation(&scary_individuals, &annoying_status_effects, &stop_for_other_reasons);
                if (scary_individuals.length() == 0 && annoying_status_effects.length() > 0) {
                    if (current_player_decision.id == Action::AUTO_WAIT) {
                        // in the middle of an auto wait
                        lines.append("Esc: cancel");
                    } else {
                        // TODO: talk about the reasons somehow
                        lines.append("r: rest");
                    }
                }
                break;
            }
            case InputMode_INVENTORY_CHOOSE_ITEM:
            case InputMode_CHOOSE_ABILITY:
                lines.append("qweadzxc: move cursor");
                lines.append("Tab/s: action...");
                lines.append("Esc: cancel");
                break;
            case InputMode_INVENTORY_CHOOSE_ACTION:
                lines.append("w/x: move cursor");
                switch (inventory_menu_items[inventory_menu_cursor]) {
                    case Action::DROP:
                    case Action::QUAFF:
                        lines.append("Tab/s: accept");
                        break;
                    case Action::ZAP:
                    case Action::READ_BOOK:
                    case Action::THROW:
                        lines.append("Tab/s: accept...");
                        break;

                    case Action::WAIT:
                    case Action::MOVE:
                    case Action::ATTACK:
                    case Action::PICKUP:
                    case Action::GO_DOWN:
                    case Action::ABILITY:
                    case Action::CHEATCODE_HEALTH_BOOST:
                    case Action::CHEATCODE_KILL:
                    case Action::CHEATCODE_POLYMORPH:
                    case Action::CHEATCODE_GENERATE_MONSTER:
                    case Action::CHEATCODE_WISH:
                    case Action::CHEATCODE_IDENTIFY:
                    case Action::CHEATCODE_GO_DOWN:
                    case Action::CHEATCODE_GAIN_LEVEL:
                    case Action::COUNT:
                    case Action::UNDECIDED:
                    case Action::AUTO_WAIT:
                        unreachable();
                }
                lines.append("Esc: back");
                break;
            case InputMode_FLOOR_CHOOSE_ACTION:
            case InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES:
            case InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE:
            case InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID:
            case InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID:
            case InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID:
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES:
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER:
                lines.append("w/x: move cursor");
                lines.append("Tab/s: accept");
                lines.append("Esc: back");
                break;
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION:
                lines.append("qweadzxc: move cursor");
                lines.append("Tab/s: accept");
                lines.append("Esc: back");
                break;
            case InputMode_THROW_CHOOSE_DIRECTION:
            case InputMode_ZAP_CHOOSE_DIRECTION:
            case InputMode_READ_BOOK_CHOOSE_DIRECTION:
            case InputMode_ABILITY_CHOOSE_DIRECTION:
                lines.append("qweadzxc: direction");
                lines.append("s: yourself");
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
    if (!(movement_cost <= action_cost && life->last_movement_time + movement_cost <= game->time_counter)) {
        Span movement_span = new_span();
        movement_span->format("move: %s", render_percent(game->time_counter - life->last_movement_time, movement_cost));
        result->append(movement_span);
    }
    result->append_newline();
    {
        String time_string = new_string();
        time_string->format("time: %d", game->time_counter / 12);
        result->append(new_span(time_string));
    }

    return result;
}

static Uint8 get_thing_alpha(Thing observer, PerceivedThing thing) {
    if (has_status(thing, StatusEffect::INVISIBILITY))
        return 0x7f;
    if (!can_see_thing(observer, thing->id)) {
        int64_t knowledge_age = game->time_counter - thing->last_seen_time;
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
        case Action::READ_BOOK:
            return "cast spell";

        case Action::WAIT:
        case Action::MOVE:
        case Action::ATTACK:
        case Action::PICKUP:
        case Action::GO_DOWN:
        case Action::ABILITY:
        case Action::CHEATCODE_HEALTH_BOOST:
        case Action::CHEATCODE_KILL:
        case Action::CHEATCODE_POLYMORPH:
        case Action::CHEATCODE_GENERATE_MONSTER:
        case Action::CHEATCODE_WISH:
        case Action::CHEATCODE_IDENTIFY:
        case Action::CHEATCODE_GO_DOWN:
        case Action::CHEATCODE_GAIN_LEVEL:
        case Action::COUNT:
        case Action::UNDECIDED:
        case Action::AUTO_WAIT:
            unreachable();
    }
    unreachable();
}

static Span render_action(Thing actor, const Action & action) {
    Span result = new_span();
    switch (action.id) {
        case Action::PICKUP: {
            SwarklandImage_ image = get_image_for_thing(actor->life()->knowledge.perceived_things.get(action.item()));
            result->format("pick up %g%s", image, get_thing_description(actor, action.item(), true));
            return result;
        }
        case Action::GO_DOWN:
            result->format("go down %g", sprite_location_stairs_down);
            return result;

        case Action::DROP:
        case Action::QUAFF:
        case Action::THROW:
        case Action::ZAP:
        case Action::READ_BOOK:
        case Action::WAIT:
        case Action::MOVE:
        case Action::ATTACK:
        case Action::ABILITY:
        case Action::CHEATCODE_HEALTH_BOOST:
        case Action::CHEATCODE_KILL:
        case Action::CHEATCODE_POLYMORPH:
        case Action::CHEATCODE_GENERATE_MONSTER:
        case Action::CHEATCODE_WISH:
        case Action::CHEATCODE_IDENTIFY:
        case Action::CHEATCODE_GO_DOWN:
        case Action::CHEATCODE_GAIN_LEVEL:
        case Action::COUNT:
        case Action::UNDECIDED:
        case Action::AUTO_WAIT:
            unreachable();
    }
    unreachable();
}
static Span render_thing_type(ThingType thing_type) {
    Span result = new_span();
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
            result->format("%g%s", sprite_location_unseen_wand, new_span("wand"));
            break;
        case ThingType_POTION:
            result->format("%g%s", sprite_location_unseen_potion, new_span("potion"));
            break;
        case ThingType_BOOK:
            result->format("%g%s", sprite_location_unseen_book, new_span("book"));
            break;

        case ThingType_COUNT:
            unreachable();
    }
    return result;
}
static Span render_wand_id(WandId wand_id) {
    Span result = new_span();
    WandDescriptionId description_id = game->actual_wand_descriptions[wand_id];
    result->format("%g%s", wand_images[description_id], new_span(get_wand_id_str((WandId)wand_id)));
    return result;
}
static Span render_potion_id(PotionId potion_id) {
    Span result = new_span();
    PotionDescriptionId description_id = game->actual_potion_descriptions[potion_id];
    result->format("%g%s", potion_images[description_id], new_span(get_potion_id_str((PotionId)potion_id)));
    return result;
}
static Span render_book_id(BookId book_id) {
    Span result = new_span();
    BookDescriptionId description_id = game->actual_book_descriptions[book_id];
    result->format("%g%s", book_images[description_id], new_span(get_book_id_str((BookId)book_id)));
    return result;
}
static Span render_species(SpeciesId species_id) {
    Span result = new_span();
    result->format("%g%s", species_images[species_id], get_species_name(species_id));
    return result;
}
static Span render_decision_maker(DecisionMakerType decision_maker) {
    Span result = new_span();
    switch (decision_maker) {
        case DecisionMakerType_PLAYER:
            result->append("Player");
            break;
        case DecisionMakerType_AI:
            result->append("AI");
            break;

        case DecisionMakerType_COUNT:
            unreachable();;
    }
    return result;
}

static Span get_ability_description(AbilityId ability_id, bool is_ready) {
    Span span = new_span();
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            span->format("spit blinding venom");
            break;
        case AbilityId_THROW_TAR:
            span->format("throw tar");
            break;
        case AbilityId_ASSUME_FORM:
            span->format("assume form");
            break;
        case AbilityId_COUNT:
            unreachable();
    }
    if (!is_ready) {
        span->append(" ");
        span->append(new_span("(recharging)", black, amber));
    }
    return span;
}

void render() {
    assert(!headless_mode);

    if (cheatcode_spectator != nullptr)
        play_the_sound = true;

    Thing spectate_from = cheatcode_spectator != nullptr ? cheatcode_spectator : player_actor();
    PerceivedThing perceived_self = spectate_from->life()->knowledge.perceived_things.get(spectate_from->id);
    List<PerceivedThing> my_inventory;
    find_items_in_inventory(player_actor(), player_actor()->id, &my_inventory);
    List<AbilityId> my_abilities;
    get_abilities(player_actor(), &my_abilities);

    SDL_SetRenderTarget(renderer, screen_buffer);
    SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
    SDL_RenderClear(renderer);

    int direction_distance_min = -1;
    int direction_distance_max = -1;
    bool show_inventory_cursor_help = false;
    bool show_inventory_action_menu = false;
    bool show_inventory_cursor = false;
    SDL_Color inventory_cursor_color = SDL_Color{0, 0, 0, 0};
    bool show_ability_cursor = false;
    bool show_ability_cursor_help = false;
    SDL_Color ability_cursor_color = SDL_Color{0, 0, 0, 0};
    bool show_floor_menu = false;
    bool show_cheatcode_polymorph_choose_species_menu = false;
    bool show_cheatcode_wish_choose_thing_type_menu = false;
    bool show_cheatcode_wish_choose_wand_id_menu = false;
    bool show_cheatcode_wish_choose_potion_id_menu = false;
    bool show_cheatcode_wish_choose_book_id_menu = false;
    bool show_cheatcode_generate_monster_choose_species_menu = false;
    bool show_cheatcode_generate_monster_choose_decision_maker_menu = false;
    bool show_cheatcode_generate_monster_location_cursor = false;
    bool show_map_popup_help = false;

    switch (input_mode) {
        case InputMode_MAIN:
            show_map_popup_help = true;
            break;
        case InputMode_INVENTORY_CHOOSE_ITEM:
            show_inventory_cursor_help = true;
            show_inventory_cursor = true;
            inventory_cursor_color = amber;
            show_map_popup_help = true;
            break;
        case InputMode_CHOOSE_ABILITY:
            show_ability_cursor = true;
            show_ability_cursor_help = true;
            ability_cursor_color = amber;
            show_map_popup_help = true;
            break;
        case InputMode_INVENTORY_CHOOSE_ACTION:
            show_inventory_action_menu = true;
            show_inventory_cursor = true;
            inventory_cursor_color = gray;
            show_map_popup_help = true;
            break;
        case InputMode_ZAP_CHOOSE_DIRECTION:
        case InputMode_READ_BOOK_CHOOSE_DIRECTION:
            direction_distance_min = beam_length_average - beam_length_error_margin;
            direction_distance_max = beam_length_average + beam_length_error_margin;
            show_inventory_cursor = true;
            inventory_cursor_color = gray;
            show_map_popup_help = true;
            break;
        case InputMode_THROW_CHOOSE_DIRECTION:
            direction_distance_min = throw_distance_average - throw_distance_error_margin;
            direction_distance_max = throw_distance_average + throw_distance_error_margin;
            show_inventory_cursor = true;
            inventory_cursor_color = gray;
            show_map_popup_help = true;
            break;
        case InputMode_ABILITY_CHOOSE_DIRECTION:
            direction_distance_min = throw_distance_average - throw_distance_error_margin;
            direction_distance_max = throw_distance_average + throw_distance_error_margin;
            show_ability_cursor = true;
            ability_cursor_color = gray;
            show_map_popup_help = true;
            break;
        case InputMode_FLOOR_CHOOSE_ACTION:
            show_floor_menu = true;
            break;
        case InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES:
            show_cheatcode_polymorph_choose_species_menu = true;
            break;
        case InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE:
            show_cheatcode_wish_choose_thing_type_menu = true;
            break;
        case InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID:
            show_cheatcode_wish_choose_wand_id_menu = true;
            break;
        case InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID:
            show_cheatcode_wish_choose_potion_id_menu = true;
            break;
        case InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID:
            show_cheatcode_wish_choose_book_id_menu = true;
            break;
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES:
            show_cheatcode_generate_monster_choose_species_menu = true;
            break;
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER:
            show_cheatcode_generate_monster_choose_decision_maker_menu = true;
            break;
        case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION:
            show_cheatcode_generate_monster_location_cursor = true;
            break;
    }

    // tutorial
    tutorial_div->set_content(get_tutorial_div_content(player_actor(), my_inventory.length() > 0, my_abilities.length() > 0));
    render_div(tutorial_div, tutorial_area, 1, 1);
    {
        Span blurb_span = new_span("v", gray, black);
        blurb_span->append(version_string);
        version_div->set_content(blurb_span);
        render_div(version_div, version_area, -1, -1);
    }

    // main map
    // render the terrain
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            TileType tile = spectate_from->life()->knowledge.tiles[cursor];
            if (cheatcode_full_visibility)
                tile = game->actual_map_tiles[cursor];
            SwarklandImage_ image = get_image_for_tile(tile, game->aesthetic_indexes[cursor]);
            if (image == SwarklandImage_::nowhere())
                continue;
            Uint8 alpha;
            if (can_see_shape(spectate_from->life()->knowledge.tile_is_visible[cursor])) {
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
            render_tile(image, alpha, cursor);
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
            render_tile(get_image_for_thing(thing), alpha, thing->location);

            List<PerceivedThing> inventory;
            find_items_in_inventory(spectate_from, thing->id, &inventory);
            if (inventory.length() > 0)
                render_tile(sprite_location_equipment, alpha, thing->location);
        }
    } else {
        // full visibility
        Thing thing;
        for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);) {
            // this exposes hashtable iteration order, but it's a cheatcode, so whatever.
            if (!thing->still_exists)
                continue;
            if (thing->location == Coord::nowhere())
                continue;
            Uint8 alpha;
            if (has_status(thing, StatusEffect::INVISIBILITY) || !can_see_shape(spectate_from->life()->knowledge.tile_is_visible[thing->location]))
                alpha = 0x7f;
            else
                alpha = 0xff;
            render_tile(get_image_for_thing(thing), alpha, thing->location);

            List<Thing> inventory;
            find_items_in_inventory(thing->id, &inventory);
            if (inventory.length() > 0)
                render_tile(sprite_location_equipment, alpha, thing->location);
        }
    }

    // status box
    {
        int hp = spectate_from->life()->hitpoints;
        int max_hp = spectate_from->max_hitpoints();
        String hp_string = new_string();
        hp_string->format("HP: %d/%d", hp, max_hp);
        Span hp_span = new_span(hp_string);
        if (hp <= max_hp / 3)
            hp_span->set_color(white, red);
        else if (hp < max_hp)
            hp_span->set_color(black, amber);
        else
            hp_span->set_color(white, dark_green);
        hp_div->set_content(hp_span);

        int mana = spectate_from->life()->mana;
        int max_mana = spectate_from->max_mana();
        if (max_mana > 0) {
            String mp_string = new_string();
            mp_string->format("MP: %d/%d", mana, max_mana);
            Span mp_span = new_span(mp_string);
            if (mana <= 0)
                mp_span->set_color(white, red);
            else if (mana < max_mana)
                mp_span->set_color(black, amber);
            else
                mp_span->set_color(white, black);
            hp_div->append_newline();
            hp_div->append(mp_span);
        }
        render_div(hp_div, hp_area, 1, 1);
    }
    {
        Div div = new_div();
        String string = new_string();
        string->format("XP Level: %d", spectate_from->experience_level());
        div->append(new_span(string));
        div->append_newline();
        string->clear();
        string->format("XP:       %d/%d", spectate_from->life()->experience, spectate_from->next_level_up());
        div->append(new_span(string));
        xp_div->set_content(div);
        render_div(xp_div, xp_area, 1, 1);
    }
    {
        String string = new_string();
        string->format("Dungeon Level: %d", game->dungeon_level);
        dungeon_level_div->set_content(new_span(string));
        render_div(dungeon_level_div, dungeon_level_area, 1, 1);
    }
    {
        status_div->set_content(get_status_description(perceived_self->status_effects));
        render_div(status_div, status_area, 1, 1);
    }
    {
        time_div->set_content(get_time_display(spectate_from));
        render_div(time_div, time_area, 1, 1);
    }

    // message area
    {
        bool expand_message_box = rect_contains(message_area, get_mouse_game_position());
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
            events_div->set_max_width(entire_window_area.w);
            Coord events_div_size = events_div->compute_size(renderer);
            Coord position = {0, entire_window_area.h - events_div_size.y};
            if (position.y > 0)
                position.y = 0;
            events_div->render(renderer, position);
        } else {
            render_div(events_div, message_area, 1, -1);
        }
    }

    // inventory pane
    // cursor is background
    if (show_inventory_cursor) {
        Coord cursor_location = inventory_index_to_location(inventory_cursor);
        SDL_Rect cursor_rect;
        cursor_rect.x = inventory_area.x + cursor_location.x * tile_size;
        cursor_rect.y = inventory_area.y + cursor_location.y * tile_size;
        cursor_rect.w = tile_size;
        cursor_rect.h = tile_size;
        fill_rect(inventory_cursor_color, &cursor_rect);
    }
    for (int i = 0; i < my_inventory.length(); i++) {
        Coord location = inventory_index_to_location(i);
        location.x += map_size.x;
        PerceivedThing item = my_inventory[i];
        render_tile(get_image_for_thing(item), 0xff, location);
    }
    if (show_inventory_cursor_help) {
        keyboard_hover_div->set_content(get_thing_description(player_actor(), my_inventory[inventory_cursor]->id, true));
        popup_help(inventory_area, inventory_index_to_location(inventory_cursor), keyboard_hover_div);
    }
    if (show_inventory_action_menu) {
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
                    item_span->set_color_recursive(black, amber);
                }
                inventory_menu_div->append(item_span);
            }
        }
        popup_help(inventory_area, inventory_index_to_location(inventory_cursor), inventory_menu_div);
    }

    if (show_ability_cursor) {
        Coord cursor_location = inventory_index_to_location(ability_cursor);
        SDL_Rect cursor_rect;
        cursor_rect.x = ability_area.x + cursor_location.x * tile_size;
        cursor_rect.y = ability_area.y + cursor_location.y * tile_size;
        cursor_rect.w = tile_size;
        cursor_rect.h = tile_size;
        fill_rect(ability_cursor_color, &cursor_rect);
    }
    for (int i = 0; i < my_abilities.length(); i++) {
        Coord location = inventory_index_to_location(i) + Coord{map_size.x, inventory_area.h / tile_size};
        int alpha = 0xff;
        if (!is_ability_ready(player_actor(), my_abilities[i]))
            alpha = 0x44;
        render_tile(get_image_for_ability(my_abilities[i]), alpha, location);
    }
    if (show_ability_cursor_help) {
        bool is_ready = is_ability_ready(player_actor(), my_abilities[ability_cursor]);
        keyboard_hover_div->set_content(get_ability_description(my_abilities[ability_cursor], is_ready));
        popup_help(ability_area, inventory_index_to_location(inventory_cursor), keyboard_hover_div);
    }

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
                    item_span->set_color_recursive(black, amber);
                }
                floor_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, spectate_from->location, floor_menu_div);
    }
    if (show_cheatcode_polymorph_choose_species_menu) {
        if (last_cheatcode_polymorph_choose_species_menu_cursor != cheatcode_polymorph_choose_species_menu_cursor) {
            // rerender menu div
            cheatcode_polymorph_choose_species_menu_div->clear();
            last_cheatcode_polymorph_choose_species_menu_cursor = cheatcode_polymorph_choose_species_menu_cursor;
            for (int i = 0; i < SpeciesId_COUNT; i++) {
                if (i > 0)
                    cheatcode_polymorph_choose_species_menu_div->append_newline();
                Span item_span = render_species((SpeciesId)i);
                if (i == cheatcode_polymorph_choose_species_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_polymorph_choose_species_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_polymorph_choose_species_menu_div);
    }
    if (show_cheatcode_wish_choose_thing_type_menu) {
        if (last_cheatcode_wish_choose_thing_type_menu_cursor != cheatcode_wish_choose_thing_type_menu_cursor) {
            // rerender menu div
            cheatcode_wish_choose_thing_type_menu_div->clear();
            last_cheatcode_wish_choose_thing_type_menu_cursor = cheatcode_wish_choose_thing_type_menu_cursor;
            for (int i = 0; i < ThingType_COUNT - 1; i++) {
                if (i > 0)
                    cheatcode_wish_choose_thing_type_menu_div->append_newline();
                Span item_span = render_thing_type((ThingType)(i + 1));
                if (i == cheatcode_wish_choose_thing_type_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_wish_choose_thing_type_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_wish_choose_thing_type_menu_div);
    }
    if (show_cheatcode_wish_choose_wand_id_menu) {
        if (last_cheatcode_wish_choose_wand_id_menu_cursor != cheatcode_wish_choose_wand_id_menu_cursor) {
            // rerender menu div
            cheatcode_wish_choose_wand_id_menu_div->clear();
            last_cheatcode_wish_choose_wand_id_menu_cursor = cheatcode_wish_choose_wand_id_menu_cursor;
            for (int i = 0; i < WandId_COUNT; i++) {
                if (i > 0)
                    cheatcode_wish_choose_wand_id_menu_div->append_newline();
                Span item_span = render_wand_id((WandId)i);
                if (i == cheatcode_wish_choose_wand_id_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_wish_choose_wand_id_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_wish_choose_wand_id_menu_div);
    }
    if (show_cheatcode_wish_choose_potion_id_menu) {
        if (last_cheatcode_wish_choose_potion_id_menu_cursor != cheatcode_wish_choose_potion_id_menu_cursor) {
            // rerender menu div
            cheatcode_wish_choose_potion_id_menu_div->clear();
            last_cheatcode_wish_choose_potion_id_menu_cursor = cheatcode_wish_choose_potion_id_menu_cursor;
            for (int i = 0; i < PotionId_COUNT; i++) {
                if (i > 0)
                    cheatcode_wish_choose_potion_id_menu_div->append_newline();
                Span item_span = render_potion_id((PotionId)i);
                if (i == cheatcode_wish_choose_potion_id_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_wish_choose_potion_id_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_wish_choose_potion_id_menu_div);
    }
    if (show_cheatcode_wish_choose_book_id_menu) {
        if (last_cheatcode_wish_choose_book_id_menu_cursor != cheatcode_wish_choose_book_id_menu_cursor) {
            // rerender menu div
            cheatcode_wish_choose_book_id_menu_div->clear();
            last_cheatcode_wish_choose_book_id_menu_cursor = cheatcode_wish_choose_book_id_menu_cursor;
            for (int i = 0; i < BookId_COUNT; i++) {
                if (i > 0)
                    cheatcode_wish_choose_book_id_menu_div->append_newline();
                Span item_span = render_book_id((BookId)i);
                if (i == cheatcode_wish_choose_book_id_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_wish_choose_book_id_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_wish_choose_book_id_menu_div);
    }
    if (show_cheatcode_generate_monster_choose_species_menu) {
        if (last_cheatcode_generate_monster_choose_species_menu_cursor != cheatcode_generate_monster_choose_species_menu_cursor) {
            // rerender menu div
            cheatcode_generate_monster_choose_species_menu_div->clear();
            last_cheatcode_generate_monster_choose_species_menu_cursor = cheatcode_generate_monster_choose_species_menu_cursor;
            for (int i = 0; i < SpeciesId_COUNT; i++) {
                if (i > 0)
                    cheatcode_generate_monster_choose_species_menu_div->append_newline();
                Span item_span = render_species((SpeciesId)i);
                if (i == cheatcode_generate_monster_choose_species_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_generate_monster_choose_species_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_generate_monster_choose_species_menu_div);
    }
    if (show_cheatcode_generate_monster_choose_decision_maker_menu) {
        if (last_cheatcode_generate_monster_choose_decision_maker_menu_cursor != cheatcode_generate_monster_choose_decision_maker_menu_cursor) {
            // rerender menu div
            cheatcode_generate_monster_choose_decision_maker_menu_div->clear();
            last_cheatcode_generate_monster_choose_decision_maker_menu_cursor = cheatcode_generate_monster_choose_decision_maker_menu_cursor;
            for (int i = 0; i < DecisionMakerType_COUNT; i++) {
                if (i > 0)
                    cheatcode_generate_monster_choose_decision_maker_menu_div->append_newline();
                Span item_span = render_decision_maker((DecisionMakerType)i);
                if (i == cheatcode_generate_monster_choose_decision_maker_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_generate_monster_choose_decision_maker_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_generate_monster_choose_decision_maker_menu_div);
    }
    if (show_cheatcode_generate_monster_location_cursor) {
        SpeciesId species_id = (SpeciesId)cheatcode_generate_monster_choose_species_menu_cursor;
        SwarklandImage_ image = species_images[species_id];
        render_tile(image, 0x7f, cheatcode_generate_monster_choose_location_cursor);
    }
    // popup help for hovering over things
    if (show_map_popup_help) {
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
                    thing_and_carrying->append(get_thing_description(spectate_from, target->id, true));
                    List<PerceivedThing> inventory;
                    find_items_in_inventory(spectate_from, target->id, &inventory);
                    if (inventory.length() > 0) {
                        thing_and_carrying->append(" carrying:");
                        content->append(thing_and_carrying);
                        for (int j = 0; j < inventory.length(); j++) {
                            content->append_newline();
                            content->append_spaces(4);
                            content->append(get_thing_description(spectate_from, inventory[j]->id, true));
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
                mouse_hover_div->set_content(get_thing_description(player_actor(), my_inventory[inventory_index]->id, true));
                popup_help(inventory_area, mouse_hover_inventory_tile, mouse_hover_div);
            }
        }
    }

    {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_GetRendererOutputSize(renderer, &output_rect.w, &output_rect.h);
        // preserve aspect ratio
        float source_aspect_ratio = (float)entire_window_area.w / (float)entire_window_area.h;
        float dest_aspect_ratio = (float)output_rect.w / (float)output_rect.h;
        if (source_aspect_ratio > dest_aspect_ratio) {
            // use width
            int new_height = (int)((float)output_rect.w / source_aspect_ratio);
            output_rect.x = 0;
            output_rect.y = (output_rect.h - new_height) / 2;
            output_rect.h = new_height;
        } else {
            // use height
            int new_width = (int)((float)output_rect.h * source_aspect_ratio);
            output_rect.x = (output_rect.w - new_width) / 2;
            output_rect.y = 0;
            output_rect.w = new_width;
        }

        SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, screen_buffer, &entire_window_area, &output_rect);
    }

    SDL_RenderPresent(renderer);
}
