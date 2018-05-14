#include "load_image.hpp"

#include "util.hpp"
#include "resources.hpp"
#include "spritesheet.hpp"

SDL_Texture * load_texture(SDL_Renderer * renderer) {
    size_t size = get_binary_spritesheet_resource_size();
    unsigned char * image_buffer = get_binary_spritesheet_resource_start();

    assert(spritesheet_width * spritesheet_height * 4 == (int)size);

    uint32_t pitch = spritesheet_width * 4;

    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STATIC, spritesheet_width, spritesheet_height);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, image_buffer, pitch);

    return texture;
}

