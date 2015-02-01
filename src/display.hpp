#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "geometry.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>

static const int tile_size = 32;

void display_init();
void display_finish();

void render();

#endif
