#include "load_image.hpp"

#include "util.hpp"

#include <FreeImage.h>

SDL_Texture * load_texture(SDL_Renderer * renderer, struct RuckSackTexture * rs_texture) {
    long size = rucksack_texture_size(rs_texture);
    unsigned char * image_buffer = (unsigned char *)panic_malloc(size, sizeof(char));
    if (rucksack_texture_read(rs_texture, image_buffer) != RuckSackErrorNone) {
        panic("read texture failed");
    }

    FIMEMORY * fi_mem = FreeImage_OpenMemory(image_buffer, size);
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(fi_mem, 0);

    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif)) {
        panic("unable to decode spritesheet image");
    }

    FIBITMAP * bmp = FreeImage_LoadFromMemory(fif, fi_mem, 0);
    if (!FreeImage_HasPixels(bmp)) {
        panic("spritesheet image has no pixels");
    }

    int spritesheet_width = FreeImage_GetWidth(bmp);
    int spritesheet_height = FreeImage_GetHeight(bmp);

    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, spritesheet_width, spritesheet_height);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    SDL_UpdateTexture(texture, NULL, FreeImage_GetBits(bmp), FreeImage_GetPitch(bmp));

    FreeImage_Unload(bmp);
    FreeImage_CloseMemory(fi_mem);
    free(image_buffer);

    return texture;
}

