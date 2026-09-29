#include "game.h"
#include "menu.h"
#include "ui.h"
#include "board_view.h"
#include "sound.h"
#include "math_utils.h"
#include "text_utils.h"
#include "drawing.h"

static void go_to_menu() {
    current_screen = SCREEN_MENU;
    screen_needs_redraw = true;
}

static int promotion_from_letter(char letter) {
    letter = to_lower_case(letter);
    if (letter == 'q') return QUEEN;
    if (letter == 'r') return ROOK;
    if (letter == 'b') return BISHOP;
    if (letter == 'n') return KNIGHT;
    return 0;
}

static bool read_square_to_square(const char* text, int* from, int* to, int* promotion) {
    char squeezed[32];
    int length = 0;
    for (int i = 0; text[i] != 0 && length < 31; i++) {
        char c = text[i];
        if (c != ' ' && c != '-' && c != '=' && c != 'x') squeezed[length++] = c;
    }
    squeezed[length] = 0;
    if (length < 4 || length > 5) return false;
    *from = text_to_square(squeezed);
    *to = text_to_square(squeezed + 2);
    if (*from == NO_SQUARE || *to == NO_SQUARE) return false;
    *promotion = length == 5 ? promotion_from_letter(squeezed[4]) : 0;
    return length == 4 || *promotion != 0;
}

static bool try_notation_move(const char* text) {
    char wanted[16];
    int length = 0;
    for (int i = 0; text[i] != 0 && length < 15; i++) {
        char c = text[i];
        if (c == '+' || c == '#' || c == '!' || c == '?' || c == ' ' || c == '=') continue;
        wanted[length++] = c == '0' ? 'O' : c; 
    }
    wanted[length] = 0;
    // allow "e8q" as well as "e8Q"
    if (length >= 3 && is_digit(wanted[length - 2])) wanted[length - 1] = to_upper_case(wanted[length - 1]);
    if (length == 0 || !is_humans_turn()) return false;

    for (int i = 0; i < game.legal_move_count; i++) {
        char notation[10];
        move_to_notation(game.board, game.legal_moves[i], notation);
        char plain[12];
        int plain_length = 0;
        for (int j = 0; notation[j] != 0; j++) {
            if (notation[j] != '+' && notation[j] != '#' && notation[j] != '=') plain[plain_length++] = notation[j];
        }
        plain[plain_length] = 0;
        if (texts_equal(plain, wanted)) {
            return try_human_move(game.legal_moves[i].from, game.legal_moves[i].to, game.legal_moves[i].promotion);
        }
    }
    return false;
}

static const char* skip_spaces(const char* text) {
    while (*text == ' ') text++;
    return text;
}

static bool is_command(const char* text, const char* word, const char** rest) {
    int length = text_length(word);
    for (int i = 0; i < length; i++) {
        if (to_lower_case(text[i]) != word[i]) return false;
    }
    if (text[length] != 0 && text[length] != ' ') return false;
    *rest = skip_spaces(text + length);
    return true;
}

static void count_and_report_selection(const char* square_text) {
    int count = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {
        if (game.target_squares & (1ULL << square)) count++;
    }
    Text message;
    message.add("Selected ").add(square_text).add(": ").add_number(count).add(count == 1 ? " legal move" : " legal moves");
    show_message(message.text, COLOR_LIGHT_GRAY);
}

//Runs one command. Returns false if it failed
static bool run_command(const char* command) {
    command = skip_spaces(command);
    if (*command == 0) return true;
    if (*command == '/') command++;
    const char* rest;

    if (is_command(command, "help", &rest) || is_command(command, "?", &rest)) {
        show_message("e2e4  Nf3  /select e2  /move e4 /move e8{q,r,b,n} to under promote /resign  /offerdraw  /accept  /decline  /undo  /new  /menu  /view N  /music  /next", COLOR_LIGHT_GRAY);
        return true;
    }
    if (is_command(command, "new", &rest))  { start_new_game(); return true; }
    if (is_command(command, "menu", &rest)) { go_to_menu(); return true; }
    if (is_command(command, "undo", &rest)) { take_back_move(); return true; }
    if (is_command(command, "music", &rest)) {
        set_music_playing(!is_music_playing());
        show_message(is_music_playing() ? "Music on." : "Music off.", COLOR_LIGHT_GRAY);
        return true;
    }
    if (is_command(command, "next", &rest)) {
        start_next_song();
        Text message;
        message.add("Now playing: ").add(current_song_name());
        show_message(message.text, COLOR_LIGHT_GRAY);
        return true;
    }
    if (is_command(command, "view", &rest) || is_command(command, "flip", &rest)) {
        bool flip = to_lower_case(command[0]) == 'f';
        int view = current_board_view() + (flip ? VIEW_COUNT / 2 : 1);
        if (*rest >= '1' && *rest <= '8') view = *rest - '1';
        set_board_view(view);
        board_needs_redraw = true;
        play_sound_effect(SOUND_CHANGE_VIEW);
        return true;
    }

    // everything below needs a game in progress
    if (game.result != STILL_PLAYING) {
        show_message("The game is over. Press B/M to replay it, or type /new.", COLOR_LIGHT_RED);
        return false;
    }
    if (is_command(command, "resign", &rest)) {
        resign_game();
        return true;
    }
    if (is_command(command, "offerdraw", &rest) || is_command(command, "draw", &rest)) {
        offer_draw();
        return true;
    }
    if (is_command(command, "accept", &rest)) {
        if (game.draw_offered_by < 0) {
            show_message("There is no draw offer to accept.", COLOR_LIGHT_RED);
            return false;
        }
        game.result = DRAW_AGREED;
        game.draw_offered_by = -1;
        game.evaluation = 0;
        clear_selection();
        show_message("Draw agreed.", COLOR_LIGHT_GRAY);
        play_sound_effect(SOUND_GAME_OVER);
        return true;
    }
    if (is_command(command, "decline", &rest)) {
        if (game.draw_offered_by < 0) {
            show_message("There is no draw offer.", COLOR_LIGHT_RED);
            return false;
        }
        game.draw_offered_by = -1;
        show_message("Draw offer declined.", COLOR_LIGHT_GRAY);
        return true;
    }
    if (is_command(command, "select", &rest) || is_command(command, "sel", &rest)) {
        int square = text_to_square(rest);
        if (square == NO_SQUARE) {
            show_message("Usage: /select e2", COLOR_LIGHT_RED);
            return false;
        }
        int piece = game.board.squares[square];
        if (!is_humans_turn() || piece == EMPTY || piece_color(piece) != game.board.side_to_move) {
            Text message;
            message.add("You have no piece on ").add(rest);
            show_message(message.text, COLOR_LIGHT_RED);
            play_sound_effect(SOUND_ILLEGAL);
            return false;
        }
        select_square(square);
        play_sound_effect(SOUND_SELECT);
        count_and_report_selection(rest);
        return true;
    }
    if (is_command(command, "move", &rest) || is_command(command, "mv", &rest)) {
        int from, to, promotion;
        if (read_square_to_square(rest, &from, &to, &promotion)) return try_human_move(from, to, promotion);
        int square = text_to_square(rest);
        if (square != NO_SQUARE) {
            if (game.selected_square < 0) {
                show_message("Select a piece first:  /select e2 && /move e4", COLOR_LIGHT_RED);
                return false;
            }
            const char* after = skip_spaces(rest + 2);
            if (*after == '=') after++;
            return try_human_move(game.selected_square, square, promotion_from_letter(*after));
        }
        if (try_notation_move(rest)) return true;
        show_message("Usage: /move e4 (after /select), /move e2 e4 or /move Nf3", COLOR_LIGHT_RED);
        return false;
    }

    // no command word: maybe just a move
    int from, to, promotion;
    if (read_square_to_square(command, &from, &to, &promotion)) return try_human_move(from, to, promotion);
    if (try_notation_move(command)) return true;
    Text message;
    message.add("Unknown command: ").add(command).add("  (type /help)");
    show_message(message.text, COLOR_LIGHT_RED);
    play_sound_effect(SOUND_ILLEGAL);
    return false;
}

void run_command_line(const char* line) {
    // split at "&&" or ";" and run the pieces one by one
    char piece[80];
    int length = 0;
    for (int i = 0; ; i++) {
        char c = line[i];
        bool end_of_piece = c == 0 || c == ';' || (c == '&' && line[i + 1] == '&');
        if (end_of_piece) {
            piece[length] = 0;
            if (!run_command(piece)) return;
            length = 0;
            if (c == 0) return;
            if (c == '&') i++;          // skip the second &
            continue;
        }
        if (length < 79) piece[length++] = c;
    }
}

// keys
static void rebuild_replay_board() {
    set_starting_position(game.replay_board);
    for (int i = 0; i < game.replay_position; i++) {
        UndoInfo undo;
        make_move(game.replay_board, game.moves[i].move, undo);
    }
}

static void step_replay(int direction) {
    if (game.replay_position < 0) game.replay_position = game.move_count;
    game.replay_position = clamp_int(game.replay_position + direction, 0, game.move_count);
    rebuild_replay_board();
    play_sound_effect(SOUND_MOVE);
    board_needs_redraw = true;
    screen_needs_redraw = true;
}

static void change_view(int direction) {
    set_board_view(current_board_view() + direction);
    board_needs_redraw = true;
    play_sound_effect(SOUND_CHANGE_VIEW);
}

void game_handle_key(const InputEvent& event) {
    int typed_length = text_length(game.typed_command);
    const int MAX_TYPED = 64;

    if (event.key == KEY_LEFT)  change_view(-1);
    if (event.key == KEY_RIGHT) change_view(1);
    if (event.key == KEY_UP) {                                 // bring back the last command
        copy_text(game.typed_command, game.last_command, sizeof(game.typed_command));
        screen_needs_redraw = true;
    }
    if (event.key == KEY_DOWN) {
        game.typed_command[0] = 0;
        screen_needs_redraw = true;
    }
    if (event.key == KEY_ESCAPE) {
        game.typed_command[0] = 0;
        clear_selection();
    }
    if (event.key == KEY_BACKSPACE && typed_length > 0) {
        game.typed_command[typed_length - 1] = 0;
        screen_needs_redraw = true;
    }
    if (event.key == KEY_F2) {
        set_music_playing(!is_music_playing());
        show_message(is_music_playing() ? "Music on." : "Music off.", COLOR_LIGHT_GRAY);
    }
    if (event.key == KEY_F3) {
        start_next_song();
        Text message;
        message.add("Now playing: ").add(current_song_name());
        show_message(message.text, COLOR_LIGHT_GRAY);
    }
    if (event.key == KEY_F5) start_new_game();
    if (event.key == KEY_F10) go_to_menu();
    if (event.key == KEY_ENTER && typed_length > 0) {
        copy_text(game.last_command, game.typed_command, sizeof(game.last_command));
        char line[72];
        copy_text(line, game.typed_command, sizeof(line));
        game.typed_command[0] = 0;
        run_command_line(line);
        screen_needs_redraw = true;
    }
    if (event.key == KEY_CHARACTER) {
        // after the game, B and M step backwards and forwards through it
        char letter = to_lower_case(event.character);
        if (game.result != STILL_PLAYING && typed_length == 0 && (letter == 'b' || letter == 'm')) {
            step_replay(letter == 'b' ? -1 : 1);
            return;
        }
        if (typed_length < MAX_TYPED) {
            game.typed_command[typed_length] = event.character;
            game.typed_command[typed_length + 1] = 0;
            screen_needs_redraw = true;
        }
    }
}


static int square_under_mouse() {
    int x = mouse_x() - BOARD_VIEW_X;
    int y = mouse_y() - BOARD_VIEW_Y;
    if (x < 0 || y < 0 || x >= BOARD_VIEW_WIDTH || y >= BOARD_VIEW_HEIGHT) return -1;
    return square_at_view_position(x, y);
}

void game_mouse_moved() {
    int square = square_under_mouse();
    if (square != game.hovered_square) {
        game.hovered_square = square;
        board_needs_redraw = true;
    }
}

void game_handle_mouse(const InputEvent& event) {
    if (event.button == MOUSE_RIGHT_BUTTON && event.type == EVENT_MOUSE_PRESS) {
        clear_selection();
        return;
    }
    if (event.button != MOUSE_LEFT_BUTTON) return;
    int square = square_under_mouse();

    if (event.type == EVENT_MOUSE_PRESS) {
        if (square < 0 || !is_humans_turn() || game.animating || game.replay_position >= 0) return;
        // clicking a marked square moves the selected piece there
        if (game.selected_square >= 0 && (game.target_squares & (1ULL << square))) {
            try_human_move(game.selected_square, square, 0);
            game.drag_from_square = -1;
            return;
        }
        int piece = game.board.squares[square];
        if (piece != EMPTY && piece_color(piece) == game.board.side_to_move) {
            select_square(square);
            game.drag_from_square = square;      // the piece may also be dragged
            play_sound_effect(SOUND_SELECT);
            char name[3];
            square_to_text(square, name);
            Text message;
            message.add("Selected ").add(name).add(" - click a marked square.");
            show_message(message.text, COLOR_LIGHT_GRAY);
        } else if (piece != EMPTY) {
            show_message("That's not your piece.", COLOR_LIGHT_RED);
            play_sound_effect(SOUND_ILLEGAL);
            clear_selection();
        } else {
            clear_selection();
        }
    }
    if (event.type == EVENT_MOUSE_RELEASE) {
        // let go over a marked square after dragging: move there
        int from = game.drag_from_square;
        if (from >= 0 && square >= 0 && square != from && game.selected_square == from && (game.target_squares & (1ULL << square))) {
            try_human_move(from, square, 0);
        }
        game.drag_from_square = -1;
    }
}
