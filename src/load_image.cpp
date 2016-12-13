#include "load_image.hpp"

#include "util.hpp"
#include "sdl_graphics.hpp"
#include "resources.hpp"
#include "spritesheet.hpp"

SDL_Surface * load_texture() {
    size_t size = get_binary_spritesheet_resource_size();
    unsigned char * image_buffer = get_binary_spritesheet_resource_start();

    assert(spritesheet_width * spritesheet_height * 4 == (int)size);

    uint32_t pitch = spritesheet_width * 4;

    // this surface does not copy the pixels
    SDL_Surface * tmp_surface = SDL_CreateRGBSurfaceFrom(image_buffer,
        spritesheet_width, spritesheet_height,
        32, pitch,
        color_rmask, color_gmask, color_bmask, color_amask);
    // this surface will hold a copy of the pixels
    SDL_Surface * surface = create_surface(spritesheet_width, spritesheet_height);
    SDL_BlitSurface(tmp_surface, nullptr, surface, nullptr);
    SDL_FreeSurface(tmp_surface);

    return surface;
}

