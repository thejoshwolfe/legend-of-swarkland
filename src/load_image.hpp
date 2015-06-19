#ifndef LOAD_IMAGE_HPP
#define LOAD_IMAGE_HPP

#include <rucksack/rucksack.h>
#include <SDL.h>

SDL_Texture * load_texture(SDL_Renderer * renderer, struct RuckSackTexture * rs_texture);

#endif
