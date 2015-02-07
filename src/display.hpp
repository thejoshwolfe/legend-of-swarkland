#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"
#include "byte_buffer.hpp"
#include "individual.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;

extern bool expand_message_box;

void display_init();
void display_finish();

void on_mouse_motion();
Coord get_mouse_tile();
void get_individual_description(Individual observer, uint256 target_id, ByteBuffer * output);
void render();

#endif
