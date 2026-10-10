#include "drawing.h"
#include "memory.h"
#include "math_utils.h"
#include "text_utils.h"
#include "font_data.h"

const u32 ALPHA_SOLID = 255;

Image create_image(int width, int height) {
    Image image;
    image.width = width;
    image.height = height;
    image.pixels = allocate_array<u32>((u64)width * height);
    return image;
}

void set_pixel(Image& target, int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= target.width || y >= target.height) {
        return;
    }
    target.pixels[y * target.width + x] = color;
}

void fill_rectangle(Image& target, int x, int y, int width, int height, u32 color) {
    // cut the rectangle to the part that is inside the image
    int left = max_int(x, 0);
    int top = max_int(y, 0);
    int right = min_int(x + width, target.width);
    int bottom = min_int(y + height, target.height);
    for (int row = top; row < bottom; row++) {
        u32* line = target.pixels + row * target.width;
        for (int column = left; column < right; column++) {
            line[column] = color;
        }
    }
}

// The four parts of a pixel 0xAARRGGBB
static u32 red_of(u32 pixel)   { return (pixel >> 16) & 0xFF; }
static u32 green_of(u32 pixel) { return (pixel >> 8) & 0xFF; }
static u32 blue_of(u32 pixel)  { return pixel & 0xFF; }

u32 blend_pixel(u32 below, u32 above) {
    u32 alpha = pixel_alpha(above);

    if (alpha == ALPHA_SOLID) return above & 0xFFFFFF;
    if (alpha == 0) return below;

    u32 show_through = ALPHA_SOLID - alpha;
    u32 red   = red_of(above)   + red_of(below)   * show_through / ALPHA_SOLID;
    u32 green = green_of(above) + green_of(below) * show_through / ALPHA_SOLID;
    u32 blue  = blue_of(above)  + blue_of(below)  * show_through / ALPHA_SOLID;
    return (min_int(red, 255) << 16) | (min_int(green, 255) << 8) | min_int(blue, 255);
}

void draw_character(Image& target, int x, int y, u8 character, u32 color, int size) {
    const u8* rows = FONT_8X16[character];
    //every font pixel becomes a block of size x size pixels
    for (int row = 0; row < CHAR_HEIGHT * size; row++) {
        u8 bits = rows[row / size];
        for (int column = 0; column < CHAR_WIDTH * size; column++) {
            // bit 7 (0x80) is the leftmost pixel bit 0 the rightmost
            u8 mask = 0x80 >> (column / size);
            if (bits & mask) {
                set_pixel(target, x + column, y + row, color);
            }
        }
    }
}

void draw_small_character(Image& target, int x, int y, u8 character, u32 color) {
    const u8* rows = FONT_8X8[character];
    for (int row = 0; row < SMALL_CHAR_HEIGHT; row++) {
        for (int column = 0; column < CHAR_WIDTH; column++) {
            if (rows[row] & (0x80 >> column)) {
                set_pixel(target, x + column, y + row, color);
            }
        }
    }
}

int draw_text(Image& target, int x, int y, const char* text, u32 color, int size) {
    for (int i = 0; text[i] != 0; i++) {
        draw_character(target, x, y, (u8)text[i], color, size);
        x += CHAR_WIDTH * size;
    }
    return x;
}

void draw_text_with_background(Image& target, int x, int y, const char* text, u32 color, u32 background) {
    fill_rectangle(target, x, y, text_length(text) * CHAR_WIDTH, CHAR_HEIGHT, background);
    draw_text(target, x, y, text, color);
}
