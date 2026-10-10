#pragma once
#include "types.h"

struct Image {
    int width;
    int height;
    u32* pixels;    //width * height pixels
};

const u32 COLOR_BLACK         = 0x000000;
const u32 COLOR_BLUE          = 0x0000AA;
const u32 COLOR_GREEN         = 0x00AA00;
const u32 COLOR_CYAN          = 0x00AAAA;
const u32 COLOR_RED           = 0xAA0000;
const u32 COLOR_MAGENTA       = 0xAA00AA;
const u32 COLOR_BROWN         = 0xAA5500;
const u32 COLOR_LIGHT_GRAY    = 0xAAAAAA;
const u32 COLOR_DARK_GRAY     = 0x555555;
const u32 COLOR_LIGHT_BLUE    = 0x5555FF;
const u32 COLOR_LIGHT_GREEN   = 0x55FF55;
const u32 COLOR_LIGHT_CYAN    = 0x55FFFF;
const u32 COLOR_LIGHT_RED     = 0xFF5555;
const u32 COLOR_LIGHT_MAGENTA = 0xFF55FF;
const u32 COLOR_YELLOW        = 0xFFFF55;
const u32 COLOR_WHITE         = 0xFFFFFF;
const u32 COLOR_DIM_GRAY      = 0x3A3A3A;

const int CHAR_WIDTH = 8;
const int CHAR_HEIGHT = 16;
const int SMALL_CHAR_HEIGHT = 8;
const int TEXT_COLUMNS = 128;
const int TEXT_ROWS = 48;
static inline int column_x(int column) { return column * CHAR_WIDTH; }
static inline int row_y(int row)       { return row * CHAR_HEIGHT; }


const u8 CHAR_FULL_BLOCK       = 0xDB;
const u8 CHAR_LIGHT_SHADE      = 0xB0;
const u8 CHAR_LINE_HORIZONTAL  = 0xC4;
const u8 CHAR_LINE_VERTICAL    = 0xB3;
const u8 CHAR_CORNER_TOP_LEFT  = 0xDA;
const u8 CHAR_CORNER_TOP_RIGHT = 0xBF;
const u8 CHAR_CORNER_BOTTOM_LEFT  = 0xC0;
const u8 CHAR_CORNER_BOTTOM_RIGHT = 0xD9;
const u8 CHAR_ARROW_UP         = 0x18;
const u8 CHAR_ARROW_DOWN       = 0x19;
const u8 CHAR_ARROW_RIGHT      = 0x1A;
const u8 CHAR_ARROW_LEFT       = 0x1B;

Image create_image(int width, int height);

// A pixel with see-through information is 0xAARRGGBB: alpha 0 = see-through, 255 = solid
static inline u32 pixel_alpha(u32 pixel) { return (pixel >> 24) & 0xFF; }
static inline u32 pixel_color(u32 pixel) { return pixel & 0xFFFFFF; }

void fill_rectangle(Image& target, int x, int y, int width, int height, u32 color);
void set_pixel(Image& target, int x, int y, u32 color);

// size 2 draws the characters twice as big, and so on
void draw_character(Image& target, int x, int y, u8 character, u32 color, int size = 1);
void draw_small_character(Image& target, int x, int y, u8 character, u32 color);    //8 x 8 pixels
//returns the x after the text
int  draw_text(Image& target, int x, int y, const char* text, u32 color, int size = 1);
void draw_text_with_background(Image& target, int x, int y, const char* text, u32 color, u32 background);

// Mixes the pixel "above" over the pixel "below", by how solid "above" is.
// (Nothing uses this at the moment: our pictures are either solid or see-through.)
u32 blend_pixel(u32 below, u32 above);
