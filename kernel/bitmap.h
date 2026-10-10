#pragma once
#include "types.h"
#include "drawing.h"

const int PORTRAIT_FILE_SIZE = 64;      // portraits are 64x64 
const int PORTRAIT_SCALE = 3;
// chess pieces are 19 x192
const int PIECE_FILE_SIZE = 192;
//magenta
const u32 SEE_THROUGH_COLOR = 0xFF00FF;

struct Bitmap {
    const u8* pixel_rows;
    const u8* palette;
    int bits_per_pixel;
    int bytes_per_row;
    bool stored_top_down;
    int size; 
};

bool open_bitmap(const u8* file, u64 file_size, int wanted_size, Bitmap* bitmap, const char** error);
u32 bitmap_pixel(const Bitmap& bitmap, int x, int y);
void draw_bitmap(Image& target, const Bitmap& bitmap, int x, int y, int scale);
void draw_scaled_pixel(Image& target, int x, int y, u32 pixel, int scale);
struct VisibleArea { int left, top, right, bottom; };
VisibleArea find_visible_area(const Bitmap& bitmap);
