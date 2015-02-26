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
    void set_text(String new_text) {
        if (is_plain_text()) {
            if (*_plain_text == *new_text)
                return; // no change
        }
        _items.clear();
        _plain_text->clear();
        _plain_text->append(new_text);
        dispose_resources();
    }
    void append(const char * str) {
        append(new_string(str));
    }
    void append(String text) {
        if (text->length() == 0)
            return; // fuck you.
        if (is_plain_text()) {
            _plain_text->append(text);
        } else {
            // need to use a plain text sub span
            if (!_items[_items.length() - 1]->is_plain_text())
                _items.append(create<SpanImpl>());
            _items[_items.length() - 1]->append(text);
        }
        dispose_resources();
    }
    void append(Span span) {
        if (is_plain_text()) {
            // convert to rich text
            if (_plain_text->length() > 0) {
                // save the old plain text in a span
                Span sub_span = create<SpanImpl>();
                sub_span->append(_plain_text);
                _items.append(sub_span);
            }
            _plain_text = NULL;
        }
        _items.append(span);
        dispose_resources();
    }
    template<typename ...Args>
    void format(const char * fmt, Span span1, Args... args);
    void format(const char * fmt) {
        // check for too many %s
        find_percent_something(fmt, '%');
        append(fmt);
    }

    SDL_Texture * get_texture(SDL_Renderer * renderer) {
        render_texture(renderer);
        return _texture;
    }
    bool is_plain_text() const {
        return _items.length() == 0;
    }
private:
    // only 1 of plain_text or items can be non-empty
    String _plain_text = new_string();
    List<Span> _items;

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
