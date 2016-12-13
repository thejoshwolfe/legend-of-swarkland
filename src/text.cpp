#include "text.hpp"

#include "display.hpp"

#include <SDL_ttf.h>

SDL_Texture * SpanImpl::render_string_item(SDL_Renderer * renderer, SpanImpl::ChildElement * element) {
    if (element->string_texture == nullptr && element->string->length() > 0) {
        ByteBuffer utf8;
        element->string->encode(&utf8);
        SDL_Surface * tmp_surface = TTF_RenderUTF8_Blended(status_box_font, utf8.raw(), _foreground);
        element->string_texture = SDL_CreateTextureFromSurface(renderer, tmp_surface);
        SDL_FreeSurface(tmp_surface);
    }
    return element->string_texture;
}

Coord SpanImpl::compute_size(SDL_Renderer * renderer) {
    if (_size == Coord::nowhere()) {
        _size = Coord{0, 0};
        for (int i = 0; i < _items.length(); i++) {
            Coord sub_size;
            if (_items[i].span != nullptr) {
                sub_size = _items[i].span->compute_size(renderer);
            } else if (_items[i].string != nullptr) {
                SDL_Texture * sub_texture = render_string_item(renderer, &_items[i]);
                if (sub_texture == nullptr)
                    continue;
                sub_size = get_texture_size(sub_texture);
            } else if (_items[i].image != SwarklandImage_::nowhere()) {
                sub_size = Coord{tile_size, tile_size};
            } else {
                unreachable();
            }

            _size.x += sub_size.x;
            if (sub_size.y > _size.y)
                _size.y = sub_size.y;
        }
    }
    return _size;
}

void SpanImpl::render(SDL_Renderer * renderer, Coord position) {
    {
        Coord size = compute_size(renderer);
        SDL_SetRenderDrawColor(renderer, _background.r, _background.g, _background.b, _background.a);
        SDL_Rect dest_rect = {position.x, position.y, size.x, size.y};
        SDL_RenderFillRect(renderer, &dest_rect);
    }

    Coord cursor = position;
    for (int i = 0; i < _items.length(); i++) {
        int delta_x;
        if (_items[i].span != nullptr) {
            Span sub_span = _items[i].span;
            delta_x = sub_span->compute_size(renderer).x;
            sub_span->render(renderer, cursor);
        } else if (_items[i].string != nullptr) {
            SDL_Texture * sub_texture = render_string_item(renderer, &_items[i]);
            if (sub_texture == nullptr)
                continue;
            Coord sub_size = get_texture_size(sub_texture);
            SDL_Rect dest_rect = {cursor.x, cursor.y, sub_size.x, sub_size.y};
            SDL_RenderCopy(renderer, sub_texture, nullptr, &dest_rect);
            delta_x = sub_size.x;
        } else if (_items[i].image != SwarklandImage_::nowhere()) {
            SDL_Rect dest_rect = {cursor.x, cursor.y, tile_size, tile_size};
            SDL_Rect src_rect = {_items[i].image.x, _items[i].image.y, tile_size, tile_size};
            SDL_RenderCopy(renderer, sprite_sheet_texture, &src_rect, &dest_rect);
            delta_x = tile_size;
        } else {
            unreachable();
        }
        cursor.x += delta_x;
    }
    assert(position.x + _size.x == cursor.x);
}


void DivImpl::compute_wrapped_items(SDL_Renderer * renderer) {
    if (_cached_wrapped_items.length() > 0)
        return;
    int line_width = 0;
    for (int i = 0; i < _items.length(); i++) {
        if (_items[i].span != nullptr) {
            Coord sub_size = _items[i].span->compute_size(renderer);
            if (line_width + sub_size.x > _max_width) {
                // line break
                _cached_wrapped_items.append(SpanOrSpace{nullptr, -1});
                line_width = 0;
            }
            _cached_wrapped_items.append(_items[i]);
            line_width += sub_size.x;
        } else {
            int space_count = _items[i].space_count;
            if (space_count != -1) {
                line_width += space_count * text_width;
                if (line_width > _max_width) {
                    // nevermind these spaces. wrap instead
                    _cached_wrapped_items.append(SpanOrSpace{nullptr, -1});
                    line_width = 0;
                }
            } else {
                // newline
                line_width = 0;
            }
            _cached_wrapped_items.append(_items[i]);
        }
    }
}

Coord DivImpl::compute_size(SDL_Renderer * renderer) {
    compute_wrapped_items(renderer);
    Coord size = {0, 0};
    int line_width = 0;
    int line_height = 0;
    for (int i = 0; i < _cached_wrapped_items.length(); i++) {
        int item_height = 0;
        if (_cached_wrapped_items[i].span != nullptr) {
            Coord sub_size = _cached_wrapped_items[i].span->compute_size(renderer);
            line_width += sub_size.x;
            item_height = sub_size.y;
        } else {
            int space_count = _cached_wrapped_items[i].space_count;
            if (space_count != -1) {
                line_width += space_count * text_width;
                item_height = text_height;
            } else {
                // newline
                size.y += line_height;
                line_width = 0;
                line_height = 0;
            }
        }
        if (line_width > size.x)
            size.x = line_width;
        if (item_height > line_height)
            line_height = item_height;
    }
    size.y += line_height;
    return size;
}

void DivImpl::render(SDL_Renderer * renderer, Coord position) {
    {
        Coord size = compute_size(renderer);
        SDL_SetRenderDrawColor(renderer, _background.r, _background.g, _background.b, _background.a);
        SDL_Rect dest_rect = {position.x, position.y, size.x, size.y};
        SDL_RenderFillRect(renderer, &dest_rect);
    }

    Coord cursor = position;
    int line_height = 0;
    for (int i = 0; i < _cached_wrapped_items.length(); i++) {
        int item_height = 0;
        if (_cached_wrapped_items[i].span != nullptr) {
            Span sub_span = _cached_wrapped_items[i].span;
            Coord sub_size = sub_span->compute_size(renderer);
            sub_span->render(renderer, cursor);

            cursor.x += sub_size.x;
            item_height = sub_size.y;
        } else {
            int space_count = _cached_wrapped_items[i].space_count;
            if (space_count != -1) {
                cursor.x += space_count * text_width;
                item_height = text_height;
            } else {
                // newline
                cursor.y += line_height;
                cursor.x = position.x;
                line_height = 0;
            }
        }
        if (item_height > line_height)
            line_height = item_height;
    }
}
