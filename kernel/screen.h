// screen.h - getting our drawing onto the real screen.
//
// We draw everything into "canvas", a 1024 x 768 picture in normal memory.
// When a picture is finished, show_canvas() copies it to the screen's memory
// (the "framebuffer"). Drawing in normal memory first is faster and stops the
// screen from flickering while we draw.
#pragma once
#include "types.h"
#include "drawing.h"
#include "boot_info.h"

const int CANVAS_WIDTH = 1024;
const int CANVAS_HEIGHT = 768;

extern Image canvas;

void setup_screen(const BootInfo* boot_info);
void show_canvas();

// The mouse pointer is drawn straight onto the screen, on top of the canvas.
void draw_mouse_pointer(int canvas_x, int canvas_y);
void erase_mouse_pointer();
// Where the pointer was last drawn (in canvas pixels)
int drawn_pointer_x();
int drawn_pointer_y();

// Shows a red error screen and stops the computer.
[[noreturn]] void fatal_error(const char* message);
