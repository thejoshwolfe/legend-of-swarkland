#include "text.hpp"

#include "display.hpp"

#include <SDL2/SDL_ttf.h>

void SpanImpl::render_texture(SDL_Renderer * renderer) {
    ByteBuffer utf8;
    for (int i = 0; i < items.length(); i++) {
        if (items[i].span != NULL)
            panic("TODO: support nested spans");
        items[i].string->encode(&utf8);
    }
    if (utf8.length() == 0)
        return;

    SDL_Surface * surface = TTF_RenderUTF8_Blended(status_box_font, utf8.raw(), foreground);
    _texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
}
