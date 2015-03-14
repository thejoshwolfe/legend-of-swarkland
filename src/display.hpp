#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "thing.hpp"
#include "string.hpp"
#include "text.hpp"

#include <rucksack/rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

extern TTF_Font * status_box_font;

void display_init();
void display_finish();

Coord get_mouse_tile(SDL_Rect area);
Span get_species_name(SpeciesId species_id);
Span get_thing_description(Thing observer, uint256 target_id);
Span get_individual_description(Thing observer, uint256 target_id);
Span get_item_description(Thing observer, uint256 item_id);
void render();

#endif
