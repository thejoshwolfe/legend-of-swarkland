#include "load_image.hpp"

#include "util.hpp"

#include <png.h>

struct PngIo {
    size_t index;
    unsigned char *buffer;
    size_t size;
};

void read_png_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    PngIo *png_io = reinterpret_cast<PngIo*>(png_get_io_ptr(png_ptr));
    size_t new_index = png_io->index + length;
    if (new_index > png_io->size)
        panic("libpng trying to read beyond buffer");
    memcpy(data, png_io->buffer + png_io->index, length);
    png_io->index = new_index;
}

SDL_Texture * load_texture(SDL_Renderer * renderer, struct RuckSackTexture * rs_texture) {
    size_t size = rucksack_texture_size(rs_texture);
    unsigned char * image_buffer = allocate<unsigned char>(size);
    if (rucksack_texture_read(rs_texture, image_buffer) != RuckSackErrorNone)
        panic("read texture failed");

    if (png_sig_cmp(image_buffer, 0, 8))
        panic("not png file");

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
        panic("unable to create png read struct");

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr)
        panic("unable to create png info struct");

    // don't call any png_* functions outside of this function D:
    if (setjmp(png_jmpbuf(png_ptr)))
        panic("libpng has jumped the shark");

    png_set_sig_bytes(png_ptr, 8);

    PngIo png_io = {8, image_buffer, size};
    png_set_read_fn(png_ptr, &png_io, read_png_data);

    png_read_info(png_ptr, info_ptr);

    uint32_t spritesheet_width  =  png_get_image_width(png_ptr, info_ptr);
    uint32_t spritesheet_height = png_get_image_height(png_ptr, info_ptr);

    if (spritesheet_width <= 0 || spritesheet_height <= 0)
        panic("spritesheet image has no pixels");

    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STATIC, spritesheet_width, spritesheet_height);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    // bits per channel (not per pixel)
    uint32_t bits_per_channel = png_get_bit_depth(png_ptr, info_ptr);
    if (bits_per_channel != 8)
        panic("expected 8 bits per channel");

    uint32_t channel_count = png_get_channels(png_ptr, info_ptr);
    if (channel_count != 4)
        panic("expected 4 channels");

    uint32_t color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type != PNG_COLOR_TYPE_RGBA)
        panic("expected RGBA");

    uint32_t pitch = spritesheet_width * bits_per_channel * channel_count / 8;

    char *decoded_image = allocate<char>(spritesheet_width * pitch);

    png_bytep *row_ptrs = allocate<png_bytep>(spritesheet_height);

    for (size_t i = 0; i < spritesheet_height; i++) {
        png_uint_32 q = (spritesheet_height - i - 1) * pitch;
        row_ptrs[i] = (png_bytep)decoded_image + q;
    }

    png_read_image(png_ptr, row_ptrs);

    SDL_UpdateTexture(texture, NULL, decoded_image, pitch);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    destroy(row_ptrs, 0);
    destroy(decoded_image, 0);
    destroy(image_buffer, 0);

    return texture;
}

