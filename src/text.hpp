#ifndef TEXT_HPP
#define TEXT_HPP

#include "string.hpp"

#include <SDL2/SDL.h>

static const SDL_Color white       = {0xff, 0xff, 0xff, 0xff};
static const SDL_Color black       = {0x00, 0x00, 0x00, 0xff};
static const SDL_Color red         = {0xff, 0x00, 0x00, 0xff};
static const SDL_Color pink        = {0xff, 0x88, 0x88, 0xff};
static const SDL_Color dark_green  = {0x00, 0x88, 0x00, 0xff};
static const SDL_Color amber       = {0xff, 0xbf, 0x00, 0xff};

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
    SpanImpl() {
    }
    ~SpanImpl() {
        dispose_resources();
    }

    void set_color(SDL_Color foreground, SDL_Color background) {
        if (_foreground == foreground && _background == background)
            return;
        _foreground = foreground;
        _background = background;
        dispose_resources();
    }
    void set_text(const String & new_text) {
        if (new_text->length() == 0) {
            // blank it out
            if (_items.length() == 0)
                return; // already blank
            _items.clear();
            dispose_resources();
            return;
        }
        // set to non blank
        if (_items.length() > 1 || (_items.length() == 1 && _items[0].string == NULL)) {
            // too complicated. start over.
            _items.clear();
            dispose_resources();
        }
        if (_items.length() == 0) {
            // starting from scratch
            _items.append(StringOrSpan{new_string(), NULL});
        } else {
            // there's already some plain text here.
            if (*_items[0].string == *new_text) {
                // nothing to do
                return;
            }
            // wrong.
            _items[0].string->clear();
        }
        _items[0].string->append(new_text);
        dispose_resources();
    }
    void append(const char * str) {
        append(new_string(str));
    }
    void append(const String & text) {
        if (text->length() == 0)
            return;
        if (_items.length() == 0 || _items[_items.length() - 1].string == NULL)
            _items.append(StringOrSpan{new_string(), NULL});
        _items[_items.length() - 1].string->append(text);
        dispose_resources();
    }
    void append(const Span & span) {
        _items.append(StringOrSpan{NULL, span});
        dispose_resources();
    }
    void format(const char * fmt) {
        // check for too many %s
        find_percent_something(fmt, '%');
        append(fmt);
    }
    template<typename ...Args>
    void format(const char * fmt, Span span1, Args... args);
    SDL_Texture * get_texture(SDL_Renderer * renderer) {
        render_texture(renderer);
        return _texture;
    }
private:
    List<StringOrSpan> _items;
    SDL_Color _foreground = white;
    SDL_Color _background = black;
    SDL_Texture * _texture = NULL;
    SDL_Surface * _surface = NULL;
    void render_texture(SDL_Renderer * renderer);
    void render_surface();
    void dispose_resources() {
        if (_texture != NULL)
            SDL_DestroyTexture(_texture);
        _texture = NULL;
        if (_surface != NULL)
            SDL_FreeSurface(_surface);
        _surface = NULL;
    }

    SpanImpl(SpanImpl & copy) = delete;
    SpanImpl & operator=(SpanImpl & other) = delete;
};

template<typename ...Args>
void SpanImpl::format(const char * fmt, Span span1, Args... args) {
    int i = find_percent_something(fmt, 's');
    ByteBuffer prefix;
    prefix.append(fmt, i - 2);
    append(new_string(prefix));

    append(span1);

    format(fmt + i, args...);
}

static inline Span new_span() {
    return create<SpanImpl>();
}
static inline Span new_span(String text) {
    Span result = create<SpanImpl>();
    result->append(text);
    return result;
}
static inline Span new_span(const char * str) {
    return new_span(new_string(str));
}

#endif
