#ifndef TEXT_HPP
#define TEXT_HPP

#include "string.hpp"
#include <rucksack/rucksack.h>

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
static const Uint32 color_rmask = 0xff000000;
static const Uint32 color_gmask = 0x00ff0000;
static const Uint32 color_bmask = 0x0000ff00;
static const Uint32 color_amask = 0x000000ff;
#else
static const Uint32 color_rmask = 0x000000ff;
static const Uint32 color_gmask = 0x0000ff00;
static const Uint32 color_bmask = 0x00ff0000;
static const Uint32 color_amask = 0xff000000;
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

static inline bool renderer_is_alive(SDL_Renderer * renderer) {
    SDL_RendererInfo info;
    int error_code = SDL_GetRendererInfo(renderer, &info);
    return error_code == 0;
}

class SpanImpl;
class DivImpl;

typedef Reference<SpanImpl> Span;
class SpanImpl : public ReferenceCounted {
private:
    struct ChildElement {
        String string;
        Span span;
        RuckSackImage * image;
        bool operator==(const ChildElement & other) const {
            return *string == *other.string &&
                   *span == *other.span &&
                   image == other.image;
        }
        bool operator!=(const ChildElement & other) const {
            return !(*this == other);
        }
    };

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
    void set_color_recursive(SDL_Color foreground, SDL_Color background) {
        set_color(foreground, background);
        for (int i = 0; i < _items.length(); i++)
            if (_items[i].span != nullptr)
                _items[i].span->set_color_recursive(foreground, background);
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
        if (!(_items.length() == 1 &&_items[0].string != nullptr)) {
            // no, make it plain text.
            _items.clear();
            _items.append(ChildElement{new_string(), nullptr, nullptr});
            dispose_resources();
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
        if (_items.length() == 0 || _items[_items.length() - 1].string == nullptr)
            _items.append(ChildElement{new_string(), nullptr, nullptr});
        _items[_items.length() - 1].string->append(text);
        dispose_resources();
    }
    void append(const Span & span) {
        _items.append(ChildElement{nullptr, span, nullptr});
        dispose_resources();
    }
    void append(RuckSackImage * image) {
        _items.append(ChildElement{nullptr, nullptr, image});
        dispose_resources();
    }
    void format(const char * fmt) {
        // check for too many %s
        find_percent_something(fmt, '\0');
        append(fmt);
    }
    template<typename ...Args>
    void format(const char * fmt, Span span1, Args... args);
    template<typename ...Args>
    void format(const char * fmt, RuckSackImage * image, Args... args);

    void to_string(String output_string) const {
        for (int i = 0; i < _items.length(); i++) {
            if (_items[i].string != nullptr) {
                output_string->append(_items[i].string);
            } else if (_items[i].span != nullptr) {
                _items[i].span->to_string(output_string);
            } else if (_items[i].image != nullptr) {
                // sorry.
            } else unreachable();
        }
    }

    bool operator==(const SpanImpl & other) const {
        if (this == &other)
            return true;
        if (this == nullptr || &other == nullptr)
            return false;
        return _items == other._items &&
               _foreground == other._foreground &&
               _background == other._background;
    }
    bool operator!=(const SpanImpl & other) const {
        return !(*this == other);
    }
private:
    List<ChildElement> _items;
    SDL_Color _foreground = white;
    SDL_Color _background = black;
    SDL_Surface * _surface = nullptr;
    SDL_Surface * get_surface();
    void dispose_resources() {
        if (_surface != nullptr)
            SDL_FreeSurface(_surface);
        _surface = nullptr;
    }

    friend class DivImpl;
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
template<typename ...Args>
void SpanImpl::format(const char * fmt, RuckSackImage * image, Args... args) {
    int i = find_percent_something(fmt, 'g');
    ByteBuffer prefix;
    prefix.append(fmt, i - 2);
    append(new_string(prefix));

    append(image);

    format(fmt + i, args...);
}

static inline Span new_span() {
    return create<SpanImpl>();
}
static inline Span new_span(const String & text) {
    Span result = create<SpanImpl>();
    result->append(text);
    return result;
}
static inline Span new_span(const char * str) {
    return new_span(new_string(str));
}
static inline Span new_span(const char * str, SDL_Color foreground, SDL_Color background) {
    Span result = new_span(new_string(str));
    result->set_color(foreground, background);
    return result;
}

typedef Reference<DivImpl> Div;
class DivImpl : public ReferenceCounted {
private:
    struct SpanOrSpace {
        Span span;
        // 0 means not space. -1 means newline. >0 means spaces.
        int space_count;
        bool operator==(const SpanOrSpace & other) const {
            return *span == *other.span && space_count == other.space_count;
        }
        bool operator!=(const SpanOrSpace & other) const {
            return !(*this == other);
        }
    };
public:
    DivImpl() {
    }
    ~DivImpl() {
        dispose_resources();
    }

    void set_content(Span span) {
        if (_items.length() == 1 && *_items[0].span == *span)
            return;
        _items.clear();
        _items.append(SpanOrSpace{span, 0});
        dispose_resources();
    }
    void append(Span span) {
        _items.append(SpanOrSpace{span, 0});
        dispose_resources();
    }
    void append_newline() {
        _items.append(SpanOrSpace{nullptr, -1});
        dispose_resources();
    }
    void append_spaces(int count) {
        _items.append(SpanOrSpace{nullptr, count});
        dispose_resources();
    }
    void clear() {
        _items.clear();
        dispose_resources();
    }

    void set_max_size(int max_width, int max_height) {
        if (_max_width == max_width && _max_height == max_height)
            return;
        _max_width = max_width;
        _max_height = max_height;
        dispose_resources();
    }

    void set_content(const Div & copy_this_guy) {
        if (_items == copy_this_guy->_items)
            return;
        _items.clear();
        _items.append_all(copy_this_guy->_items);
        dispose_resources();
    }

    SDL_Texture * get_texture(SDL_Renderer * renderer) {
        _renderer = renderer;
        if (_texture == nullptr) {
            render_surface();
            if (_surface != nullptr)
                _texture = SDL_CreateTextureFromSurface(renderer, _surface);
        }
        return _texture;
    }
private:
    static const int text_width = 8;
    static const int text_height = 16;
    List<SpanOrSpace> _items;
    SDL_Color _background = black;
    SDL_Surface * _surface = nullptr;
    SDL_Texture * _texture = nullptr;
    SDL_Renderer * _renderer = nullptr;
    int _max_width = 0;
    int _max_height = 0;
    void render_surface();
    void dispose_resources() {
        if (_surface != nullptr)
            SDL_FreeSurface(_surface);
        _surface = nullptr;
        if (_texture != nullptr) {
            // if the renderer has already been shutdown, then the texture has already been destroyed.
            if (renderer_is_alive(_renderer))
                SDL_DestroyTexture(_texture);
        }
        _texture = nullptr;
    }

    DivImpl(DivImpl & copy) = delete;
    DivImpl & operator=(DivImpl & other) = delete;
};

static inline Div new_div() {
    return create<DivImpl>();
}

#endif
