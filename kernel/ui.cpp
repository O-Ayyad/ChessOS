#include "ui.h"
#include "screen.h"
#include "sound.h"
#include "keyboard_mouse.h"
#include "math_utils.h"
#include "text_utils.h"

Screen current_screen = SCREEN_MENU;
bool screen_needs_redraw = true;
bool board_needs_redraw = true;

void keep_things_running() {
    update_sound();
    check_keyboard_and_mouse();
    if (mouse_x() != drawn_pointer_x() || mouse_y() != drawn_pointer_y()) {
        erase_mouse_pointer();
        draw_mouse_pointer(mouse_x(), mouse_y());
    }
}

void show_everything() {
    show_canvas();
    draw_mouse_pointer(mouse_x(), mouse_y());
}

void write_text(int column, int row, const char* text, u32 color) {
    draw_text(canvas, column_x(column), row_y(row), text, color);
}

void draw_dashes(int column, int row, int width, u32 color) {
    for (int i = 0; i < width; i++) {
        draw_character(canvas, column_x(column + i), row_y(row), CHAR_LINE_HORIZONTAL, color);
    }
}

int write_wrapped_text(int column, int row, int width, int max_rows, const char* text, u32 color) {
    int rows_used = 0;
    while (*text != 0 && rows_used < max_rows) {
        // how much fits on this row? break at the last space if the text goes on
        int length = 0;
        int last_space = -1;
        while (text[length] != 0 && length < width) {
            if (text[length] == ' ') last_space = length;
            length++;
        }
        if (text[length] != 0 && last_space > 0) length = last_space;

        char line[TEXT_COLUMNS + 1];
        memcpy(line, text, length);
        line[length] = 0;
        write_text(column, row + rows_used, line, color);
        rows_used++;

        text += length;
        while (*text == ' ') text++;
    }
    return rows_used;
}

void draw_box(int column, int row, int width, int height, u32 color) {
    fill_rectangle(canvas, column_x(column), row_y(row), column_x(width), row_y(height), COLOR_BLACK);
    int right = column + width - 1;
    int bottom = row + height - 1;
    draw_character(canvas, column_x(column), row_y(row), CHAR_CORNER_TOP_LEFT, color);
    draw_character(canvas, column_x(right), row_y(row), CHAR_CORNER_TOP_RIGHT, color);
    draw_character(canvas, column_x(column), row_y(bottom), CHAR_CORNER_BOTTOM_LEFT, color);
    draw_character(canvas, column_x(right), row_y(bottom), CHAR_CORNER_BOTTOM_RIGHT, color);
    for (int x = column + 1; x < right; x++) {
        draw_character(canvas, column_x(x), row_y(row), CHAR_LINE_HORIZONTAL, color);
        draw_character(canvas, column_x(x), row_y(bottom), CHAR_LINE_HORIZONTAL, color);
    }
    for (int y = row + 1; y < bottom; y++) {
        draw_character(canvas, column_x(column), row_y(y), CHAR_LINE_VERTICAL, color);
        draw_character(canvas, column_x(right), row_y(y), CHAR_LINE_VERTICAL, color);
    }
}

void draw_portrait(const Bitmap& picture, int column, int row) {
    draw_bitmap(canvas, picture, column_x(column), row_y(row), PORTRAIT_SCALE);   // 64 x 64 drawn 3x = 192 x 192 = 24 x 12 text cells
}
