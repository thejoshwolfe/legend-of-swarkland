#include "load_image.hpp"

#include "util.hpp"
#include "text.hpp"
#include "resources.hpp"
#include "spritesheet.hpp"

void load_texture(SDL_Renderer * renderer, SDL_Texture ** output_texture, SDL_Surface ** output_surface) {
    size_t size = get_binary_spritesheet_resource_size();
    unsigned char * image_buffer = get_binary_spritesheet_resource_start();

    assert(spritesheet_width * spritesheet_height * 4 == (int)size);

    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STATIC, spritesheet_width, spritesheet_height);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    uint32_t pitch = spritesheet_width * 4;

    SDL_UpdateTexture(texture, nullptr, image_buffer, pitch);

    // this surface does not copy the pixels
    SDL_Surface * tmp_surface = SDL_CreateRGBSurfaceFrom(image_buffer,
        spritesheet_width, spritesheet_height,
        32, pitch,
        color_rmask, color_gmask, color_bmask, color_amask);
    // this surface will hold a copy of the pixels
    SDL_Surface * surface = SDL_CreateRGBSurface(0,
        spritesheet_width, spritesheet_height,
        32,
        color_rmask, color_gmask, color_bmask, color_amask);
    SDL_BlitSurface(tmp_surface, nullptr, surface, nullptr);
    SDL_FreeSurface(tmp_surface);

    *output_texture = texture;
    *output_surface = surface;
}

