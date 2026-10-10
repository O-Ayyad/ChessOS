#pragma once
#include "types.h"
#include "drawing.h"
#include "bitmap.h"

enum Screen { SCREEN_MENU, SCREEN_GAME };
extern Screen current_screen;
extern bool screen_needs_redraw;
extern bool board_needs_redraw;
void keep_things_running();
void show_everything();

const int BLINK_MILLISECONDS = 260;

void write_text(int column, int row, const char* text, u32 color);
void draw_dashes(int column, int row, int width, u32 color);
int  write_wrapped_text(int column, int row, int width, int max_rows, const char* text, u32 color);
void draw_box(int column, int row, int width, int height, u32 color);
void draw_portrait(const Bitmap& picture, int column, int row);
