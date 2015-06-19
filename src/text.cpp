#include "text.hpp"

#include "display.hpp"

#include <SDL_ttf.h>

static SDL_Surface * create_surface(int w, int h) {
    return SDL_CreateRGBSurface(0, w, h, 32, color_rmask, color_gmask, color_bmask, color_amask);
}

SDL_Surface * SpanImpl::get_surface() {
    if (_surface != nullptr)
        return _surface;
    List<SDL_Surface*> render_surfaces;
    List<SDL_Surface*> delete_surfaces;
    for (int i = 0; i < _items.length(); i++) {
        if (_items[i].span != nullptr) {
            Span sub_span = _items[i].span;
            SDL_Surface * sub_surface = sub_span->get_surface();
            if (sub_surface == nullptr)
                continue;
            render_surfaces.append(sub_surface);
        } else {
            ByteBuffer utf8;
            _items[i].string->encode(&utf8);
            if (utf8.length() == 0)
                continue;
            SDL_Surface * tmp_surface = TTF_RenderUTF8_Blended(status_box_font, utf8.raw(), _foreground);
            render_surfaces.append(tmp_surface);
            delete_surfaces.append(tmp_surface);
        }
    }
    if (render_surfaces.length() == 0)
        return nullptr;

    // measure dimensions
    int width = 0;
    for (int i = 0; i < render_surfaces.length(); i++)
        width += render_surfaces[i]->w;
    // height is the same for every span.
    int height = render_surfaces[0]->h;
    SDL_Rect bounds = {0, 0, width, height};
    _surface = create_surface(bounds.w, bounds.h);
    // copy all the sub surfaces
    SDL_FillRect(_surface, &bounds, pack_color(_background));
    int x_cursor = 0;
    for (int i = 0; i < render_surfaces.length(); i++) {
        SDL_Rect src_rect = {0, 0, render_surfaces[i]->w, height};
        SDL_Rect dest_rect = src_rect;
        dest_rect.x = x_cursor;
        SDL_BlitSurface(render_surfaces[i], &src_rect, _surface, &dest_rect);
        x_cursor += dest_rect.w;
    }
    // cleanup
    for (int i = 0; i < delete_surfaces.length(); i++)
        SDL_FreeSurface(delete_surfaces[i]);
    return _surface;
}


void DivImpl::render_surface() {
    if (_surface != nullptr)
        return;
    int total_width = 0;
    int line_width = 0;
    List<SpanOrSpace> wrapped_items;
    for (int i = 0; i < _items.length(); i++) {
        if (_items[i].span != nullptr) {
            SDL_Surface * sub_surface = _items[i].span->get_surface();
            if (sub_surface == nullptr)
                continue;
            if (line_width + sub_surface->w > _max_width) {
                // line break
                wrapped_items.append(SpanOrSpace{nullptr, -1});
                line_width = 0;
            }
            wrapped_items.append(_items[i]);
            line_width += sub_surface->w;
            if (line_width > total_width)
                total_width = line_width;
        } else {
            int space_count = _items[i].space_count;
            if (space_count != -1) {
                line_width += space_count * text_width;
                if (line_width > _max_width) {
                    // nevermind these spaces. wrap instead
                    wrapped_items.append(SpanOrSpace{nullptr, -1});
                    line_width = 0;
                } else {
                    if (line_width > total_width)
                        total_width = line_width;
                }
            } else {
                // newline
                line_width = 0;
            }
            wrapped_items.append(_items[i]);
        }
    }
    if (total_width == 0)
        return;

    // limit the height from the bottom
    int first_visible_index = wrapped_items.length();
    int visible_height = text_height;
    while (true) {
        if (first_visible_index == 0)
            break;
        first_visible_index--;
        if (wrapped_items[first_visible_index].space_count != -1)
            continue;
        if (visible_height + text_height > _max_height) {
            // this one is out of view.
            first_visible_index++;
            break;
        }
        visible_height += text_height;
    }

    // combine the sub surfaces
    SDL_Rect bounds = {0, 0, total_width, visible_height};
    _surface = create_surface(bounds.w, bounds.h);
    SDL_FillRect(_surface, &bounds, pack_color(_background));
    int x_cursor = 0;
    int y_cursor = 0;
    for (int i = first_visible_index; i < wrapped_items.length(); i++) {
        if (wrapped_items[i].span != nullptr) {
            SDL_Surface * sub_surface = wrapped_items[i].span->get_surface();
            if (sub_surface == nullptr)
                continue;
            SDL_Rect src_rect = {0, 0, sub_surface->w, sub_surface->h};
            SDL_Rect dest_rect = {x_cursor, y_cursor, src_rect.w, src_rect.h};
            SDL_BlitSurface(sub_surface, &src_rect, _surface, &dest_rect);
            x_cursor += dest_rect.w;
        } else {
            int space_count = wrapped_items[i].space_count;
            if (space_count != -1) {
                x_cursor += space_count * text_width;
            } else {
                // newline
                x_cursor = 0;
                y_cursor += text_height;
            }
        }
    }
}
