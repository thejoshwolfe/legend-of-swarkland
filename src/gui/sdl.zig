const std = @import("std");
const sdl = @This();

// this is technically all we need
pub const c = @cImport({
    @cInclude("SDL2/SDL.h");
});
// but for convenience, we'll publish some special case wrappers/aliases
// to isolate the quirks of using a C api from zig.

// See https://github.com/zig-lang/zig/issues/565
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
// SDL_video.h:#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000u
pub const SDL_WINDOWPOS_UNDEFINED = @bitCast(c_int, sdl.c.SDL_WINDOWPOS_UNDEFINED_MASK);

pub extern fn SDL_PollEvent(event: *sdl.c.SDL_Event) c_int;

pub extern fn SDL_RenderCopy(
    renderer: *sdl.c.SDL_Renderer,
    texture: *sdl.c.SDL_Texture,
    srcrect: ?*const sdl.c.SDL_Rect,
    dstrect: ?*const sdl.c.SDL_Rect,
) c_int;
pub extern fn SDL_RenderCopyEx(
    renderer: *sdl.c.SDL_Renderer,
    texture: *sdl.c.SDL_Texture,
    srcrect: ?*const sdl.c.SDL_Rect,
    dstrect: ?*const sdl.c.SDL_Rect,
    angle: f64,
    center: ?*const sdl.c.SDL_Point,
    flip: c_int, // SDL_RendererFlip
) c_int;

const geometry = @import("core").geometry;

pub fn makeRect(rect: geometry.Rect) sdl.c.SDL_Rect {
    return sdl.c.SDL_Rect{
        .x = rect.x,
        .y = rect.y,
        .w = rect.width,
        .h = rect.height,
    };
}

pub fn assertZero(ret: c_int) void {
    if (ret == 0) return;
    std.debug.panic("sdl function returned an error: {c}", sdl.c.SDL_GetError());
}

pub const Renderer = sdl.c.SDL_Renderer;
pub const Texture = sdl.c.SDL_Texture;
