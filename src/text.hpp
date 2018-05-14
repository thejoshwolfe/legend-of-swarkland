#ifndef TEXT_HPP
#define TEXT_HPP

#include "geometry.hpp"
#include "string.hpp"
#include "sdl_graphics.hpp"

void init_text();
void text_finish();

class SpanImpl;
class DivImpl;

typedef Reference<SpanImpl> Span;
class SpanImpl : public ReferenceCounted {
private:
    class ChildElement {
    public:
        String string;
        SDL_Texture * string_texture = nullptr;
        Span span;
        Rect image;

        ChildElement() :
            image(Rect::nowhere()) {
        }
        ChildElement(String string) :
            string(string), image(Rect::nowhere()) {
        }
        ChildElement(Span span) :
            span(span), image(Rect::nowhere()) {
        }
        ChildElement(Rect image) :
            image(image) {
        }
        ~ChildElement() {
            clear_cached_texture();
        }
        void clear_cached_texture() {
            if (string_texture != nullptr)
                SDL_DestroyTexture(string_texture);
        }
        ChildElement & operator=(const ChildElement & other) {
            if (this != &other) {
                clear_cached_texture();
                this->string = other.string;
                this->string_texture = other.string_texture;
                this->span = other.span;
                this->image = other.image;
            }
            return *this;
        }
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

    void set_color(SDL_Color foreground, SDL_Color background) {
        if (_foreground == foreground && _background == background)
            return;
        _foreground = foreground;
        _background = background;
        clear_cache();
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
            return;
        }
        // set to non blank
        if (!(_items.length() == 1 &&_items[0].string != nullptr)) {
            // no, make it plain text.
            _items.clear();
            _items.append(ChildElement(new_string()));
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
        clear_cache();
    }
    void append(const char * str) {
        append(new_string(str));
        _size = Coord::nowhere();
    }
    void append(const String & text) {
        if (text->length() == 0)
            return;
        if (_items.length() == 0 || _items[_items.length() - 1].string == nullptr)
            _items.append(ChildElement(new_string()));
        _items[_items.length() - 1].string->append(text);
        _size = Coord::nowhere();
    }
    void append(const Span & span) {
        _items.append(ChildElement(span));
        _size = Coord::nowhere();
    }
    void append(Rect image) {
        _items.append(ChildElement(image));
        _size = Coord::nowhere();
    }
    void format(const char * fmt) {
        // check for too many %s
        find_percent_something(fmt, '\0');
        append(fmt);
        _size = Coord::nowhere();
    }
    template<typename ...Args>
    void format(const char * fmt, Span span1, Args... args);
    template<typename ...Args>
    void format(const char * fmt, Rect image, Args... args);

    void to_string(String output_string) const {
        for (int i = 0; i < _items.length(); i++) {
            if (_items[i].string != nullptr) {
                output_string->append(_items[i].string);
            } else if (_items[i].span != nullptr) {
                _items[i].span->to_string(output_string);
            } else if (_items[i].image != Rect::nowhere()) {
                // sorry.
            } else unreachable();
        }
    }

    bool operator==(const SpanImpl & other) const {
        if (this == &other)
            return true;
        return _items == other._items &&
               _foreground == other._foreground &&
               _background == other._background;
    }
    bool operator!=(const SpanImpl & other) const {
        return !(*this == other);
    }

    Coord compute_size(SDL_Renderer * renderer);
    void render(SDL_Renderer * renderer, Coord position);
private:
    List<ChildElement> _items;
    SDL_Color _foreground = white;
    SDL_Color _background = black;
    Coord _size = Coord::nowhere();
    void clear_cache() {
        for (int i = 0; i < _items.length(); i++)
            _items[i].clear_cached_texture();
        _size = Coord::nowhere();
    }

    friend class DivImpl;
    SpanImpl(SpanImpl & copy) = delete;
    SpanImpl & operator=(SpanImpl & other) = delete;
    SDL_Texture * render_string_item(SDL_Renderer * renderer, SpanImpl::ChildElement * element);
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
void SpanImpl::format(const char * fmt, Rect image, Args... args) {
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

    void set_content(Span span) {
        if (_items.length() == 1 && *_items[0].span == *span)
            return;
        _items.clear();
        _items.append(SpanOrSpace{span, 0});
        clear_cache();
    }
    void append(Span span) {
        _items.append(SpanOrSpace{span, 0});
        clear_cache();
    }
    void append_newline() {
        _items.append(SpanOrSpace{nullptr, -1});
        clear_cache();
    }
    void append_spaces(int count) {
        _items.append(SpanOrSpace{nullptr, count});
        clear_cache();
    }
    void clear() {
        _items.clear();
        clear_cache();
    }

    void set_content(const Div & copy_this_guy) {
        if (_items == copy_this_guy->_items)
            return;
        _items.clear();
        _items.append_all(copy_this_guy->_items);
        clear_cache();
    }

    void set_max_width(int max_width) {
        if (_max_width == max_width)
            return;
        _max_width = max_width;
        clear_cache();
    }

    Coord compute_size(SDL_Renderer * renderer);

    void render(SDL_Renderer * renderer, Coord position);
private:
    static const int text_width = 8;
    static const int text_height = 16;
    List<SpanOrSpace> _items;
    SDL_Color _background = black;
    int _max_width = 0;
    List<SpanOrSpace> _cached_wrapped_items;

    void compute_wrapped_items(SDL_Renderer * renderer);
    void clear_cache() {
        _cached_wrapped_items.clear();
    }

    DivImpl(DivImpl & copy) = delete;
    DivImpl & operator=(DivImpl & other) = delete;
};

static inline Div new_div() {
    return create<DivImpl>();
}

#endif
