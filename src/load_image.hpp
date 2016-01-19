#ifndef LOAD_IMAGE_HPP
#define LOAD_IMAGE_HPP

#include <rucksack/rucksack.h>
#include <SDL.h>

void load_texture(SDL_Renderer * renderer, struct RuckSackTexture * rs_texture, SDL_Texture ** output_texture, SDL_Surface ** output_surface);

#endif
