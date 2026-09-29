// screen.cpp - see screen.h
#include "screen.h"
#include "memory.h"
#include "math_utils.h"
#include "text_utils.h"
#include "debug_log.h"
#include "cpu.h"

Image canvas;


static u8* framebuffer = nullptr;
static int screen_width = 0;
static int screen_height = 0;
static int bytes_per_line = 0;
static int bytes_per_pixel = 0;
static int red_shift = 16, green_shift = 8, blue_shift = 0;

static bool screen_matches_canvas = false;
static float scale = 1;
static int offset_x = 0, offset_y = 0;
static int* canvas_column_for_screen_x = nullptr;
static int* canvas_row_for_screen_y = nullptr;

void setup_screen(const BootInfo* boot_info) {
    framebuffer = (u8*)boot_info->framebuffer_address;
    screen_width = boot_info->screen_width;
    screen_height = boot_info->screen_height;
    bytes_per_line = boot_info->bytes_per_line;
    bytes_per_pixel = boot_info->bits_per_pixel / 8;
    red_shift = boot_info->red_shift;
    green_shift = boot_info->green_shift;
    blue_shift = boot_info->blue_shift;

    canvas = create_image(CANVAS_WIDTH, CANVAS_HEIGHT);

    screen_matches_canvas = screen_width == CANVAS_WIDTH && screen_height == CANVAS_HEIGHT &&
                            bytes_per_pixel == 4 && red_shift == 16 && green_shift == 8 && blue_shift == 0;

    // Work out the stretching: as big as fits, centred, with black bars if needed.
    scale = min_float((float)screen_width / CANVAS_WIDTH, (float)screen_height / CANVAS_HEIGHT);
    int shown_width = (int)(CANVAS_WIDTH * scale);
    int shown_height = (int)(CANVAS_HEIGHT * scale);
    offset_x = (screen_width - shown_width) / 2;
    offset_y = (screen_height - shown_height) / 2;
    canvas_column_for_screen_x = allocate_array<int>(screen_width);
    canvas_row_for_screen_y = allocate_array<int>(screen_height);
    for (int x = 0; x < screen_width; x++) {
        int column = (int)((x - offset_x + 0.5f) / scale);
        bool outside = x < offset_x || x >= offset_x + shown_width || column >= CANVAS_WIDTH;
        canvas_column_for_screen_x[x] = outside ? -1 : column;
    }
    for (int y = 0; y < screen_height; y++) {
        int row = (int)((y - offset_y + 0.5f) / scale);
        bool outside = y < offset_y || y >= offset_y + shown_height || row >= CANVAS_HEIGHT;
        canvas_row_for_screen_y[y] = outside ? -1 : row;
    }
}

static u32 to_screen_color(u32 color) {
    u32 red = (color >> 16) & 0xFF;
    u32 green = (color >> 8) & 0xFF;
    u32 blue = color & 0xFF;
    return (red << red_shift) | (green << green_shift) | (blue << blue_shift);
}

static void write_screen_pixel(int x, int y, u32 color) {
    u8* pixel = framebuffer + (u64)y * bytes_per_line + x * bytes_per_pixel;
    u32 value = to_screen_color(color);
    if (bytes_per_pixel == 4) {
        *(u32*)pixel = value;
    } else { 
        pixel[0] = value & 0xFF;
        pixel[1] = (value >> 8) & 0xFF;
        pixel[2] = (value >> 16) & 0xFF;
    }
}

static void copy_to_screen(int x, int y, int width, int height) {
    int left = max_int(x, 0);
    int top = max_int(y, 0);
    int right = min_int(x + width, screen_width);
    int bottom = min_int(y + height, screen_height);
    if (left >= right || top >= bottom) return;

    for (int screen_y = top; screen_y < bottom; screen_y++) {
        if (screen_matches_canvas) {
            // the easy, fast case: one row of the canvas is one row of the screen
            u8* destination = framebuffer + (u64)screen_y * bytes_per_line + left * 4;
            memcpy(destination, canvas.pixels + screen_y * CANVAS_WIDTH + left, (right - left) * 4);
            continue;
        }
        int canvas_y = canvas_row_for_screen_y[screen_y];
        for (int screen_x = left; screen_x < right; screen_x++) {
            int canvas_x = canvas_column_for_screen_x[screen_x];
            u32 color = COLOR_BLACK;
            if (canvas_x >= 0 && canvas_y >= 0) {
                color = canvas.pixels[canvas_y * CANVAS_WIDTH + canvas_x];
            }
            write_screen_pixel(screen_x, screen_y, color);
        }
    }
}

void show_canvas() {
    copy_to_screen(0, 0, screen_width, screen_height);
}

const int POINTER_WIDTH = 11;
const int POINTER_HEIGHT = 19;
static const char* POINTER_SHAPE[POINTER_HEIGHT] = {
    "X          ",
    "XX         ",
    "X.X        ",
    "X..X       ",
    "X...X      ",
    "X....X     ",
    "X.....X    ",
    "X......X   ",
    "X.......X  ",
    "X........X ",
    "X.........X",
    "X......XXXX",
    "X...X..X   ",
    "X..XX..X   ",
    "X.X  X..X  ",
    "XX   X..X  ",
    "X     X..X ",
    "      X..X ",
    "       XX  ",
};
static int pointer_screen_x = -100;
static int pointer_screen_y = -100;
static int pointer_canvas_x = -1;
static int pointer_canvas_y = -1;

int drawn_pointer_x() { return pointer_canvas_x; }
int drawn_pointer_y() { return pointer_canvas_y; }

void draw_mouse_pointer(int canvas_x, int canvas_y) {
    pointer_canvas_x = canvas_x;
    pointer_canvas_y = canvas_y;
    pointer_screen_x = offset_x + (int)(canvas_x * scale);
    pointer_screen_y = offset_y + (int)(canvas_y * scale);
    for (int row = 0; row < POINTER_HEIGHT; row++) {
        for (int column = 0; column < POINTER_WIDTH; column++) {
            char c = POINTER_SHAPE[row][column];
            if (c == ' ') continue;
            int x = pointer_screen_x + column;
            int y = pointer_screen_y + row;
            if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) continue;
            write_screen_pixel(x, y, c == 'X' ? COLOR_BLACK : COLOR_WHITE);
        }
    }
}

void erase_mouse_pointer() {
    copy_to_screen(pointer_screen_x, pointer_screen_y, POINTER_WIDTH + 1, POINTER_HEIGHT + 1);
}


void fatal_error(const char* message) {
    debug_log("FATAL ERROR: ");
    debug_log(message);
    debug_log("\n");
    if (canvas.pixels != nullptr && framebuffer != nullptr) {
        fill_rectangle(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_RED);
        draw_text(canvas, column_x(4), row_y(4), "CHESSOS HAS STOPPED", COLOR_WHITE, 2);
        draw_text(canvas, column_x(4), row_y(8), message, COLOR_WHITE);
        draw_text(canvas, column_x(4), row_y(10), "Please restart the computer.", COLOR_LIGHT_GRAY);
        show_canvas();
    }
    stop_forever();
}
