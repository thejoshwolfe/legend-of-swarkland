#ifndef TEXT_HPP
#define TEXT_HPP

#include "string.hpp"

#include <SDL2/SDL.h>

static inline bool operator==(SDL_Color a, SDL_Color b) {
    return a.r == b.r &&
           a.b == b.b &&
           a.g == b.g &&
           a.a == b.a;
}

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
    SpanImpl(SDL_Color foreground, SDL_Color background) :
            _foreground(foreground), _background(background) {
    }
    ~SpanImpl() {
        dispose_texture();
    }

    void set_color(SDL_Color foreground, SDL_Color background) {
        if (_foreground == foreground && _background == background)
            return;
        _foreground = foreground;
        _background = background;
        dispose_texture();
    }
    void set_text(String new_text) {
        String old_text = get_plain_text();
        if (*old_text == *new_text)
            return; // no change
        items.clear();
        items.append(StringOrSpan{new_text, NULL});
        dispose_texture();
    }
    SDL_Texture * get_texture(SDL_Renderer * renderer) {
        if (_texture == NULL)
            render_texture(renderer);
        return _texture;
    }
    // NULL if there are nested spans.
    String get_plain_text() const {
        String result = new_string();
        for (int i = 0; i < items.length(); i++) {
            if (items[i].span != NULL)
                return NULL;
            result->append(items[i].string);
        }
        return result;
    }
private:
    SDL_Texture * _texture = NULL;
    SDL_Color _foreground;
    SDL_Color _background;
    void render_texture(SDL_Renderer * renderer);
    void dispose_texture() {
        if (_texture != NULL)
            SDL_DestroyTexture(_texture);
        _texture = NULL;
    }
};

static inline Span new_span(String string, SDL_Color foreground, SDL_Color background) {
    Span result = create<SpanImpl>(foreground, background);
    result->items.append(StringOrSpan{string, NULL});
    return result;
}

#endif
