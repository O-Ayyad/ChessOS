#include "game.h"
#include "ui.h"
#include "board_view.h"
#include "screen.h"
#include "sound.h"
#include "timer.h"
#include "math_utils.h"
#include "text_utils.h"
#include "menu.h"

const int PANEL = 86; 
const int PANEL_WIDTH = 41;
const int MOVE_LIST_FIRST_ROW = 7;
const int MOVE_LIST_ROWS = 19;
const int STATUS_ROW = 28;
const int COMMANDS_ROW = 31;
const int MESSAGE_ROW = 37;
const int COMMAND_LINE_ROW = 45;
const int PORTRAIT_COLUMN = 32;
const int SPEECH_COLUMN = 58;


static void draw_evaluation_bar() {
    if (game.result == STILL_PLAYING && !settings.show_eval) {
        return;
    }

    int x = column_x(1);
    int y = BOARD_VIEW_Y;
    int height = BOARD_VIEW_HEIGHT;

    // in a replay
    float evaluation = game.evaluation;
    int mate_in = game.mate_in;
    if (game.replay_position > 0) {
        evaluation = game.moves[game.replay_position - 1].evaluation;
        mate_in = game.moves[game.replay_position - 1].mate_in;
    } else if (game.replay_position == 0) {
        evaluation = STARTING_EVALUATION;
        mate_in = 0;
    }

    bool hide_mate = mate_in != 0 && !settings.show_mates && game.result == STILL_PLAYING;
    bool showing_mate = mate_in != 0 && !hide_mate;
    const float HIDDEN_MATE_EVALUATION = 9.9f;
    if (hide_mate) evaluation = mate_in > 0 ? HIDDEN_MATE_EVALUATION : -HIDDEN_MATE_EVALUATION;

    bool decided = abs_float(evaluation) >= DECIDED_EVALUATION;

    float white_share;
    if (decided) {
        white_share = evaluation > 0 ? 1.0f : 0.0f;
    } else {
        white_share = 0.5f + 0.47f * evaluation / (abs_float(evaluation) + 3.0f);
    }

    int split_y = y + (int)(height * (1 - white_share));
    for (int row_y = y; row_y < y + height; row_y += SMALL_CHAR_HEIGHT) {
        bool black_part = row_y + SMALL_CHAR_HEIGHT <= split_y;
        u8 character = black_part ? CHAR_LIGHT_SHADE : CHAR_FULL_BLOCK;
        u32 color = black_part ? COLOR_DARK_GRAY : COLOR_LIGHT_GRAY;
        draw_small_character(canvas, x, row_y, character, color);
        draw_small_character(canvas, x + CHAR_WIDTH, row_y, character, color);
    }

    Text label;
    if (showing_mate) {
        label.add_char('M').add_number(abs_int(mate_in));
    } else if (decided) {
        label.add(evaluation > 0 ? "1-0" : "0-1");
    } else {
        if (evaluation >= 0) label.add_char('+');
        label.add_decimal(evaluation);
    }
    write_text(0, 12, label.text, showing_mate ? COLOR_YELLOW : COLOR_WHITE);
}

static void draw_top_area() {
    int side = game.board.side_to_move;
    bool against_computer = is_computer_game();
    int shown = against_computer ? computer_color() : side;

    const Bitmap* picture = nullptr;
    if (against_computer) {
        Face face = game.face;
        bool computer_won = game.result != STILL_PLAYING && game.winner == computer_color();
        if (computer_won && face == FACE_LAUGH1) {

            //laughing
            bool second_picture = (milliseconds_since_start() / BLINK_MILLISECONDS) % 2 == 1;
            face = second_picture ? FACE_LAUGH2 : FACE_LAUGH1;
        }
        picture = &computer_character().faces[face];
    } else if (game.players[shown].portrait >= 0 && game.players[shown].portrait < character_count()) {
        picture = &get_character(game.players[shown].portrait).faces[FACE_DEFAULT];
    }
    if (picture != nullptr) draw_portrait(*picture, PORTRAIT_COLUMN, 0);

    write_text(4, 1, game.players[shown].name, COLOR_WHITE);
    if (against_computer) {
        Character& opponent = computer_character();
        Text elo;
        elo.add("ELO ").add_number(opponent.elo);
        if (opponent.max_strength) elo.add("  MAX");
        write_text(4, 2, elo.text, opponent.max_strength ? COLOR_YELLOW : COLOR_LIGHT_GRAY);
        write_wrapped_text(4, 3, 26, 2, opponent.title, COLOR_DARK_GRAY);
        write_text(4, 6, shown == WHITE ? "WHITE" : "BLACK", COLOR_DARK_GRAY);
    } else {
        write_text(4, 2, shown == WHITE ? "WHITE" : "BLACK", COLOR_LIGHT_GRAY);
    }

    if (game.result == STILL_PLAYING && side == shown) {
        if (game.computer_thinking) write_text(4, 8, "THINKING...", COLOR_YELLOW);
        else write_text(4, 8, "TO MOVE", COLOR_LIGHT_GREEN);
    } else if (game.result == STILL_PLAYING) {
        write_text(4, 8, "WAITING", COLOR_DARK_GRAY);
    }

    if (against_computer && game.speech[0] != 0) {
        write_text(SPEECH_COLUMN - 1, 1, "\"", COLOR_DARK_GRAY);
        write_wrapped_text(SPEECH_COLUMN, 1, 25, 8, game.speech, COLOR_LIGHT_CYAN);
    } else if (!against_computer && game.result == STILL_PLAYING) {
        Text prompt;
        prompt.add(game.players[side].name).add(", your move.");
        write_wrapped_text(SPEECH_COLUMN, 1, 25, 3, prompt.text, COLOR_DARK_GRAY);
    }
}

//---- the panel on the right ----
static void write_result_text(Text& text) {
    const char* winner = game.winner >= 0 ? game.players[game.winner].name : "";
    switch (game.result) {
        case STILL_PLAYING:          text.add(game.players[game.board.side_to_move].name).add(" TO MOVE"); break;
        case CHECKMATE:              text.add("CHECKMATE - ").add(winner).add(" WINS"); break;
        case STALEMATE:              text.add("STALEMATE - DRAW"); break;
        case DRAW_FIFTY_MOVES:       text.add("DRAW - 50-MOVE RULE"); break;
        case DRAW_REPETITION:        text.add("DRAW - REPETITION"); break;
        case DRAW_NOT_ENOUGH_PIECES: text.add("DRAW - INSUFFICIENT MATERIAL"); break;
        case WON_BY_RESIGNATION:     text.add(winner).add(" WINS BY RESIGNATION"); break;
        case DRAW_AGREED:            text.add("DRAW AGREED"); break;
        case DRAW_GAME_TOO_LONG:     text.add("DRAW - GAME TOO LONG"); break;
    }
}

static void draw_move_list() {
    write_text(PANEL, 6, "MOVES", COLOR_DARK_GRAY);
    draw_dashes(PANEL + 6, 6, 35, COLOR_DIM_GRAY);
    if (game.move_count == 0) {
        write_text(PANEL, MOVE_LIST_FIRST_ROW, "no moves yet", COLOR_DARK_GRAY);
        return;
    }

    int shown_up_to = game.replay_position >= 0 ? game.replay_position : game.move_count;
    int full_moves = (game.move_count + 1) / 2;          //one row per white + black move

    // scroll so the latest (or replayed) move is visible
    int first = 0;
    if (full_moves > MOVE_LIST_ROWS) {
        int focus = max_int(0, (shown_up_to - 1) / 2);
        first = clamp_int(focus - MOVE_LIST_ROWS + 1, 0, full_moves - MOVE_LIST_ROWS);
        if (game.replay_position < 0) first = full_moves - MOVE_LIST_ROWS;
    }

    for (int move_row = first; move_row < full_moves && move_row < first + MOVE_LIST_ROWS; move_row++) {
        int row = MOVE_LIST_FIRST_ROW + move_row - first;
        Text number;
        number.add_number(move_row + 1).add_char('.');
        write_text(PANEL + 4 - number.length, row, number.text, COLOR_DARK_GRAY);
        for (int side = 0; side < 2; side++) {
            int index = move_row * 2 + side;
            if (index >= game.move_count) break;
            // dim = not reached in the replay
            u32 color = index < shown_up_to ? COLOR_LIGHT_GRAY : COLOR_DIM_GRAY;
            // the last move shown
            if (index == shown_up_to - 1) color = COLOR_WHITE;
            write_text(PANEL + 6 + side * 14, row, game.moves[index].short_text, color);
        }
    }
}

static void draw_panel() {
    draw_text(canvas, column_x(PANEL), row_y(0), "CHESSOS", COLOR_WHITE, 2);
    char song[23];      // long song names are cut off to fit
    copy_text(song, is_music_playing() ? current_song_name() : "music off", sizeof(song));
    write_text(PANEL + 18, 0, song, COLOR_DARK_GRAY);

    //the two players
    for (int color = 0; color < 2; color++) {
        const Player& player = game.players[color];
        Text line;
        line.add(color == WHITE ? "WHITE  " : "BLACK  ").add(player.name);
        if (player.is_computer) line.add(" (").add_number(get_character(player.character).elo).add(")");
        bool to_move = game.board.side_to_move == color && game.result == STILL_PLAYING;
        write_text(PANEL, 3 + color, line.text, to_move ? COLOR_WHITE : COLOR_LIGHT_GRAY);
    }

    draw_move_list();

    //the status, with one more line below it for check, a draw offer or the replay
    draw_dashes(PANEL, STATUS_ROW - 1, PANEL_WIDTH, COLOR_DIM_GRAY);
    Text status;
    write_result_text(status);
    write_text(PANEL, STATUS_ROW, status.text, game.result == STILL_PLAYING ? COLOR_WHITE : COLOR_YELLOW);
    if (game.result == STILL_PLAYING && is_in_check(game.board, game.board.side_to_move)) {
        write_text(PANEL, STATUS_ROW + 1, "CHECK!", COLOR_LIGHT_RED);
    } else if (game.draw_offered_by >= 0) {
        Text offer;
        offer.add(game.players[game.draw_offered_by].name).add(" offers a draw");
        write_text(PANEL, STATUS_ROW + 1, offer.text, COLOR_LIGHT_CYAN);
    } else if (game.result != STILL_PLAYING) {
        Text replay;
        if (game.replay_position >= 0) {
            replay.add("REPLAY ").add_number(game.replay_position).add_char('/').add_number(game.move_count).add("   B back  M forward");
        } else {
            replay.add("B / M  step through the game");
        }
        write_text(PANEL, STATUS_ROW + 1, replay.text, COLOR_LIGHT_CYAN);
    }

    write_text(PANEL, COMMANDS_ROW, "COMMANDS", COLOR_DARK_GRAY);
    write_text(PANEL + 2, COMMANDS_ROW + 1, "/select", COLOR_LIGHT_GRAY);
    write_text(PANEL + 10, COMMANDS_ROW + 1, "e2", COLOR_DARK_GRAY);
    write_text(PANEL + 2, COMMANDS_ROW + 2, "/move", COLOR_LIGHT_GRAY);
    write_text(PANEL + 10, COMMANDS_ROW + 2, "e4", COLOR_DARK_GRAY);
    write_text(PANEL + 2, COMMANDS_ROW + 3, "/resign", COLOR_LIGHT_GRAY);
    write_text(PANEL + 2, COMMANDS_ROW + 4, "/offerdraw", COLOR_LIGHT_GRAY);
    write_text(PANEL + 20, COMMANDS_ROW + 1, "/undo  /new", COLOR_DARK_GRAY);
    write_text(PANEL + 20, COMMANDS_ROW + 2, "/menu  /help", COLOR_DARK_GRAY);
    write_text(PANEL + 20, COMMANDS_ROW + 3, "chain with &&", COLOR_DARK_GRAY);
    write_text(PANEL + 20, COMMANDS_ROW + 4, "underpromote with", COLOR_DARK_GRAY);
    write_text(PANEL + 20, COMMANDS_ROW + 5, "/move (use /help)", COLOR_DARK_GRAY);

    write_wrapped_text(PANEL, MESSAGE_ROW, PANEL_WIDTH, 5, game.message, game.message_color);

    //the command line, with a blinking block cursor
    draw_dashes(PANEL, COMMAND_LINE_ROW - 1, PANEL_WIDTH, COLOR_DIM_GRAY);
    write_text(PANEL, COMMAND_LINE_ROW, ">", COLOR_LIGHT_GREEN);
    const int VISIBLE_LENGTH = 38;
    int length = text_length(game.typed_command);
    int start = length > VISIBLE_LENGTH ? length - VISIBLE_LENGTH : 0;   // show the end of a long command
    write_text(PANEL + 2, COMMAND_LINE_ROW, game.typed_command + start, COLOR_WHITE);
    const int CURSOR_BLINK_MILLISECONDS = 450;
    if ((milliseconds_since_start() / CURSOR_BLINK_MILLISECONDS) % 2 == 0) {
        draw_character(canvas, column_x(PANEL + 2 + length - start), row_y(COMMAND_LINE_ROW), CHAR_FULL_BLOCK, COLOR_LIGHT_GREEN);
    }
}

static void draw_the_board() {
    BoardViewState state;
    bool replaying = game.replay_position >= 0;
    const ChessBoard& shown = replaying ? game.replay_board : game.board;
    state.squares = shown.squares;
    state.selected_square = game.selected_square;
    state.target_squares = game.target_squares;
    state.hovered_square = game.hovered_square;

    state.last_move_from = -1;
    state.last_move_to = -1;
    int moves_shown = replaying ? game.replay_position : game.move_count;
    if (moves_shown > 0) {
        state.last_move_from = game.moves[moves_shown - 1].move.from;
        state.last_move_to = game.moves[moves_shown - 1].move.to;
    }

    state.king_in_check_square = -1;
    bool check_matters = game.result == STILL_PLAYING || game.result == CHECKMATE;
    if (!replaying && check_matters && is_in_check(game.board, game.board.side_to_move)) {
        state.king_in_check_square = game.board.king_square[game.board.side_to_move];
    }

    state.animating = game.animating && !replaying;
    state.animation_piece = game.animation_piece;
    state.animation_from = game.animation_from;
    state.animation_to = game.animation_to;
    float progress = (float)(milliseconds_since_start() - game.animation_start) / ANIMATION_MILLISECONDS;
    state.animation_progress = clamp_float(progress, 0, 1);
    draw_board(state);
}

static void draw_game_over_box() {
    Text title;
    if (game.result == CHECKMATE) title.add("CHECKMATE  ").add(game.players[game.winner].name).add(" WINS");
    else if (game.result == WON_BY_RESIGNATION) title.add(game.players[game.winner].name).add(" WINS BY RESIGNATION");
    else title.add("GAME DRAWN");
    const char* help = "B/M replay   /new rematch   /menu";

    int width = max_int(title.length, 36) + 4;
    int left = 4 + (80 - width) / 2;
    int top = 27;
    draw_box(left, top, width, 4, COLOR_LIGHT_GRAY);
    write_text(left + (width - title.length) / 2, top + 1, title.text, COLOR_YELLOW);
    write_text(left + (width - text_length(help)) / 2, top + 2, help, COLOR_LIGHT_GRAY);
}

void draw_game_screen() {
    if (board_needs_redraw) {
        draw_the_board();
        board_needs_redraw = false;
    }
    fill_rectangle(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BLACK);
    draw_top_area();

    //copy the finished board picture onto the canvas
    Image& board = board_view_image();
    for (int y = 0; y < BOARD_VIEW_HEIGHT; y++) {
        memcpy(canvas.pixels + (BOARD_VIEW_Y + y) * CANVAS_WIDTH + BOARD_VIEW_X, board.pixels + y * BOARD_VIEW_WIDTH, BOARD_VIEW_WIDTH * 4);
    }
    draw_evaluation_bar();

    //the help line at the bottom
    Text view_line;
    view_line.add("VIEW ").add_number(current_board_view() + 1).add("/8 ").add(board_view_name()).add("   ");
    view_line.add_char(CHAR_ARROW_LEFT).add_char(' ').add_char(CHAR_ARROW_RIGHT).add(" rotate   F2 music  F3 next song  F5 restart game  F10 menu");
    write_text(4, 47, view_line.text, COLOR_DARK_GRAY);

    if (game.replay_position >= 0) {
        Text replay;
        replay.add(" REPLAY ").add_number(game.replay_position).add_char('/').add_number(game.move_count).add_char(' ');
        draw_text_with_background(canvas, BOARD_VIEW_X + CHAR_WIDTH, BOARD_VIEW_Y + CHAR_WIDTH, replay.text, COLOR_BLACK, COLOR_LIGHT_CYAN);
    }
    if (game.result != STILL_PLAYING && game.replay_position < 0) draw_game_over_box();
    draw_panel();
}
