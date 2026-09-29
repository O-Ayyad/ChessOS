// ui.h - things shared by all the screens (boot screen, menu, game).
//
// The screen is laid out as a grid of text: 128 columns x 48 rows. Most
// functions here take a column and a row instead of pixels.
#pragma once
#include "types.h"
#include "drawing.h"
#include "bitmap.h"

enum Screen { SCREEN_MENU, SCREEN_GAME };
extern Screen current_screen;

// Set these to true when something changed; the main loop then draws again.
extern bool screen_needs_redraw;     // redraw the text, portraits and panels
extern bool board_needs_redraw;      // also redraw the 3D board (slower)

// Called during long jobs (the computer thinking, drawing the board) so the
// music keeps playing and the mouse pointer keeps moving.
void keep_things_running();

// Put the finished canvas on the screen, with the mouse pointer on top.
void show_everything();

const int BLINK_MILLISECONDS = 260;         // text cursor blinking and the laughing animation

void write_text(int column, int row, const char* text, u32 color);
void draw_dashes(int column, int row, int width, u32 color);
// Writes text over several rows, breaking lines between words. Returns the number of rows used.
int  write_wrapped_text(int column, int row, int width, int max_rows, const char* text, u32 color);
void draw_box(int column, int row, int width, int height, u32 color);
void draw_portrait(const Bitmap& picture, int column, int row);
