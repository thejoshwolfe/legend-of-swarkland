#ifndef LOAD_IMAGE_HPP
#define LOAD_IMAGE_HPP

#include <SDL.h>
#include "image.hpp"

void load_texture(SDL_Renderer * renderer, SDL_Texture ** output_texture, SDL_Surface ** output_surface);

#endif
