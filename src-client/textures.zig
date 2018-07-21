const std = @import("std");
const sdl = @import("./sdl.zig");

pub const sprites = @import("../zig-cache/spritesheet.zig");
pub const fonts = @import("../zig-cache/fontsheet.zig");

var sprites_texture: *sdl.c.SDL_Texture = undefined;
var fonts_texture: *sdl.c.SDL_Texture = undefined;
pub fn init(renderer: *sdl.c.SDL_Renderer) void {
    sprites_texture = load_texture(renderer, sprites.buffer, sprites.width, sprites.height);
    fonts_texture = load_texture(renderer, fonts.buffer, fonts.width, fonts.height);
}

pub fn deinit() void {
    sdl.c.SDL_DestroyTexture(sprites_texture);
    sdl.c.SDL_DestroyTexture(fonts_texture);
}

fn load_texture(renderer: *sdl.c.SDL_Renderer, buffer: []const u8, width: i32, height: i32) *sdl.c.SDL_Texture {
    var texture: *sdl.c.SDL_Texture = sdl.c.SDL_CreateTexture(renderer, sdl.c.SDL_PIXELFORMAT_RGBA8888, sdl.c.SDL_TEXTUREACCESS_STATIC, width, height) orelse {
        std.debug.panic("SDL_CreateTexture failed: {c}\n", sdl.c.SDL_GetError());
    };

    if (sdl.c.SDL_SetTextureBlendMode(texture, @intToEnum(sdl.c.SDL_BlendMode, sdl.c.SDL_BLENDMODE_BLEND)) != 0) {
        std.debug.panic("SDL_SetTextureBlendMode failed: {c}\n", sdl.c.SDL_GetError());
    }

    const pitch = width * 4;
    if (sdl.c.SDL_UpdateTexture(texture, null, @ptrCast(?*const c_void, buffer[0..].ptr), pitch) != 0) {
        std.debug.panic("SDL_UpdateTexture failed: {c}\n", sdl.c.SDL_GetError());
    }

    return texture;
}

pub fn render_something_idk(renderer: *sdl.c.SDL_Renderer) void {
    _ = sdl.c.SDL_RenderCopy(renderer, fonts_texture, null, null);
}
