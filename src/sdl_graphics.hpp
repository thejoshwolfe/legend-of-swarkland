#ifndef SDL_GRAPHICS_H
#define SDL_GRAPHICS_H

#include "geometry.hpp"

#include <SDL.h>

static const SDL_Color white       = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color gray        = {0x88, 0x88, 0x88, 0xff};
static const SDL_Color black       = {0x00, 0x00, 0x00, 0xff};
static const SDL_Color red         = {0xff, 0x00, 0x00, 0xff};
static const SDL_Color light_blue  = {0x88, 0x88, 0xff, 0xff};
static const SDL_Color pink        = {0xff, 0x88, 0x88, 0xff};
static const SDL_Color dark_green  = {0x00, 0x88, 0x00, 0xff};
static const SDL_Color light_green = {0x88, 0xff, 0x88, 0xff};
static const SDL_Color amber       = {0xff, 0xbf, 0x00, 0xff};
static const SDL_Color light_brown = {0xff, 0xa8, 0x00, 0xff};

// i wonder if this difference even matters
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
static const Uint32 color_rmask = 0x000000ff;
static const Uint32 color_gmask = 0x0000ff00;
static const Uint32 color_bmask = 0x00ff0000;
static const Uint32 color_amask = 0xff000000;
#else
static const Uint32 color_rmask = 0xff000000;
static const Uint32 color_gmask = 0x00ff0000;
static const Uint32 color_bmask = 0x0000ff00;
static const Uint32 color_amask = 0x000000ff;
#endif
static constexpr Uint32 pack_color(SDL_Color color) {
    struct LocalFunctionPlease {
        static constexpr Uint32 mask_color(Uint8 value, Uint32 mask) {
            return ((value << 0) |
                    (value << 8) |
                    (value << 16) |
                    (value << 24)) & mask;
        }
    };
    return LocalFunctionPlease::mask_color(color.r, color_rmask) |
           LocalFunctionPlease::mask_color(color.g, color_gmask) |
           LocalFunctionPlease::mask_color(color.b, color_bmask) |
           LocalFunctionPlease::mask_color(color.a, color_amask);
}
static constexpr bool operator==(SDL_Color a, SDL_Color b) {
    return pack_color(a) == pack_color(b);
}
static constexpr bool operator!=(SDL_Color a, SDL_Color b) {
    return !(a == b);
}

static inline Coord get_texture_size(SDL_Texture * texture) {
    SDL_Rect result = {0, 0, 0, 0};
    Uint32 format;
    int access;
    SDL_QueryTexture(texture, &format, &access, &result.w, &result.h);
    return Coord{result.w, result.h};
}

#endif
