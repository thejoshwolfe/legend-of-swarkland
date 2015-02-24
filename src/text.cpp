#include "text.hpp"

#include "display.hpp"

#include <SDL2/SDL_ttf.h>

// apparently this is how you use SDL :/
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
static const Uint32 rmask = 0xff000000;
static const Uint32 gmask = 0x00ff0000;
static const Uint32 bmask = 0x0000ff00;
static const Uint32 amask = 0x000000ff;
#else
static const Uint32 rmask = 0x000000ff;
static const Uint32 gmask = 0x0000ff00;
static const Uint32 bmask = 0x00ff0000;
static const Uint32 amask = 0xff000000;
#endif
static SDL_Surface * create_surface(int w, int h) {
    return SDL_CreateRGBSurface(0, w, h, 32, rmask, gmask, bmask, amask);
}
static inline Uint32 mask_color(Uint8 value, Uint32 mask) {
    return ((value << 0) |
            (value << 8) |
            (value << 16) |
            (value << 24)) & mask;
}
static inline Uint32 pack_color(SDL_Color color) {
    return mask_color(color.r, rmask) |
           mask_color(color.g, gmask) |
           mask_color(color.b, bmask) |
           mask_color(color.a, amask);
}


void SpanImpl::render_texture(SDL_Renderer * renderer) {
    ByteBuffer utf8;
    for (int i = 0; i < items.length(); i++) {
        if (items[i].span != NULL)
            panic("TODO: support nested spans");
        items[i].string->encode(&utf8);
    }
    if (utf8.length() == 0)
        return;

    SDL_Surface * text_surface = TTF_RenderUTF8_Blended(status_box_font, utf8.raw(), _foreground);
    SDL_Rect bounds = {0, 0, text_surface->w, text_surface->h};
    SDL_Surface * surface = create_surface(bounds.w, bounds.h);
    SDL_FillRect(surface, &bounds, pack_color(_background));
    SDL_BlitSurface(text_surface, &bounds, surface, &bounds);

    _texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    SDL_FreeSurface(text_surface);
}
