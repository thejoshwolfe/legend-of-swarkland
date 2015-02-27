#include "text.hpp"

#include "display.hpp"

#include <SDL2/SDL_ttf.h>

static SDL_Surface * create_surface(int w, int h) {
    return SDL_CreateRGBSurface(0, w, h, 32, color_rmask, color_gmask, color_bmask, color_amask);
}

void SpanImpl::render_surface() {
    if (_surface != NULL)
        return;
    List<SDL_Surface*> render_surfaces;
    List<SDL_Surface*> delete_surfaces;
    for (int i = 0; i < _items.length(); i++) {
        if (_items[i].span != NULL) {
            Span sub_span = _items[i].span;
            sub_span->render_surface();
            if (sub_span->_surface == NULL)
                continue;
            render_surfaces.append(sub_span->_surface);
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
        return;

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
}
void SpanImpl::render_texture(SDL_Renderer * renderer) {
    if (_texture != NULL)
        return;
    render_surface();
    if (_surface != NULL)
        _texture = SDL_CreateTextureFromSurface(renderer, _surface);
}
