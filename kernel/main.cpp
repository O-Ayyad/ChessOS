#include "boot_info.h"
#include "cpu.h"
#include "memory.h"
#include "screen.h"
#include "timer.h"
#include "debug_log.h"
#include "asset_files.h"
#include "keyboard_mouse.h"
#include "sound.h"
#include "chess_rules.h"
#include "chess_ai.h"
#include "board_view.h"
#include "boot_screen.h"
#include "menu.h"
#include "game.h"
#include "ui.h"
#include "math_utils.h"

static PiecePictures piece_pictures;

static void handle_input() {
    InputEvent event;
    while (next_input_event(&event)) {
        if (event.type == EVENT_KEY_PRESS) {
            if (current_screen == SCREEN_MENU) menu_handle_key(event);
            else game_handle_key(event);
        } else if (current_screen == SCREEN_MENU) {
            if (event.type == EVENT_MOUSE_PRESS && event.button == MOUSE_LEFT_BUTTON) {
                menu_handle_click(mouse_x() / CHAR_WIDTH, mouse_y() / CHAR_HEIGHT);
            }
        } else {
            game_handle_mouse(event);
        }
    }
}

static void main_loop() {
    u64 last_blink = 0;
    while (true) {
        check_keyboard_and_mouse();
        handle_input();
        update_sound();

        bool pointer_moved = mouse_has_moved();
        if (pointer_moved && current_screen == SCREEN_GAME) game_mouse_moved();

        // blinking cursors and the laughing animation need a redraw now and then
        u64 blink = milliseconds_since_start() / BLINK_MILLISECONDS;
        if (blink != last_blink) {
            last_blink = blink;
            screen_needs_redraw = true;
        }

        if (current_screen == SCREEN_GAME) game_update();

        if (board_needs_redraw && current_screen == SCREEN_GAME) screen_needs_redraw = true;
        if (screen_needs_redraw) {
            if (current_screen == SCREEN_MENU) draw_menu();
            else draw_game_screen();
            show_everything();
            screen_needs_redraw = false;
        } else if (pointer_moved) {
            // only the pointer moved
            erase_mouse_pointer();
            draw_mouse_pointer(mouse_x(), mouse_y());
        }
        cpu_relax();
    }
}

extern "C" __attribute__((section(".text.start"))) void kernel_start(BootInfo* boot_info) {
    clear_global_variables();
    setup_error_handlers();

    //we were not started by our own loader
    if (boot_info->magic != BOOT_INFO_MAGIC) kernel_panic();

    setup_memory(boot_info);
    setup_screen(boot_info);
    setup_timer();
    debug_log("ChessOS starting\n");
    seed_random(read_cycle_counter());
    setup_asset_files((const u8*)(u64)boot_info->assets_address, boot_info->assets_size);
    setup_keyboard_and_mouse();
    setup_chess_rules();

    //music and mouse while the computer thinks
    ai_set_background_work(keep_things_running);

    run_boot_screen(boot_info, &piece_pictures);
    setup_board_view(&piece_pictures);
    setup_menu();
    wait_for_any_key(7500);

    current_screen = SCREEN_MENU;
    screen_needs_redraw = true;
    main_loop();
}
