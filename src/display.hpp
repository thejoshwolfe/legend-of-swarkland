#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "individual.hpp"
#include "string.hpp"

#include <rucksack/rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

extern TTF_Font * status_box_font;

void display_init(const char * resource_bundle_path);
void display_finish();

Coord get_mouse_tile(SDL_Rect area);
String get_species_name(SpeciesId species_id);
void get_thing_description(Thing observer, uint256 target_id, String output);
void get_individual_description(Thing observer, uint256 target_id, String output);
void get_item_description(Thing observer, uint256 item_id, String output);
void render();

#endif
