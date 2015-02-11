#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "individual.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;
extern const SDL_Rect main_map_area;

void display_init(const char * resource_bundle_path);
void display_finish();

Coord get_mouse_tile(SDL_Rect area);
void get_individual_description(Individual observer, uint256 target_id, ByteBuffer * output);
void render();

#endif
