const std = @import("std");
const sdl = @import("./sdl.zig");
const core = @import("core");
const Rect = core.geometry.Rect;
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

pub const version_string = @embedFile("../../zig-cache/version.txt");

pub const sprites = @import("../../zig-cache/spritesheet32.zig");
pub const large_sprites = @import("../../zig-cache/spritesheet200.zig");
pub const fonts = @import("../../zig-cache/fontsheet.zig");

var sprites_texture: *sdl.c.SDL_Texture = undefined;
var large_sprites_texture: *sdl.c.SDL_Texture = undefined;
var fonts_texture: *sdl.c.SDL_Texture = undefined;
pub fn init(renderer: *sdl.Renderer) void {
    sprites_texture = loadTexture(renderer, sprites.buffer, sprites.width, sprites.height);
    large_sprites_texture = loadTexture(renderer, large_sprites.buffer, large_sprites.width, large_sprites.height);
    fonts_texture = loadTexture(renderer, fonts.buffer, fonts.width, fonts.height);
}

pub fn deinit() void {
    sdl.c.SDL_DestroyTexture(sprites_texture);
    sdl.c.SDL_DestroyTexture(large_sprites_texture);
    sdl.c.SDL_DestroyTexture(fonts_texture);
}

fn loadTexture(renderer: *sdl.Renderer, buffer: []const u8, width: i32, height: i32) *sdl.c.SDL_Texture {
    var texture: *sdl.c.SDL_Texture = sdl.c.SDL_CreateTexture(renderer, sdl.c.SDL_PIXELFORMAT_RGBA8888, sdl.c.SDL_TEXTUREACCESS_STATIC, width, height) orelse {
        std.debug.panic("SDL_CreateTexture failed: {s}\n", .{sdl.c.SDL_GetError()});
    };

    if (sdl.c.SDL_SetTextureBlendMode(texture, sdl.c.SDL_BLENDMODE_BLEND) != 0) {
        std.debug.panic("SDL_SetTextureBlendMode failed: {s}\n", .{sdl.c.SDL_GetError()});
    }

    const pitch = width * 4;
    if (sdl.c.SDL_UpdateTexture(texture, 0, @ptrCast(?*const c_void, buffer.ptr), pitch) != 0) {
        std.debug.panic("SDL_UpdateTexture failed: {s}\n", .{sdl.c.SDL_GetError()});
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

pub fn getCharRect(c: u8, bold: bool) Rect {
    return (if (bold) fonts.console_bold else fonts.console)[c - ' '];
}

fn renderChar(renderer: *sdl.Renderer, c: u8, location: Coord, bold: bool, scale: i32) Coord {
    std.debug.assert(scale > 0);
    const char_rect = getCharRect(c, bold);
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = char_rect.width * scale,
        .height = char_rect.height * scale,
    };

    const source_sdl = sdl.makeRect(char_rect);
    const dest_sdl = sdl.makeRect(dest);
    sdl.assertZero(sdl.SDL_RenderCopy(renderer, fonts_texture, &source_sdl, &dest_sdl));

    return makeCoord(dest.x + dest.width, dest.y + dest.height);
}

pub fn renderSprite(renderer: *sdl.Renderer, sprite: Rect, location: Coord) void {
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = sprite.width,
        .height = sprite.height,
    };
    renderSpriteScaled(renderer, sprite, dest);
}
pub fn renderSpriteScaled(renderer: *sdl.Renderer, sprite: Rect, dest: Rect) void {
    const source_sdl = sdl.makeRect(sprite);
    const dest_sdl = sdl.makeRect(dest);
    sdl.assertZero(sdl.SDL_RenderCopy(renderer, sprites_texture, &source_sdl, &dest_sdl));
}
pub fn renderSpriteRotated(renderer: *sdl.Renderer, sprite: Rect, location: Coord, rotation: u3) void {
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = sprite.width,
        .height = sprite.height,
    };

    const source_sdl = sdl.makeRect(sprite);
    const dest_sdl = sdl.makeRect(dest);

    const angle = @intToFloat(f64, rotation) * 45.0;
    sdl.assertZero(sdl.SDL_RenderCopyEx(renderer, sprites_texture, &source_sdl, &dest_sdl, angle, null, sdl.c.SDL_FLIP_NONE));
}

pub fn renderLargeSprite(renderer: *sdl.Renderer, large_sprite: Rect, location: Coord) void {
    const dest = Rect{
        .x = location.x,
        .y = location.y,
        .width = large_sprite.width,
        .height = large_sprite.height,
    };

    const source_sdl = sdl.makeRect(large_sprite);
    const dest_sdl = sdl.makeRect(dest);
    sdl.assertZero(sdl.SDL_RenderCopy(renderer, large_sprites_texture, &source_sdl, &dest_sdl));
}
