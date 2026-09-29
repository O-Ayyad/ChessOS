#pragma once
#include "types.h"
#include "drawing.h"

//Two strict picture sizes:
const int PORTRAIT_FILE_SIZE = 64;  //portraits are 64 x 64 files...
const int PORTRAIT_SCALE = 3;       //...drawn 3 times bigger (192 x 192 on screen)
const int PIECE_FILE_SIZE = 192;    //chess pieces are 192 x 192 files, drawn as they are
const u32 SEE_THROUGH_COLOR = 0xFF00FF;    // magenta 255 0 255

struct Bitmap {
    const u8* pixel_rows;
    const u8* palette;
    int bits_per_pixel;
    int bytes_per_row;
    bool stored_top_down;
    int size;                  // width and height in pixels
};


// wanted_size is PORTRAIT_FILE_SIZE or PIECE_FILE_SIZE: any other size is an error.
bool open_bitmap(const u8* file, u64 file_size, int wanted_size, Bitmap* bitmap, const char** error);
u32 bitmap_pixel(const Bitmap& bitmap, int x, int y);
// Draws the picture "scale" times bigger, with its top-left corner at (x, y).
void draw_bitmap(Image& target, const Bitmap& bitmap, int x, int y, int scale);
// Paints one file pixel as a scale x scale block (skipped if see-through).
void draw_scaled_pixel(Image& target, int x, int y, u32 pixel, int scale);

struct VisibleArea {
    int left, top, right, bottom;   // in file pixels
};
VisibleArea find_visible_area(const Bitmap& bitmap);
