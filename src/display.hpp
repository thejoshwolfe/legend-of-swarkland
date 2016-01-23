#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "thing.hpp"
#include "string.hpp"
#include "text.hpp"

#include <rucksack/rucksack.h>
#include <SDL.h>
#include <SDL_ttf.h>

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

extern TTF_Font * status_box_font;
extern SDL_Surface * sprite_sheet_surface;

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
Span get_species_name(SpeciesId species_id);
Span get_thing_description(Thing observer, uint256 target_id);
void render();

#endif
