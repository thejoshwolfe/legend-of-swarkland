const std = @import("std");
const sdl = @import("./sdl.zig");
const Rect = @import("core").geometry.Rect;
const Coord = @import("core").geometry.Coord;
const makeCoord = @import("core").geometry.makeCoord;

pub const sprites = @import("../zig-cache/spritesheet.zig");
pub const fonts = @import("../zig-cache/fontsheet.zig");

var sprites_texture: *sdl.c.SDL_Texture = undefined;
var fonts_texture: *sdl.c.SDL_Texture = undefined;
pub fn init(renderer: *sdl.Renderer) void {
    sprites_texture = load_texture(renderer, sprites.buffer, sprites.width, sprites.height);
    fonts_texture = load_texture(renderer, fonts.buffer, fonts.width, fonts.height);
}

pub fn deinit() void {
    sdl.c.SDL_DestroyTexture(sprites_texture);
    sdl.c.SDL_DestroyTexture(fonts_texture);
}

fn load_texture(renderer: *sdl.Renderer, buffer: []const u8, width: i32, height: i32) *sdl.c.SDL_Texture {
    var texture: *sdl.c.SDL_Texture = sdl.c.SDL_CreateTexture(renderer, sdl.c.SDL_PIXELFORMAT_RGBA8888, sdl.c.SDL_TEXTUREACCESS_STATIC, width, height) orelse {
        std.debug.panic("SDL_CreateTexture failed: {c}\n", sdl.c.SDL_GetError());
    };

    if (sdl.c.SDL_SetTextureBlendMode(texture, @intToEnum(sdl.c.SDL_BlendMode, sdl.c.SDL_BLENDMODE_BLEND)) != 0) {
        std.debug.panic("SDL_SetTextureBlendMode failed: {c}\n", sdl.c.SDL_GetError());
    }

    const pitch = width * 4;
    if (sdl.c.SDL_UpdateTexture(texture, null, @ptrCast(?*const c_void, buffer.ptr), pitch) != 0) {
        std.debug.panic("SDL_UpdateTexture failed: {c}\n", sdl.c.SDL_GetError());
    }

    return texture;
}

pub fn renderTextScaled(renderer: *sdl.Renderer, text: []const u8, location: Coord, bold: bool, scale: i32) Coord {
    var lower_right = location;
    for (text) |c| {
        lower_right = renderChar(renderer, c, makeCoord(lower_right.x, location.y), bold, scale);
    }
    return lower_right;
}

fn renderChar(renderer: *sdl.Renderer, c: u8, location: Coord, bold: bool, scale: i32) Coord {
    std.debug.assert(scale > 0);
    const char_rect = (if (bold) fonts.console_bold else fonts.console)[c - ' '];
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = char_rect.width * scale,
        .height = char_rect.height * scale,
    };

    const source_sdl = sdl.makeRect(char_rect);
    const dest_sdl = sdl.makeRect(dest);
    _ = sdl.SDL_RenderCopy(renderer, fonts_texture, &source_sdl, &dest_sdl);

    return makeCoord(dest.x + dest.width, dest.y + dest.height);
}

pub fn renderSprite(renderer: *sdl.Renderer, sprite: Rect, location: Coord) void {
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = sprite.width,
        .height = sprite.height,
    };

    const source_sdl = sdl.makeRect(sprite);
    const dest_sdl = sdl.makeRect(dest);
    _ = sdl.SDL_RenderCopy(renderer, sprites_texture, &source_sdl, &dest_sdl);
}
