#include "boot_screen.h"
#include "ui.h"
#include "screen.h"
#include "memory.h"
#include "sound.h"
#include "characters.h"
#include "asset_files.h"
#include "bitmap.h"
#include "keyboard_mouse.h"
#include "timer.h"
#include "text_utils.h"
#include "debug_log.h"

const int LEFT = 2;     // the column where every line starts
const int PROGRESS_BAR_WIDTH = 50;
const int PRESS_ANY_KEY_ROW = 45;

static int current_row = 1;             //where the next line is written
static int files_loaded = 0;
static int files_to_load = 1;

//debug line
static void print_line(const char* text, u32 color) {
    write_text(LEFT, current_row, text, color);
    current_row++;
    show_canvas();
}

//Asset loading bar
static void draw_progress_bar(const char* file_name) {

    fill_rectangle(canvas, 0, row_y(current_row), CANVAS_WIDTH, 2 * CHAR_HEIGHT, COLOR_BLACK);

    int filled = PROGRESS_BAR_WIDTH * files_loaded / files_to_load;
    Text bar;
    bar.add_char('[');
    for (int i = 0; i < PROGRESS_BAR_WIDTH; i++) {
        bar.add_char(i < filled ? CHAR_FULL_BLOCK : CHAR_LIGHT_SHADE);
    }
    bar.add("] ").add_number(100 * files_loaded / files_to_load).add_char('%');
    write_text(LEFT, current_row, bar.text, COLOR_LIGHT_GRAY);
    write_text(LEFT, current_row + 1, file_name, COLOR_DARK_GRAY);
    show_canvas();
}

static void one_more_file_loaded(const char* file_name) {
    files_loaded++;
    draw_progress_bar(file_name);
    update_sound();
}

static void load_picture(const char* path, Bitmap* picture) {
    const AssetFile* file = find_asset_file(path);
    if (file == nullptr) {
        Text message;
        message.add("A file is missing: ").add(path);
        kernel_panic(message.text);
    }
    const char* error;
    if (!open_bitmap(file->data, file->size, PIECE_FILE_SIZE, picture, &error)) {
        Text message;
        message.add("Could not read ").add(path).add(": ").add(error);
        kernel_panic(message.text);
    }
}

static void load_piece_pictures(PiecePictures* pieces) {
    static const char* PIECE_NAMES[7] = { "", "pawn", "knight", "bishop", "rook", "queen", "king" };
    static const char* SIZE_LETTERS[PIECE_SIZE_COUNT] = { "_s_", "_m_", "_l_" };
    for (int color = 0; color < 2; color++) {
        for (int type = 1; type <= 6; type++) {
            for (int size = 0; size < PIECE_SIZE_COUNT; size++) {
                for (int rotation = 0; rotation < PIECE_ROTATIONS; rotation++) {
                    Text path;
                    path.add("/assets/pieces/").add(color == 0 ? "white_" : "black_").add(PIECE_NAMES[type]);
                    path.add(SIZE_LETTERS[size]).add_number(rotation).add(".bmp");
                    Bitmap& picture = pieces->picture[color][type][size][rotation];
                    load_picture(path.text, &picture);
                    pieces->visible[color][type][size][rotation] = find_visible_area(picture);
                    one_more_file_loaded(path.text + text_length("/assets/"));
                }
            }
        }
    }
}


void run_boot_screen(const BootInfo* boot_info, PiecePictures* pieces) {
    fill_rectangle(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BLACK);
    print_line("CHESSOS BIOS", COLOR_WHITE);
    print_line("code: github.com / O-Ayyad", COLOR_LIGHT_GRAY);
    current_row++;
    u64 free_at_start = free_memory_bytes();

    Text line;
    line.add("CPU ............ x86-64, 64-bit long mode, SSE2");
    print_line(line.text, COLOR_LIGHT_GRAY);
    line = Text();
    line.add("MEMORY ......... ").add_number(free_at_start / 1024).add("K free");
    print_line(line.text, COLOR_LIGHT_GRAY);
    line = Text();
    line.add("VIDEO .......... ").add_number(boot_info->screen_width).add_char('x').add_number(boot_info->screen_height);
    line.add_char('x').add_number(boot_info->bits_per_pixel);
    print_line(line.text, COLOR_LIGHT_GRAY);
    print_line("INPUT .......... PS/2 keyboard + mouse", COLOR_LIGHT_GRAY);

    setup_sound();
    line = Text();
    line.add("SOUND .......... ").add(sound_device_name());
    print_line(line.text, COLOR_LIGHT_GRAY);
    line = Text();
    line.add("MIDI ........... ").add_number(song_count()).add(song_count() == 1 ? " song" : " songs");
    print_line(line.text, COLOR_LIGHT_GRAY);
    current_row++;

    print_line("LOADING ASSETS", COLOR_WHITE);
    const int PIECE_PICTURE_COUNT = 2 * 6 * PIECE_SIZE_COUNT * PIECE_ROTATIONS;
    files_to_load = PIECE_PICTURE_COUNT + count_character_pictures();
    files_loaded = 0;
    load_piece_pictures(pieces);
    load_characters(one_more_file_loaded);
    files_loaded = files_to_load;
    draw_progress_bar("done");
    current_row += 3;

    if (character_count() == 0) kernel_panic("No characters found in assets/characters/");
    line = Text();
    line.add_number(character_count()).add(" characters ready.  Memory used: ");
    line.add_number((free_at_start - free_memory_bytes()) / 1024).add("K");
    print_line(line.text, COLOR_LIGHT_GRAY);
    debug_log(line.text);
    debug_log("\n");
}

void wait_for_any_key(int max_milliseconds) {
    write_text(LEFT, PRESS_ANY_KEY_ROW, "PRESS ANY KEY TO CONTINUE", COLOR_WHITE);
    show_everything();
    u64 give_up_at = milliseconds_since_start() + max_milliseconds;
    while (milliseconds_since_start() < give_up_at) {
        keep_things_running();
        InputEvent event;
        bool pressed = false;
        while (next_input_event(&event)) {
            if (event.type == EVENT_KEY_PRESS || event.type == EVENT_MOUSE_PRESS) pressed = true;
        }
        if (pressed) return;
    }
}
