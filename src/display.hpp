#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "thing.hpp"
#include "string.hpp"
#include "text.hpp"

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

extern SDL_Texture * sprite_sheet_texture;

void init_display();
void display_finish();

static const int inventory_layout_width = 5;
// 1 => {1, 0}
static inline Coord inventory_index_to_location(int index) {
    return {index % inventory_layout_width, index / inventory_layout_width};
}
static inline int inventory_location_to_index(Coord location) {
    return location.y * inventory_layout_width + location.x;
}

Coord get_mouse_tile(SDL_Rect area);
const char * get_species_name_str(SpeciesId species_id);
const char * get_wand_id_str(WandId wand_id);
const char * get_potion_id_str(PotionId potion_id);
const char * get_book_id_str(BookId book_id);
const char * get_weapon_id_str(WeaponId weapon_id);
Span get_species_name(SpeciesId species_id);
Span get_thing_description(ThingSnapshot thing_snapshot);
Span get_thing_description(Thing observer, uint256 target_id, bool verbose);
Span render_event(PerceivedEvent event);
void render();

#endif
