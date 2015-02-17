#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "individual.hpp"

#include <rucksack/rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

void display_init(const char * resource_bundle_path);
void display_finish();

Coord get_mouse_tile(SDL_Rect area);
const char * get_species_name(SpeciesId species_id);
void get_thing_description(Thing observer, uint256 target_id, ByteBuffer * output);
void get_individual_description(Thing observer, uint256 target_id, ByteBuffer * output);
void get_item_description(Thing observer, uint256 item_id, ByteBuffer * output);
void render();

#endif
