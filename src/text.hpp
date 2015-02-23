#ifndef TEXT_HPP
#define TEXT_HPP

#include "string.hpp"

#include <SDL2/SDL.h>

struct TextureArea {
    SDL_Texture * texture;
    SDL_Rect rect;
};

class SpanImpl;
typedef Reference<SpanImpl> Span;
class StringOrSpan {
public:
    String string;
    Span span;
};

class SpanImpl : public ReferenceCounted {
public:
    List<StringOrSpan> items;
    SDL_Color foreground;
    SDL_Color background;
    SpanImpl(SDL_Color foreground, SDL_Color background) :
            foreground(foreground), background(background) {
    }
    ~SpanImpl() {
        if (texture_area.texture != NULL)
            SDL_DestroyTexture(texture_area.texture);
    }

    TextureArea get_texture_area(SDL_Renderer * renderer) {
        if (texture_area.texture == NULL)
            render_texture(renderer);
        return texture_area;
    }
private:
    TextureArea texture_area = {NULL, {0, 0, 0, 0}};
    void render_texture(SDL_Renderer * renderer);
};

static inline Span new_span(String string, SDL_Color foreground, SDL_Color background) {
    Span result = create<SpanImpl>(foreground, background);
    result->items.append(StringOrSpan{string, NULL});
    return result;
}

#endif
