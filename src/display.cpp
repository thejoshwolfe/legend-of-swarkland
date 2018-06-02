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
#include "sdl_graphics.hpp"

#include <SDL.h>

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

static const Rect dirt_floor_images[] = {
    sprite_location_dirt_floor0,
    sprite_location_dirt_floor1,
    sprite_location_dirt_floor2,
    sprite_location_dirt_floor3,
    sprite_location_dirt_floor4,
    sprite_location_dirt_floor5,
    sprite_location_dirt_floor6,
    sprite_location_dirt_floor7,
};
static Rect marble_floor_images[] = {
    sprite_location_marble_floor0,
    sprite_location_marble_floor1,
    sprite_location_marble_floor2,
    sprite_location_marble_floor3,
    sprite_location_marble_floor4,
    sprite_location_marble_floor5,
};
static Rect lava_floor_images[] = {
    sprite_location_lava0,
    sprite_location_lava1,
    sprite_location_lava2,
    sprite_location_lava3,
};
static Rect brown_brick_wall_images[] = {
    sprite_location_brown_brick0,
    sprite_location_brown_brick1,
    sprite_location_brown_brick2,
    sprite_location_brown_brick3,
    sprite_location_brown_brick4,
    sprite_location_brown_brick5,
    sprite_location_brown_brick6,
    sprite_location_brown_brick7,
};
static Rect gray_brick_wall_images[] = {
    sprite_location_gray_brick0,
    sprite_location_gray_brick1,
    sprite_location_gray_brick2,
    sprite_location_gray_brick3,
};
static Rect species_images[SpeciesId_COUNT];
static Rect wand_images[WandDescriptionId_COUNT];
static Rect potion_images[PotionDescriptionId_COUNT];
static Rect book_images[BookDescriptionId_COUNT];
static Rect weapon_images[WeaponId_COUNT];

static String version_string = new_string();

#define fill_with_trash(array) \
    for (int i = 0; i < (int)(get_array_length(array)); i++) \
        array[i] = Rect::nowhere()
#define check_no_trash(array) \
    for (int i = 0; i < (int)(get_array_length(array)); i++) \
        assert_str(array[i] != Rect::nowhere(), "missed a spot")

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
    potion_images[PotionDescriptionId_GLITTERY_BLUE_POTION] = sprite_location_glittery_blue_potion;
    potion_images[PotionDescriptionId_GLITTERY_GREEN_POTION] = sprite_location_glittery_green_potion;
    check_no_trash(potion_images);

    fill_with_trash(book_images);
    book_images[BookDescriptionId_PURPLE_BOOK] = sprite_location_purple_book;
    book_images[BookDescriptionId_BLUE_BOOK] = sprite_location_blue_book;
    book_images[BookDescriptionId_RED_BOOK] = sprite_location_red_book;
    book_images[BookDescriptionId_GREEN_BOOK] = sprite_location_green_book;
    book_images[BookDescriptionId_YELLOW_BOOK] = sprite_location_yellow_book;
    check_no_trash(book_images);

    fill_with_trash(weapon_images);
    weapon_images[WeaponId_DAGGER] = sprite_location_dagger;
    weapon_images[WeaponId_BATTLEAXE] = sprite_location_battleaxe;
    weapon_images[WeaponId_STICK] = sprite_location_stick;
    check_no_trash(weapon_images);
}

void init_display() {
    assert(!headless_mode);

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        panic("unable to init SDL", SDL_GetError());

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
        panic("window create failed", SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
        panic("renderer create failed", SDL_GetError());

    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(renderer, &renderer_info);
    if (!(renderer_info.flags & SDL_RENDERER_TARGETTEXTURE))
        panic("rendering to a temporary texture is not supported");

    screen_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, entire_window_area.w, entire_window_area.h);

    sprite_sheet_texture = load_texture(renderer);

    load_images();
    if (false) load_images();

    init_text();

    ByteBuffer buffer;
    buffer.append((const char *)get_binary_version_resource_start(), (int)get_binary_version_resource_size());
    version_string->decode(buffer);
}

void display_finish() {
    assert(!headless_mode);

    text_finish();

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

static void render_tile(Rect image, int alpha, Coord dest_coord) {
    SDL_Rect source_rect;
    source_rect.x = image.position.x;
    source_rect.y = image.position.y;
    source_rect.w = image.size.x;
    source_rect.h = image.size.y;

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
template <typename T>
static bool remove_bit(T * value, int mask) {
    T result = *value & mask;
    *value &= ~mask;
    return result != 0;
}

// ends with " " if non-blank
static Span get_status_description(StatusEffectIdBitField status_effect_bits) {
    Span result = new_span();
    // we need the order of words to be consistent.
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::CONFUSION))
        result->append("confused ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::SPEED))
        result->append("fast ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::SLOWING))
        result->append("slow ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::ETHEREAL_VISION))
        result->append("ethereal-visioned ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::COGNISCOPY))
        result->append("cogniscopic ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::BLINDNESS))
        result->append("blind ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::POISON))
        result->append("poisoned ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::INVISIBILITY))
        result->append("invisible ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::POLYMORPH))
        result->append("polymorphed ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::BURROWING))
        result->append("burrowing ");
    if (remove_bit(&status_effect_bits, 1 << StatusEffect::LEVITATING))
        result->append("levitating ");
    assert_str(status_effect_bits == 0, "missed a spot");
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
static const char * get_wand_description_str(WandDescriptionId description_id, WandId identified_id) {
    if (identified_id != WandId_UNKNOWN) {
        return get_wand_id_str(identified_id);
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
static const char * get_wand_description_str(Thing observer, PerceivedThing item) {
    WandDescriptionId description_id = item->wand_info()->description_id;
    WandId identified_id = description_id != WandDescriptionId_UNSEEN ? observer->life()->knowledge.wand_identities[description_id] : WandId_UNKNOWN;
    return get_wand_description_str(description_id, identified_id);
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
        case PotionId_POTION_OF_BURROWING:
            return "potion of burrowing";
        case PotionId_POTION_OF_LEVITATION:
            return "potion of levitation";

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
const char * get_weapon_id_str(WeaponId weapon_id) {
    switch (weapon_id) {
        case WeaponId_DAGGER:
            return "dagger";
        case WeaponId_BATTLEAXE:
            return "battleaxe";
        case WeaponId_STICK:
            return "stick";

        case WeaponId_COUNT:
        case WeaponId_UNKNOWN:
            unreachable();;
    }
    unreachable();;
}
static const char * get_potion_description_str(PotionDescriptionId description_id, PotionId identified_id) {
    if (identified_id != PotionId_UNKNOWN) {
        return get_potion_id_str(identified_id);
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
            case PotionDescriptionId_GLITTERY_BLUE_POTION:
                return "glittery blue potion";
            case PotionDescriptionId_GLITTERY_GREEN_POTION:
                return "glittery green potion";

            case PotionDescriptionId_UNSEEN:
                return "potion";

            case PotionDescriptionId_COUNT:
                unreachable();
        }
        unreachable();
    }
}
static const char * get_potion_description_str(Thing observer, PerceivedThing item) {
    PotionDescriptionId description_id = item->potion_info()->description_id;
    PotionId identified_id = description_id != PotionDescriptionId_UNSEEN ? observer->life()->knowledge.potion_identities[description_id] : PotionId_UNKNOWN;
    return get_potion_description_str(description_id, identified_id);
}

static const char * get_book_description_str(BookDescriptionId description_id, BookId identified_id) {
    if (identified_id != BookId_UNKNOWN) {
        return get_book_id_str(identified_id);
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
static const char * get_book_description_str(Thing observer, PerceivedThing item) {
    BookDescriptionId description_id = item->book_info()->description_id;
    BookId identified_id = description_id != BookDescriptionId_UNSEEN ? observer->life()->knowledge.book_identities[description_id] : BookId_UNKNOWN;
    return get_book_description_str(description_id, identified_id);
}

static const char * get_weapon_description_str(WeaponId weapon_id) {
    switch (weapon_id) {
        case WeaponId_DAGGER:
            return "dagger";
        case WeaponId_BATTLEAXE:
            return "battleaxe";
        case WeaponId_STICK:
            return "stick";

        case WeaponId_UNKNOWN:
            return "weapon";

        case WeaponId_COUNT:
            unreachable();
    }
    unreachable();
}

Span get_thing_description(ThingSnapshot target) {
    switch (target.thing_type) {
        case ThingSnapshotType_YOU:
            return new_span("you", light_blue, black);
        case ThingSnapshotType_INDIVIDUAL: {
            ThingSnapshot::IndividualSnapshotData & data = target.individual_data();
            Span result = new_span("a ");
            result->append(get_status_description(data.status_effect_bits));
            result->append(get_species_name(data.species_id));
            return result;
        }
        case ThingSnapshotType_WAND: {
            ThingSnapshot::WandSnapshotData & data = target.wand_data();
            Span result = new_span("a ");
            result->append(new_span(get_wand_description_str(data.description_id, data.identified_id), light_green, black));
            return result;
        }
        case ThingSnapshotType_POTION: {
            ThingSnapshot::PotionSnapshotData & data = target.potion_data();
            Span result = new_span("a ");
            result->append(new_span(get_potion_description_str(data.description_id, data.identified_id), light_green, black));
            return result;
        }
        case ThingSnapshotType_BOOK: {
            ThingSnapshot::BookSnapshotData & data = target.book_data();
            Span result = new_span("a ");
            result->append(new_span(get_book_description_str(data.description_id, data.identified_id), light_green, black));
            return result;
        }
        case ThingSnapshotType_WEAPON: {
            WeaponId & weapon_id = target.weapon_data();
            Span result = new_span("a ");
            result->append(new_span(get_weapon_description_str(weapon_id), light_green, black));
            return result;
        }
    }
    unreachable();
}

Span get_thing_description(Thing observer, uint256 target_id, bool verbose) {
    if (observer->id == target_id)
        return new_span("you", light_blue, black);

    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id);

    Span result = new_span("a ");
    result->append(get_status_description(target->status_effect_bits));

    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            result->append(get_species_name(target->life()->species_id));
            break;
        case ThingType_WAND: {
            result->append(new_span(get_wand_description_str(observer, target), light_green, black));
            int used_count = target->wand_info()->used_count;
            if (verbose && used_count != 0) {
                result->append(" ");
                result->append(get_used_count_description(used_count));
            }
            break;
        }
        case ThingType_POTION:
            result->append(new_span(get_potion_description_str(observer, target), light_green, black));
            break;
        case ThingType_BOOK:
            result->append(new_span(get_book_description_str(observer, target), light_green, black));
            break;
        case ThingType_WEAPON:
            result->append(new_span(get_weapon_description_str(target->weapon_info()->weapon_id), light_green, black));
            break;

        case ThingType_COUNT:
            unreachable();
    }

    if (verbose) {
        if (target->location.type == Location::INVENTORY && target->location.inventory().is_equipped)
            result->append(" (equipped)");
    }

    return result;
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

static Rect get_image_for_thing(PerceivedThing thing) {
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
        case ThingType_WEAPON:
            if (thing->weapon_info()->weapon_id == WeaponId_UNKNOWN)
                return sprite_location_unseen_weapon;
            return weapon_images[thing->weapon_info()->weapon_id];

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
static Rect get_image_for_thing(Thing thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            return species_images[thing->physical_species_id()];
        case ThingType_WAND:
            return wand_images[game->actual_wand_descriptions[thing->wand_info()->wand_id]];
        case ThingType_POTION:
            return potion_images[game->actual_potion_descriptions[thing->potion_info()->potion_id]];
        case ThingType_BOOK:
            return book_images[game->actual_book_descriptions[thing->book_info()->book_id]];
        case ThingType_WEAPON:
            return weapon_images[thing->weapon_info()->weapon_id];

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
static Rect get_image_for_tile(TileType tile_type, uint32_t aesthetic_index) {
    switch (tile_type) {
        case TileType_UNKNOWN:
            return Rect::nowhere();
        case TileType_DIRT_FLOOR:
            return dirt_floor_images[aesthetic_index % get_array_length(dirt_floor_images)];
        case TileType_MARBLE_FLOOR:
            return marble_floor_images[aesthetic_index % get_array_length(marble_floor_images)];
        case TileType_LAVA_FLOOR:
            return lava_floor_images[aesthetic_index % get_array_length(lava_floor_images)];
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
static Rect get_image_for_ability(AbilityId ability_id) {
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            return sprite_location_cobra_venom;
        case AbilityId_THROW_TAR:
            return sprite_location_tar_elemental_throw;
        case AbilityId_ASSUME_FORM:
            return sprite_location_shapeshifter;
        case AbilityId_LUNGE_ATTACK:
            return sprite_location_lunge;

        case AbilityId_COUNT:
            unreachable();
    }
    unreachable();
}

struct StatusDisplay {
    int hp;
    int max_hp;
    int mana;
    int max_mana;
    int experience_level;
    int experience;
    int next_level_up;
    int dungeon_level;
    Nullable<WeaponBehavior> weapon_behavior;
    int innate_attack_power;
    int status_effect_bits;

    StatusDisplay() {
        memset(this, 0, sizeof(StatusDisplay));
    }
    constexpr bool operator==(const StatusDisplay & other) const {
        return memcmp(this, &other, sizeof(StatusDisplay)) == 0;
    }
    constexpr bool operator!=(const StatusDisplay & other) const {
        return !(*this == other);
    }
};

static int previous_events_length = 0;
static uint256 previous_spectator_id = uint256::zero();
static TutorialPromptBits previous_tutorial_prompt;
static Div tutorial_div = new_div();
static Div version_div = new_div();
static Div events_div = new_div();
StatusDisplay previous_status_display;
static Div vitality_div = new_div();
static Div experience_div = new_div();
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
static Div cheatcode_wish_choose_weapon_id_menu_div = new_div();
static int last_cheatcode_wish_choose_weapon_id_menu_cursor = -1;
static Div cheatcode_generate_monster_choose_species_menu_div = new_div();
static int last_cheatcode_generate_monster_choose_species_menu_cursor = -1;
static Div cheatcode_generate_monster_choose_decision_maker_menu_div = new_div();
static int last_cheatcode_generate_monster_choose_decision_maker_menu_cursor = -1;

static const char * tutorial_prompt_str(TutorialPrompt tutorial_prompt) {
    switch (tutorial_prompt) {
        case TutorialPrompt_QUIT:
            return "Alt+F4: quit";
        case TutorialPrompt_MOVE_HIT:
            return "qweadzxc: move/hit";
        case TutorialPrompt_INVENTORY:
            return "Tab: inventory...";
        case TutorialPrompt_ABILITY:
            return "v: ability...";
        case TutorialPrompt_PICK_UP:
            return "s: pick up";
        case TutorialPrompt_GO_DOWN:
            return "s: go down";
        case TutorialPrompt_GROUND_ACTION:
            return "s: action...";
        case TutorialPrompt_REST:
            return "r: rest";
        case TutorialPrompt_MOVE_CURSOR:
            return "qweadzxc: move cursor";
        case TutorialPrompt_MOVE_CURSOR_MENU:
            return "w/x: move cursor";
        case TutorialPrompt_DIRECTION:
            return "qweadzxc: direction";
        case TutorialPrompt_YOURSELF:
            return "s: yourself";
        case TutorialPrompt_MENU_ACTION:
            return "Tab/s: action...";
        case TutorialPrompt_ACCEPT:
            return "Tab/s: accept";
        case TutorialPrompt_ACCEPT_SUBMENU:
            return "Tab/s: accept...";
        case TutorialPrompt_CANCEL:
            return "Esc: cancel";
        case TutorialPrompt_BACK:
            return "Esc: back";
        case TutorialPrompt_WHATS_THIS:
            return "mouse: what's this";
        case TutorialPrompt_COUNT:
            unreachable();
    }
    unreachable();
}

static Div render_tutorial_div_content(TutorialPromptBits tutorial_prompt_bits) {
    Div div = new_div();
    bool add_separator = false;
    TutorialPrompt tutorial_prompt;
    for (auto iter = BitFieldIterator<TutorialPrompt, TutorialPromptBits>(tutorial_prompt_bits); iter.next(&tutorial_prompt);) {
        if (add_separator)
            div->append_newline();
        add_separator = true;

        const char * text = tutorial_prompt_str(tutorial_prompt);
        div->append(new_span(text));
    }
    return div;
}
static TutorialPromptBits get_tutorial_prompts(Thing spectate_from, bool has_inventory, bool has_abilities) {
    TutorialPromptBits result = 0;
    if (!you()->still_exists) {
        result |= 1 << TutorialPrompt_QUIT;
    } else {
        switch (input_mode) {
            case InputMode_MAIN: {
                List<Action> floor_actions;
                get_floor_actions(spectate_from, &floor_actions);

                result |= 1 << TutorialPrompt_MOVE_HIT;
                if (has_inventory)
                    result |= 1 << TutorialPrompt_INVENTORY;
                if (has_abilities)
                    result |= 1 << TutorialPrompt_ABILITY;
                if (floor_actions.length() == 0) {
                } else if (floor_actions.length() == 1) {
                    Action::Id action_id = floor_actions[0].id;
                    if (action_id == Action::PICKUP) {
                        result |= 1 << TutorialPrompt_PICK_UP;
                    } else if (action_id == Action::GO_DOWN) {
                        result |= 1 << TutorialPrompt_GO_DOWN;
                    } else {
                        unreachable();
                    }
                } else {
                    result |= 1 << TutorialPrompt_GROUND_ACTION;
                }

                List<uint256> scary_individuals;
                List<StatusEffect::Id> annoying_status_effects;
                bool stop_for_other_reasons;
                assess_auto_wait_situation(&scary_individuals, &annoying_status_effects, &stop_for_other_reasons);
                if (scary_individuals.length() == 0 && annoying_status_effects.length() > 0) {
                    if (current_player_decision.id == Action::AUTO_WAIT) {
                        // in the middle of an auto wait
                        result |= 1 << TutorialPrompt_CANCEL;
                    } else {
                        // TODO: talk about the reasons somehow
                        result |= 1 << TutorialPrompt_REST;
                    }
                }
                break;
            }
            case InputMode_INVENTORY_CHOOSE_ITEM:
            case InputMode_CHOOSE_ABILITY:
                result |= 1 << TutorialPrompt_MOVE_CURSOR_MENU;
                result |= 1 << TutorialPrompt_MENU_ACTION;
                result |= 1 << TutorialPrompt_CANCEL;
                break;
            case InputMode_INVENTORY_CHOOSE_ACTION:
                result |= 1 << TutorialPrompt_MOVE_CURSOR_MENU;
                switch (inventory_menu_items[inventory_menu_cursor]) {
                    case Action::DROP:
                    case Action::QUAFF:
                    case Action::EQUIP:
                    case Action::UNEQUIP:
                        result |= 1 << TutorialPrompt_ACCEPT;
                        break;
                    case Action::ZAP:
                    case Action::READ_BOOK:
                    case Action::THROW:
                        result |= 1 << TutorialPrompt_ACCEPT_SUBMENU;
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
                result |= 1 << TutorialPrompt_BACK;
                break;
            case InputMode_FLOOR_CHOOSE_ACTION:
            case InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES:
            case InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE:
            case InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID:
            case InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID:
            case InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID:
            case InputMode_CHEATCODE_WISH_CHOOSE_WEAPON_ID:
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES:
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER:
                result |= 1 << TutorialPrompt_MOVE_CURSOR_MENU;
                result |= 1 << TutorialPrompt_ACCEPT;
                result |= 1 << TutorialPrompt_BACK;
                break;
            case InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION:
                result |= 1 << TutorialPrompt_MOVE_CURSOR;
                result |= 1 << TutorialPrompt_ACCEPT;
                result |= 1 << TutorialPrompt_BACK;
                break;
            case InputMode_THROW_CHOOSE_DIRECTION:
            case InputMode_ZAP_CHOOSE_DIRECTION:
            case InputMode_READ_BOOK_CHOOSE_DIRECTION:
            case InputMode_ABILITY_CHOOSE_DIRECTION:
                result |= 1 << TutorialPrompt_DIRECTION;
                result |= 1 << TutorialPrompt_YOURSELF;
                result |= 1 << TutorialPrompt_CANCEL;
                break;
        }
    }
    result |= 1 << TutorialPrompt_WHATS_THIS;
    return result;
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
        if (movement_cost < 12) {
            // enhanced time sense can see the ticks
            int and_ticks = game->time_counter % 12;
            time_string->append(':');
            if (and_ticks < 10)
                time_string->append('0');
            time_string->format("%d", and_ticks);
        }
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
        case Action::EQUIP:
            return "equip";
        case Action::UNEQUIP:
            return "unequip";

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
            Rect image = get_image_for_thing(actor->life()->knowledge.perceived_things.get(action.item()));
            result->format("pick up %g%s", image, get_thing_description(actor, action.item(), true));
            return result;
        }
        case Action::GO_DOWN:
            result->format("go down %g", sprite_location_stairs_down);
            return result;

        case Action::DROP:
        case Action::EQUIP:
        case Action::UNEQUIP:
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
        case ThingType_WEAPON:
            result->format("%g%s", sprite_location_unseen_weapon, new_span("weapon"));
            break;

        case ThingType_COUNT:
            unreachable();
    }
    return result;
}
static Span render_wand_id(WandId wand_id) {
    Span result = new_span();
    WandDescriptionId description_id = game->actual_wand_descriptions[wand_id];
    result->format("%g%s", wand_images[description_id], new_span(get_wand_id_str(wand_id)));
    return result;
}
static Span render_potion_id(PotionId potion_id) {
    Span result = new_span();
    PotionDescriptionId description_id = game->actual_potion_descriptions[potion_id];
    result->format("%g%s", potion_images[description_id], new_span(get_potion_id_str(potion_id)));
    return result;
}
static Span render_book_id(BookId book_id) {
    Span result = new_span();
    BookDescriptionId description_id = game->actual_book_descriptions[book_id];
    result->format("%g%s", book_images[description_id], new_span(get_book_id_str(book_id)));
    return result;
}
static Span render_weapon_id(WeaponId weapon_id) {
    Span result = new_span();
    result->format("%g%s", weapon_images[weapon_id], new_span(get_weapon_id_str(weapon_id)));
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
        case AbilityId_LUNGE_ATTACK:
            span->format("lunge attack");
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

Span render_event(PerceivedEvent event) {
    Span span = new_span();
    switch (event.type) {
        case PerceivedEvent::THE_INDIVIDUAL: {
            const PerceivedEvent::TheIndividualData & data = event.the_individual_data();
            Span individual_description;
            switch (data.id) {
                case PerceivedEvent::APPEAR:
                    span->format("%s appears out of nowhere!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::LEVEL_UP:
                    span->format("%s levels up.", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::DIE:
                    span->format("%s dies.", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::DELETE_THING:
                    return nullptr;
                case PerceivedEvent::SPIT_BLINDING_VENOM:
                    span->format("%s spits blinding venom!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::BLINDING_VENOM_HIT_INDIVIDUAL:
                    span->format("%s is hit by blinding venom!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::THROW_TAR:
                    span->format("%s throws tar!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::TAR_HIT_INDIVIDUAL:
                    span->format("%s is hit by tar!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::MAGIC_BEAM_HIT_INDIVIDUAL:
                    span->format("a magic beam hits %s.", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::INDIVIDUAL_DODGES_MAGIC_BEAM:
                    span->format("%s dodges a magic beam.", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::MAGIC_MISSILE_HIT_INDIVIDUAL:
                    span->format("a magic missile hits %s!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::MAGIC_BULLET_HIT_INDIVIDUAL:
                    span->format("a magic bullet hits %s!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::INDIVIDUAL_IS_HEALED:
                    span->format("%s is healed!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::ACTIVATED_MAPPING:
                    span->format("%s gets a vision of a map of the area!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::MAGIC_BEAM_PUSH_INDIVIDUAL:
                    span->format("a magic beam pushes %s!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::MAGIC_BEAM_RECOILS_AND_PUSHES_INDIVIDUAL:
                    span->format("the magic beam recoils and pushes %s!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::FAIL_TO_CAST_SPELL:
                    span->format("%s must not understand how to cast that spell.", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::SEARED_BY_LAVA:
                    span->format("%s is seared by lava!", get_thing_description(data.individual));
                    break;
                case PerceivedEvent::INDIVIDUAL_FLOATS_UNCONTROLLABLY:
                    span->format("%s floats uncontrollably!", get_thing_description(data.individual));
                    // we also know that the individual is levitating, but we should already know that.
                    break;
                case PerceivedEvent::LUNGE:
                    span->format("%s lunges!", get_thing_description(data.individual));
                    break;
            }
            break;
        }
        case PerceivedEvent::THE_LOCATION: {
            const PerceivedEvent::TheLocationData & data = event.the_location_data();
            switch (data.id) {
                case PerceivedEvent::MAGIC_BEAM_HIT_WALL:
                    span->format("a magic beam hits a wall.");
                    break;
                case PerceivedEvent::BEAM_OF_DIGGING_DIGS_WALL:
                    span->format("a magic beam digs away a wall!");
                    break;
                case PerceivedEvent::MAGIC_BEAM_PASS_THROUGH_AIR:
                    span = nullptr;
                    break;
            }
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_STATUS: {
            const PerceivedEvent::IndividualAndStatusData & data = event.individual_and_status_data();
            Span individual_description;
            const char * is_no_longer;
            const char * punctuation;
            bool is_gain = data.id == PerceivedEvent::GAIN_STATUS;
            if (is_gain) {
                is_no_longer = "is";
                punctuation = "!";
                individual_description = get_thing_description(data.individual);
            } else {
                is_no_longer = "is no longer";
                punctuation = ".";
                individual_description = get_thing_description(data.individual);
            }
            const char * status_description = nullptr;
            switch (data.status) {
                case StatusEffect::CONFUSION:
                    status_description = "confused";
                    break;
                case StatusEffect::SPEED:
                    if (is_gain) {
                        span->format("%s speeds up!", individual_description);
                    } else {
                        span->format("%s slows back down to normal speed.", individual_description);
                    }
                    break;
                case StatusEffect::ETHEREAL_VISION:
                    if (is_gain) {
                        span->format("%s gains ethereal vision!", individual_description);
                    } else {
                        span->format("%s no longer has ethereal vision.", individual_description);
                    }
                    break;
                case StatusEffect::COGNISCOPY:
                    status_description = "cogniscopic";
                    break;
                case StatusEffect::BLINDNESS:
                    status_description = "blind";
                    break;
                case StatusEffect::POISON:
                    status_description = "poisoned";
                    break;
                case StatusEffect::INVISIBILITY:
                    status_description = "invisible";
                    break;
                case StatusEffect::POLYMORPH:
                    // there's a different event for polymorph since you can't tell if someone is gaining it or losing it.
                    unreachable();
                case StatusEffect::SLOWING:
                    status_description = "slow";
                    break;
                case StatusEffect::BURROWING:
                    if (is_gain) {
                        span->format("the ground below %s starts to ripple!", individual_description);
                    } else {
                        span->format("the ground below %s looks solid.", individual_description);
                    }
                    break;
                case StatusEffect::LEVITATING:
                    status_description = "levitating";
                    break;

                case StatusEffect::PUSHED:
                case StatusEffect::COUNT:
                    unreachable();
            }
            if (status_description != nullptr) {
                // TODO: we should be able to pass const char * to span->format()
                String string = new_string();
                string->format("%s %s%s", is_no_longer, status_description, punctuation);
                span->format("%s %s", individual_description, new_span(string));
            }
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_LOCATION: {
            const PerceivedEvent::IndividualAndLocationData & data = event.individual_and_location_data();
            Span actor_description = get_thing_description(data.actor);
            Span bumpee_description;
            if (!data.is_air)
                bumpee_description = new_span("a wall");
            else
                bumpee_description = new_span("thin air");
            switch (data.id) {
                case PerceivedEvent::BUMP_INTO_LOCATION:
                    span->format("%s bumps into %s.", actor_description, bumpee_description);
                    break;
                case PerceivedEvent::ATTACK_LOCATION:
                    span->format("%s hits %s.", actor_description, bumpee_description);
                    break;
                case PerceivedEvent::INDIVIDUAL_BURROWS_THROUGH_WALL:
                    span->format("%s burrows through a wall.", actor_description);
                    break;
            }
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_TWO_LOCATION:
            return nullptr;
        case PerceivedEvent::TWO_INDIVIDUAL: {
            const PerceivedEvent::TwoIndividualData & data = event.two_individual_data();
            Span actor_description = get_thing_description(data.actor);
            // what did it bump into? whatever we think is there
            Span bumpee_description = get_thing_description(data.target);
            const char * fmt;
            switch (data.id) {
                case PerceivedEvent::BUMP_INTO_INDIVIDUAL:
                    fmt = "%s bumps into %s.";
                    break;
                case PerceivedEvent::ATTACK_INDIVIDUAL:
                    fmt = "%s hits %s.";
                    break;
                case PerceivedEvent::DODGE_ATTACK:
                    fmt = "%s attacks, but %s dodges.";
                    break;
                case PerceivedEvent::MELEE_KILL:
                    fmt = "%s kills %s.";
                    break;
                case PerceivedEvent::PUSH_INDIVIDUAL:
                    fmt = "%s pushes %s.";
                    break;
            }
            span->format(fmt, actor_description, bumpee_description);
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_ITEM: {
            const PerceivedEvent::IndividualAndItemData & data = event.individual_and_item_data();
            Span individual_description = get_thing_description(data.individual);
            Span item_description = get_thing_description(data.item);
            switch (data.id) {
                case PerceivedEvent::ZAP_WAND:
                    span->format("%s zaps %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::ZAP_WAND_NO_CHARGES:
                    span->format("%s zaps %s, but %s just sputters.", individual_description, item_description, item_description);
                    break;
                case PerceivedEvent::WAND_DISINTEGRATES:
                    span->format("%s tries to zap %s, but %s disintegrates.", individual_description, item_description, item_description);
                    break;
                case PerceivedEvent::READ_BOOK:
                    span->format("%s reads %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::QUAFF_POTION:
                    span->format("%s quaffs %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::THROW_ITEM:
                    span->format("%s throws %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::ITEM_HITS_INDIVIDUAL:
                    span->format("%s hits %s!", item_description, individual_description);
                    break;
                case PerceivedEvent::ITEM_SINKS_INTO_INDIVIDUAL:
                    span->format("%s sinks into %s!", item_description, individual_description);
                    break;
                case PerceivedEvent::INDIVIDUAL_DODGES_THROWN_ITEM:
                    span->format("%s dodges %s!", individual_description, item_description);
                    break;
                case PerceivedEvent::POTION_HITS_INDIVIDUAL:
                    span->format("%s shatters and splashes on %s!", item_description, individual_description);
                    break;
                case PerceivedEvent::INDIVIDUAL_PICKS_UP_ITEM:
                    span->format("%s picks up %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::INDIVIDUAL_SUCKS_UP_ITEM:
                    span->format("%s sucks up %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::EQUIP_ITEM:
                    span->format("%s equips %s.", individual_description, item_description);
                    break;
                case PerceivedEvent::UNEQUIP_ITEM:
                    span->format("%s unequips %s.", individual_description, item_description);
                    break;
            }
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_TWO_ITEM: {
            const PerceivedEvent::IndividualAndTwoItemData & data = event.individual_and_two_item_data();
            Span individual_description = get_thing_description(data.individual);
            // we don't shw the old item
            Span new_item_description = get_thing_description(data.new_item);
            switch (data.id) {
                case PerceivedEvent::SWAP_EQUIPPED_ITEM:
                    span->format("%s zaps %s.", individual_description, new_item_description);
                    break;
            }
            break;
        }
        case PerceivedEvent::INDIVIDUAL_AND_SPECIES: {
            const PerceivedEvent::IndividualAndSpeciesData & data = event.individual_and_species_data();
            span->format("%s transforms into a %s!", get_thing_description(data.individual), get_species_name(data.new_species));
            break;
        }
        case PerceivedEvent::ITEM_AND_LOCATION: {
            const PerceivedEvent::ItemAndLocationData & data = event.item_and_location_data();
            Span item_description = get_thing_description(data.item);
            switch (data.id) {
                case PerceivedEvent::WAND_EXPLODES:
                    span->format("%s explodes!", item_description);
                    break;
                case PerceivedEvent::ITEM_HITS_WALL:
                    span->format("%s hits a wall.", item_description);
                    break;
                case PerceivedEvent::ITEM_DROPS_TO_THE_FLOOR:
                    span->format("%s drops to the floor.", item_description);
                    break;
                case PerceivedEvent::POTION_BREAKS:
                    span->format("%s shatters!", item_description);
                    break;
                case PerceivedEvent::ITEM_DISINTEGRATES_IN_LAVA:
                    span->format("%s disintegrates in lava!", item_description);
                    break;
            }
            break;
        }
    }

    return span;
}


void render() {
    assert(!headless_mode);

    Thing spectate_from = cheatcode_spectator != nullptr ? cheatcode_spectator : player_actor();
    PerceivedThing perceived_self = spectate_from->life()->knowledge.perceived_things.get(spectate_from->id);
    List<PerceivedThing> my_inventory;
    find_items_in_inventory(player_actor(), player_actor()->id, &my_inventory);
    List<AbilityId> my_abilities;
    get_abilities(player_actor(), &my_abilities);
    Thing equipped_weapon = get_equipped_weapon(spectate_from);

    StatusDisplay status_display;
    status_display.hp = spectate_from->life()->hitpoints;
    status_display.max_hp = spectate_from->max_hitpoints();
    status_display.mana = spectate_from->life()->mana;
    status_display.max_mana = spectate_from->max_mana();
    status_display.experience_level = spectate_from->experience_level();
    status_display.experience = spectate_from->life()->experience;
    status_display.next_level_up = spectate_from->next_level_up();
    status_display.dungeon_level = game->dungeon_level;
    status_display.weapon_behavior = nullptr;
    if (equipped_weapon != nullptr) {
        status_display.weapon_behavior = get_weapon_behavior(equipped_weapon->weapon_info()->weapon_id);
    }
    status_display.innate_attack_power = spectate_from->innate_attack_power();
    status_display.status_effect_bits = perceived_self->status_effect_bits;

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
    bool show_cheatcode_wish_choose_weapon_id_menu = false;
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
        case InputMode_THROW_CHOOSE_DIRECTION: {
            Coord range_window = get_throw_range_window(my_inventory[inventory_cursor]);
            direction_distance_min = range_window.x;
            direction_distance_max = range_window.y;
            show_inventory_cursor = true;
            inventory_cursor_color = gray;
            show_map_popup_help = true;
            break;
        }
        case InputMode_ABILITY_CHOOSE_DIRECTION: {
            List<AbilityId> abilities;
            get_abilities(player_actor(), &abilities);
            AbilityId chosen_ability = abilities[ability_cursor];
            Coord range_window = get_ability_range_window(chosen_ability);
            direction_distance_min = range_window.x;
            direction_distance_max = range_window.y;
            show_ability_cursor = true;
            ability_cursor_color = gray;
            show_map_popup_help = true;
            break;
        }
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
        case InputMode_CHEATCODE_WISH_CHOOSE_WEAPON_ID:
            show_cheatcode_wish_choose_weapon_id_menu = true;
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
    {
        TutorialPromptBits tutorial_prompt = get_tutorial_prompts(player_actor(), my_inventory.length() > 0, my_abilities.length() > 0);
        if (previous_tutorial_prompt != tutorial_prompt) {
            tutorial_div->set_content(render_tutorial_div_content(tutorial_prompt));
            previous_tutorial_prompt = tutorial_prompt;
        }
    }
    render_div(tutorial_div, tutorial_area, 1, 1);

    if (version_div->is_empty()) {
        Span blurb_span = new_span("v", gray, black);
        blurb_span->append(version_string);
        version_div->set_content(blurb_span);
    }
    render_div(version_div, version_area, -1, -1);

    // main map
    // render the terrain
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            TileType tile = spectate_from->life()->knowledge.tiles[cursor];
            uint8_t aesthetic_index = spectate_from->life()->knowledge.aesthetic_indexes[cursor];
            if (cheatcode_full_visibility) {
                tile = game->actual_map_tiles[cursor];
                aesthetic_index = game->aesthetic_indexes[cursor];
            }
            Rect image = get_image_for_tile(tile, aesthetic_index);
            if (image == Rect::nowhere())
                continue;
            Uint8 alpha;
            if (can_see_shape(spectate_from->life()->knowledge.tile_is_visible[cursor])) {
                // it's in our direct line of sight
                if (direction_distance_min != -1) {
                    // actually, let's only show the 8 directions
                    Coord vector = spectate_from->location.standing() - cursor;
                    if (vector.x * vector.y == 0 || abs(vector.x) == abs(vector.y)) {
                        // ordinal aligned
                        int distance = ordinal_distance(spectate_from->location.standing(), cursor);
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
        List<PerceivedThing> floor_items;
        List<PerceivedThing> standing_individuals;
        PerceivedThing thing;
        for (auto iterator = spectate_from->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
            switch (thing->location.type) {
                case Location::NOWHERE:
                case Location::INVENTORY:
                    continue;
                case Location::FLOOR_PILE:
                    floor_items.append(thing);
                    break;
                case Location::STANDING:
                    standing_individuals.append(thing);
                    break;
            }
        }
        sort<PerceivedThing, compare_floor_items_by_z_order>(floor_items.raw(), floor_items.length());
        // only render 1 of each type of thing in each location on the map
        MapMatrix<bool> item_pile_rendered;
        item_pile_rendered.set_all(false);
        for (int i = 0; i < floor_items.length(); i++) {
            PerceivedThing item = floor_items[i];
            Coord coord = item->location.floor_pile().coord;
            if (item_pile_rendered[coord])
                continue;
            item_pile_rendered[coord] = true;

            Uint8 alpha = get_thing_alpha(spectate_from, item);
            render_tile(get_image_for_thing(item), alpha, coord);
        }

        // render individuals in front of item piles
        for (int i = 0; i < standing_individuals.length(); i++) {
            PerceivedThing individual = standing_individuals[i];
            Coord coord = individual->location.standing();
            Uint8 alpha = get_thing_alpha(spectate_from, individual);
            render_tile(get_image_for_thing(individual), alpha, coord);

            List<PerceivedThing> inventory;
            find_items_in_inventory(spectate_from, individual->id, &inventory);
            if (inventory.length() > 0) {
                render_tile(sprite_location_equipment, alpha, coord);
            }
        }
    } else {
        // full visibility
        Thing thing;
        for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);) {
            // this exposes hashtable iteration order, but it's a cheatcode, so whatever.
            if (!thing->still_exists)
                continue;
            if (!is_standing_or_floor(thing->location))
                continue;
            Coord coord = get_standing_or_floor_coord(thing->location);
            Uint8 alpha;
            if (has_status(thing, StatusEffect::INVISIBILITY) || !can_see_shape(spectate_from->life()->knowledge.tile_is_visible[coord]))
                alpha = 0x7f;
            else
                alpha = 0xff;
            render_tile(get_image_for_thing(thing), alpha, coord);

            List<Thing> inventory;
            find_items_in_inventory(thing->id, &inventory);
            if (inventory.length() > 0)
                render_tile(sprite_location_equipment, alpha, coord);
        }
    }

    // status bar
    if (status_display != previous_status_display) {
        vitality_div->clear();
        String hp_string = new_string();
        hp_string->format("HP: %d/%d", status_display.hp, status_display.max_hp);
        Span hp_span = new_span(hp_string);
        if (status_display.hp <= status_display.max_hp / 3)
            hp_span->set_color(white, red);
        else if (status_display.hp < status_display.max_hp)
            hp_span->set_color(black, amber);
        else
            hp_span->set_color(white, dark_green);
        vitality_div->append(hp_span);
        if (status_display.max_mana > 0) {
            String mp_string = new_string();
            mp_string->format("MP: %d/%d", status_display.mana, status_display.max_mana);
            Span mp_span = new_span(mp_string);
            if (status_display.mana <= 0)
                mp_span->set_color(white, red);
            else if (status_display.mana < status_display.max_mana)
                mp_span->set_color(black, amber);
            else
                mp_span->set_color(white, black);
            vitality_div->append_newline();
            vitality_div->append(mp_span);
        }

        experience_div->clear();
        String string = new_string();
        string->format("XP Level: %d", status_display.experience_level);
        experience_div->append(new_span(string));
        experience_div->append_newline();
        string->clear();
        string->format("XP:       %d/%d", status_display.experience, status_display.next_level_up);
        experience_div->append(new_span(string));

        dungeon_level_div->clear();
        String dungeon_level_string = new_string();
        dungeon_level_string->format("Dungeon Level: %d", status_display.dungeon_level);
        dungeon_level_div->append(new_span(dungeon_level_string));
        dungeon_level_div->append_newline();
        String attack_damage_string = new_string();
        if (status_display.weapon_behavior == nullptr) {
            attack_damage_string->format("Attack: %d", status_display.innate_attack_power);
        } else {
            switch (status_display.weapon_behavior->type) {
                case WeaponBehavior::BONUS_DAMAGE:
                    attack_damage_string->format("Attack: %d+%d", status_display.innate_attack_power, status_display.weapon_behavior->bonus_damage);
                    break;
                case WeaponBehavior::PUSH:
                    attack_damage_string->format("Attack: push");
                    break;
            }
        }
        dungeon_level_div->append(new_span(attack_damage_string));

        status_div->set_content(get_status_description(status_display.status_effect_bits));

        previous_status_display = status_display;
    }
    {
        time_div->set_content(get_time_display(spectate_from));
    }
    render_div(vitality_div, hp_area, 1, 1);
    render_div(experience_div, xp_area, 1, 1);
    render_div(dungeon_level_div, dungeon_level_area, 1, 1);
    render_div(status_div, status_area, 1, 1);
    render_div(time_div, time_area, 1, 1);

    // message area
    {
        bool expand_message_box = rect_contains(message_area, get_mouse_game_position());
        List<Nullable<PerceivedEvent>> & events = spectate_from->life()->knowledge.perceived_events;
        bool refresh_events = previous_spectator_id != spectate_from->id;
        if (refresh_events) {
            previous_events_length = 0;
            previous_spectator_id = spectate_from->id;
            events_div->clear();
        }
        for (int i = previous_events_length; i < events.length(); i++) {
            Nullable<PerceivedEvent> event = events[i];
            if (event != nullptr) {
                // append something
                if (i > 0) {
                    // maybe sneak in a delimiter
                    if (events[i - 1] == nullptr)
                        events_div->append_newline();
                    else
                        events_div->append_spaces(2);
                }
                events_div->append(render_event(*event));
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
        if (equipped_weapon != nullptr && item->id == equipped_weapon->id)
            render_tile(sprite_location_equipment, 0xff, location);
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
        popup_help(main_map_area, spectate_from->location.standing(), floor_menu_div);
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
    if (show_cheatcode_wish_choose_weapon_id_menu) {
        if (last_cheatcode_wish_choose_weapon_id_menu_cursor != cheatcode_wish_choose_weapon_id_menu_cursor) {
            // rerender menu div
            cheatcode_wish_choose_weapon_id_menu_div->clear();
            last_cheatcode_wish_choose_weapon_id_menu_cursor = cheatcode_wish_choose_weapon_id_menu_cursor;
            for (int i = 0; i < WeaponId_COUNT; i++) {
                if (i > 0)
                    cheatcode_wish_choose_weapon_id_menu_div->append_newline();
                Span item_span = render_weapon_id((WeaponId)i);
                if (i == cheatcode_wish_choose_weapon_id_menu_cursor) {
                    item_span->set_color_recursive(black, amber);
                }
                cheatcode_wish_choose_weapon_id_menu_div->append(item_span);
            }
        }
        popup_help(main_map_area, Coord{-1, -1}, cheatcode_wish_choose_weapon_id_menu_div);
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
        Rect image = species_images[species_id];
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
                    if (i > 0)
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
