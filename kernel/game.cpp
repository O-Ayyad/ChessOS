#include "game.h"
#include "menu.h"
#include "ui.h"
#include "chess_ai.h"
#include "board_view.h"
#include "sound.h"
#include "timer.h"
#include "math_utils.h"
#include "text_utils.h"
#include "drawing.h"

Game game;

// milliseconds, after the game starts
const int PAUSE_BEFORE_COMPUTER_MOVES = 900;
const int PAUSE_AFTER_A_MOVE = 350;
// a quick look after every move, for the evaluation bar and faces
const int ANALYSIS_MILLISECONDS = 220;
const int ANALYSIS_DEPTH = 7;
const int THREAT_CHECK_MILLISECONDS = 120;
const int THREAT_CHECK_DEPTH = 5;
// shown when the game is decided
const float WINNING_EVALUATION = 100.0f;

bool is_computer_game() { return settings.mode == MODE_VS_COMPUTER; }
int computer_color() { return game.players[WHITE].is_computer ? WHITE : BLACK; }
Character& computer_character() { return get_character(game.players[computer_color()].character); }

bool is_humans_turn() {
    return game.result == STILL_PLAYING && !game.players[game.board.side_to_move].is_computer;
}

// Shows a text in the message area of the panel.
void show_message(const char* text, u32 color) {
    copy_text(game.message, text, sizeof(game.message));
    game.message_color = color;
    screen_needs_redraw = true;
}

// Makes the computer opponent say something (nullptr = keep what it said before).
void say(const char* text) {
    if (text == nullptr) return;
    copy_text(game.speech, text, sizeof(game.speech));
    screen_needs_redraw = true;
}

void set_face(Face face) {
    if (game.face != face) {
        game.face = face;
        screen_needs_redraw = true;
    }
}

void clear_selection() {
    game.selected_square = -1;
    game.target_squares = 0;
    board_needs_redraw = true;
    screen_needs_redraw = true;
}

// Selects the piece on a square and marks where it can go. target_squares has
// one bit for each of the 64 squares.
void select_square(int square) {
    game.selected_square = square;
    game.target_squares = 0;
    for (int i = 0; i < game.legal_move_count; i++) {
        if (game.legal_moves[i].from == square) game.target_squares |= 1ULL << game.legal_moves[i].to;
    }
    board_needs_redraw = true;
    screen_needs_redraw = true;
}

// ---- is the game over? ----
// True if only the kings are left, or the kings and one knight or bishop.
static bool not_enough_pieces_to_mate() {
    int minor_pieces = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {
        int type = piece_type(game.board.squares[square]);
        if (type == KNIGHT || type == BISHOP) minor_pieces++;
        else if (type != EMPTY && type != KING) return false;
    }
    return minor_pieces <= 1;
}

// Works out the legal moves and whether the game has ended.
static void update_result() {
    game.legal_move_count = generate_legal_moves(game.board, game.legal_moves);
    game.result = STILL_PLAYING;
    game.winner = -1;
    if (game.legal_move_count == 0) {
        if (is_in_check(game.board, game.board.side_to_move)) {
            game.result = CHECKMATE;
            game.winner = other_color(game.board.side_to_move);
        } else {
            game.result = STALEMATE;
        }
    } else if (game.board.halfmove_clock >= FIFTY_MOVE_RULE_HALFMOVES) {
        game.result = DRAW_FIFTY_MOVES;
    } else if (game.move_count >= MAX_GAME_MOVES) {
        // we have no room to remember more moves
        game.result = DRAW_GAME_TOO_LONG;
    } else if (not_enough_pieces_to_mate()) {
        game.result = DRAW_NOT_ENOUGH_PIECES;
    } else {
        //the same position for the third time is a draw
        int times_seen = 0;
        for (int i = 0; i < game.position_key_count; i++) {
            if (game.position_keys[i] == game.board.position_key) times_seen++;
        }
        if (times_seen >= 3) game.result = DRAW_REPETITION;
    }
}

// ---- the evaluation bar ----
// A "ply" is one move by one side, so a mate in 3 plies is a mate in 2 moves.
static int plies_to_moves(int plies) {
    return (plies + 1) / 2;
}

//A short search after every move, to see who stands better.
static void update_evaluation() {
    game.mate_in = 0;
    if (game.result == CHECKMATE) {
        game.evaluation = game.winner == WHITE ? WINNING_EVALUATION : -WINNING_EVALUATION;
        return;
    }
    if (game.result != STILL_PLAYING) {
        game.evaluation = 0;
        return;
    }

    SearchResult analysis = ai_search(game.board, ANALYSIS_DEPTH, ANALYSIS_MILLISECONDS, game.position_keys, game.position_key_count);

    // the search's scores are for the side to move; ours are for White
    int sign = color_sign(game.board.side_to_move);
    if (analysis.mate_in_plies > 0) {
        game.mate_in = sign * plies_to_moves(analysis.mate_in_plies);
        game.evaluation = sign * WINNING_EVALUATION;
    } else if (analysis.mate_in_plies < 0) {
        game.mate_in = -sign * plies_to_moves(-analysis.mate_in_plies);
        game.evaluation = -sign * WINNING_EVALUATION;
    } else {
        game.evaluation = sign * analysis.score / (float)CENTIPAWNS_PER_PAWN;
    }

    // Does the computer threaten a mate? Pretend the human passes, and see
    // whether the computer could then force one.
    bool humans_turn = is_computer_game() && game.board.side_to_move != computer_color();
    if (game.mate_in == 0 && humans_turn && !is_in_check(game.board, game.board.side_to_move)) {
        UndoInfo undo;
        make_null_move(game.board, undo);
        SearchResult threat = ai_search(game.board, THREAT_CHECK_DEPTH, THREAT_CHECK_MILLISECONDS, game.position_keys, game.position_key_count);
        undo_null_move(game.board, undo);
        if (threat.mate_in_plies > 0) {
            game.mate_in = color_sign(computer_color()) * plies_to_moves(threat.mate_in_plies);
        }
    }
}

//The evaluation as the computer sees it: in centipawns, + is good for the computer.
static int computer_evaluation() {
    float for_computer = computer_color() == WHITE ? game.evaluation : -game.evaluation;
    return (int)(for_computer * CENTIPAWNS_PER_PAWN);
}

// ---- the computer's face and what it says ----
// All in centipawns, from the computer's side
const int BLUNDER_SIZE = 250;         // a human move that gives the computer this much more is a blunder
const int ALREADY_WINNING = 600;      // ... unless the computer was already this far ahead
const int HAPPY_SAD_MIN_DIFF = 200;   // a happy or sad face from this advantage
const int WORTH_TALKING_ABOUT = 300;
const int CHANCE_OF_REMARK = 25;      // percent

// Is the rook on "square" hanging (can be taken for free, or by a cheaper piece)?
static bool rook_is_hanging(const ChessBoard& board, int square, int owner) {
    if (piece_type(board.squares[square]) != ROOK || piece_color(board.squares[square]) != owner) return false;
    int attacker = cheapest_attacker(board, square, other_color(owner));
    if (attacker == EMPTY) return false;
    bool defended = is_square_attacked(board, square, owner);
    return attacker < ROOK || !defended;
}

// Picks the computer opponent's face and line for the new position. "mover"
// is the color that just moved. "my" here means the computer's.
// rook_blundered is not used at the moment.
void react_to_position(int mover, bool rook_blundered) {
    if (!is_computer_game()) return;
    Character& character = computer_character();
    int my_color = computer_color();
    int my_evaluation = computer_evaluation();

    // the game is over
    if (game.result != STILL_PLAYING) {
        if (game.result == CHECKMATE || game.result == WON_BY_RESIGNATION) {
            if (game.winner == my_color) {
                set_face(FACE_LAUGH1);
                say(pick_line(character, LINE_WIN));
            } else {
                set_face(FACE_DEFEAT);
                say(pick_line(character, LINE_LOSE));
            }
        } else {
            set_face(FACE_DEFAULT);
            say(pick_line(character, LINE_DRAW));
        }
        return;
    }

    // the human just blundered
    bool human_moved = mover != my_color;
    if (human_moved && my_evaluation - game.evaluation_before_human_move >= BLUNDER_SIZE &&
        game.evaluation_before_human_move < ALREADY_WINNING) {
        set_face(FACE_SHOCKED);
        say(pick_line(character, LINE_SHOCKED));
        return;
    }

    // someone has a forced mate
    if (settings.mate_faces && game.mate_in != 0) {
        int mating_color = game.mate_in > 0 ? WHITE : BLACK;
        bool mate_in_one = abs_int(game.mate_in) <= 1;

        if (mating_color == my_color) {
            set_face(mate_in_one ? FACE_VICTORY : FACE_HAPPY);
            say(pick_line(character, mate_in_one ? LINE_VICTORY : LINE_SMIRK));
        } else {
            set_face(mate_in_one ? FACE_DEFEAT : FACE_SCARED);
            say(pick_line(character, mate_in_one ? LINE_DEFEAT : LINE_SCARED));
        }
        return;
    }

    // otherwise: happy when ahead, sad when behind
    Face new_face = FACE_DEFAULT;
    if (my_evaluation >= HAPPY_SAD_MIN_DIFF) new_face = FACE_HAPPY;
    if (my_evaluation <= -HAPPY_SAD_MIN_DIFF) new_face = FACE_SAD;
    bool face_changed = new_face != game.face;
    set_face(new_face);

    // and now and then a remark about it
    bool big_difference = my_evaluation >= WORTH_TALKING_ABOUT || my_evaluation <= -WORTH_TALKING_ABOUT;
    bool in_the_mood = face_changed || random_below(100) < CHANCE_OF_REMARK;
    if (new_face != FACE_DEFAULT && big_difference && in_the_mood) {
        say(pick_line(character, new_face == FACE_HAPPY ? LINE_TAUNT : LINE_HUMBLE));
    }
}

// ---- making moves ----
//The text for the move list: "e2 e4", "d4xe5", "O-O", with the + or # of the notation.
static void make_short_text(const Move& move, const char* notation, char* out) {
    Text text;
    char from[3], to[3];
    square_to_text(move.from, from);
    square_to_text(move.to, to);
    if (move.flags & MOVE_CASTLE) {
        text.add(move.to > move.from ? "O-O" : "O-O-O");
    } else {
        text.add(from).add((move.flags & MOVE_CAPTURE) ? "x" : " ").add(to);
        if (move.flags & MOVE_PROMOTION) text.add_char('=').add_char(" PNBRQK"[move.promotion]);
    }

    int length = text_length(notation);
    if (length > 0 && (notation[length - 1] == '+' || notation[length - 1] == '#')) text.add_char(notation[length - 1]);
    copy_text(out, text.text, sizeof(PlayedMove::short_text));
}

//Stores the evaluation with the last move, for the replay.
static void remember_evaluation() {
    if (game.move_count == 0) return;
    game.moves[game.move_count - 1].evaluation = game.evaluation;
    game.moves[game.move_count - 1].mate_in = game.mate_in;
}

static void play_sound_for_move(Move move) {
    if (game.result != STILL_PLAYING) {
        play_sound_effect(SOUND_GAME_OVER);
    } else if (is_in_check(game.board, game.board.side_to_move)) {
        play_sound_effect(SOUND_CHECK);
    } else if (move.flags & MOVE_CASTLE) {
        play_sound_effect(SOUND_CASTLE);
    } else if (move.flags & MOVE_CAPTURE) {
        play_sound_effect(SOUND_CAPTURE);
    } else {
        play_sound_effect(SOUND_MOVE);
    }
}

//Makes a move on the board (the human's or the computer's) and does
//everything that comes with it: the move list, the animation, the result,
//the evaluation, the opponent's reaction and the sound.
static void play_move(Move move) {
    int mover = game.board.side_to_move;
    if (is_computer_game() && mover != computer_color()) {
        game.evaluation_before_human_move = computer_evaluation();
    }

    //which rooks were already hanging before the move (to spot new rook blunders)
    bool rook_was_hanging[SQUARE_COUNT];
    for (int square = 0; square < SQUARE_COUNT; square++) {
        rook_was_hanging[square] = rook_is_hanging(game.board, square, mover);
    }

    //write the move down (before making it: the notation needs the old position)
    PlayedMove& played = game.moves[game.move_count];
    played.move = move;
    move_to_notation(game.board, move, played.notation);
    make_short_text(move, played.notation, played.short_text);

    game.animation_piece = game.board.squares[move.from];
    game.animation_from = move.from;
    game.animation_to = move.to;

    make_move(game.board, move, played.undo);
    game.move_count++;
    game.position_keys[game.position_key_count] = game.board.position_key;
    game.position_key_count++;
    // slide the new piece, not the pawn
    if (move.flags & MOVE_PROMOTION) game.animation_piece = game.board.squares[move.to];

    //moving without answering a draw offer declines it
    if (game.draw_offered_by >= 0 && game.draw_offered_by != mover) game.draw_offered_by = -1;

    //a rook of the human's that is newly hanging after the move is a "blunder"
    bool rook_blundered = false;
    bool human_moved = !is_computer_game() || mover != computer_color();
    if (human_moved) {
        for (int square = 0; square < SQUARE_COUNT; square++) {
            if (rook_is_hanging(game.board, square, mover) && !rook_was_hanging[square]) rook_blundered = true;
        }
    }

    clear_selection();
    game.animating = true;
    game.animation_start = milliseconds_since_start();

    Text message;
    message.add(game.players[mover].name).add(": ").add(played.short_text).add("  (").add(played.notation).add(")");
    show_message(message.text, COLOR_LIGHT_GRAY);

    update_result();
    update_evaluation();
    remember_evaluation();
    react_to_position(mover, rook_blundered);
    board_needs_redraw = true;
    screen_needs_redraw = true;

    play_sound_for_move(move);
    game.computer_may_move_at = milliseconds_since_start() + ANIMATION_MILLISECONDS + PAUSE_AFTER_A_MOVE;
}

bool try_human_move(int from, int to, int promotion) {
    if (game.result != STILL_PLAYING) {
        show_message("The game is over. Press B/M to replay it, or type /new.", COLOR_LIGHT_RED);
        return false;
    }
    if (!is_humans_turn()) {
        show_message("Wait - it's the computer's turn.", COLOR_LIGHT_RED);
        return false;
    }
    //promote to a queen unless told otherwise
    if (promotion == 0) promotion = QUEEN;
    for (int i = 0; i < game.legal_move_count; i++) {
        Move move = game.legal_moves[i];
        bool promotion_matches = !(move.flags & MOVE_PROMOTION) || move.promotion == promotion;
        if (move.from == from && move.to == to && promotion_matches) {
            play_move(move);
            return true;
        }
    }
    // explain why it's not allowed
    char from_text[3], to_text[3];
    square_to_text(from, from_text);
    square_to_text(to, to_text);
    Text message;
    message.add("Illegal move: ").add(from_text).add_char(' ').add(to_text);
    if (game.board.squares[from] == EMPTY) {
        message.add(" (there is no piece on ").add(from_text).add(")");
    } else if (piece_color(game.board.squares[from]) != game.board.side_to_move) {
        message.add(" (that's not your piece)");
    } else if (is_in_check(game.board, game.board.side_to_move)) {
        message.add(" (you are in check)");
    }
    show_message(message.text, COLOR_LIGHT_RED);
    play_sound_effect(SOUND_ILLEGAL);
    return false;
}

void take_back_move() {
    // Against the computer, take back a whole turn: its move and yours.
    // If it is the computer's turn, only your last move is taken back.
    int moves_to_undo = 1;
    if (is_computer_game() && !game.players[game.board.side_to_move].is_computer) moves_to_undo = 2;
    if (game.move_count < moves_to_undo) {
        show_message("There is nothing to take back.", COLOR_LIGHT_RED);
        return;
    }
    for (int i = 0; i < moves_to_undo; i++) {
        game.move_count--;
        game.position_key_count--;
        undo_move(game.board, game.moves[game.move_count].undo);
    }
    game.draw_offered_by = -1;
    game.animating = false;
    game.replay_position = -1;
    clear_selection();
    update_result();
    update_evaluation();
    remember_evaluation();
    set_face(FACE_DEFAULT);
    Text message;
    message.add("Took back. ").add(game.players[game.board.side_to_move].name).add(" to move.");
    show_message(message.text, COLOR_LIGHT_GRAY);
    game.computer_may_move_at = milliseconds_since_start() + PAUSE_AFTER_A_MOVE * 2;
    board_needs_redraw = true;
}

void resign_game() {
    if (!is_humans_turn()) {
        show_message("You can resign on your own turn.", COLOR_LIGHT_RED);
        return;
    }
    int loser = game.board.side_to_move;
    game.result = WON_BY_RESIGNATION;
    game.winner = other_color(loser);
    game.evaluation = game.winner == WHITE ? WINNING_EVALUATION : -WINNING_EVALUATION;
    clear_selection();
    Text message;
    message.add(game.players[loser].name).add(" resigned. ").add(game.players[game.winner].name).add(" wins.");
    show_message(message.text, COLOR_LIGHT_GRAY);
    react_to_position(loser, false);
    play_sound_effect(SOUND_GAME_OVER);
}

void offer_draw() {
    if (is_computer_game()) {
        // the computer accepts a draw only if it is not better
        const int ACCEPT_DRAW_BELOW = 30;
        if (computer_evaluation() <= ACCEPT_DRAW_BELOW) {
            game.result = DRAW_AGREED;
            clear_selection();
            show_message("The computer accepts your draw offer.", COLOR_LIGHT_GRAY);
            react_to_position(game.board.side_to_move, false);
            play_sound_effect(SOUND_GAME_OVER);
        } else {
            show_message("The computer declines your draw offer.", COLOR_YELLOW);
            say(pick_line(computer_character(), LINE_TAUNT));
        }
        board_needs_redraw = true;
        return;
    }
    game.draw_offered_by = game.board.side_to_move;
    Text message;
    message.add(game.players[game.board.side_to_move].name).add(" offers a draw. ");
    message.add(game.players[other_color(game.board.side_to_move)].name).add(": /accept or /decline.");
    show_message(message.text, COLOR_LIGHT_CYAN);
}

// ---- starting and running the game ----
void start_new_game() {
    // set up the two players from the menu's settings
    if (settings.mode == MODE_VS_COMPUTER) {
        int human_color = settings.color;       // PLAY_WHITE and PLAY_BLACK have the same values as WHITE and BLACK
        if (settings.color == PLAY_RANDOM) human_color = random_below(2);

        Player& human = game.players[human_color];
        human.is_computer = false;
        copy_text(human.name, settings.player_name[0], sizeof(human.name));
        human.portrait = settings.player_portrait[0];
        human.character = -1;

        Player& opponent = game.players[other_color(human_color)];
        opponent.is_computer = true;
        copy_text(opponent.name, get_character(settings.opponent).name, sizeof(opponent.name));
        opponent.portrait = -1;
        opponent.character = settings.opponent;

        set_board_view(human_color == WHITE ? 0 : 4);     // look from your own side
    } else {
        for (int color = 0; color < 2; color++) {
            game.players[color].is_computer = false;
            copy_text(game.players[color].name, settings.player_name[color], sizeof(game.players[color].name));
            game.players[color].portrait = settings.player_portrait[color];
            game.players[color].character = -1;
        }
        set_board_view(0);
    }

    set_starting_position(game.board);
    game.move_count = 0;
    game.position_key_count = 0;
    game.position_keys[game.position_key_count++] = game.board.position_key;
    game.draw_offered_by = -1;
    game.animating = false;
    game.selected_square = -1;
    game.target_squares = 0;
    game.hovered_square = -1;
    game.drag_from_square = -1;
    game.replay_position = -1;
    game.computer_thinking = false;

    ai_new_game();
    update_result();
    game.evaluation = STARTING_EVALUATION;
    game.mate_in = 0;
    game.face = FACE_DEFAULT;
    game.speech[0] = 0;
    game.evaluation_before_human_move = 0;
    game.typed_command[0] = 0;
    if (is_computer_game()) say(pick_line(computer_character(), LINE_GREET));

    Text message;
    message.add("New game. ").add(game.players[WHITE].name).add(" (white) vs ").add(game.players[BLACK].name).add(" (black).");
    show_message(message.text, COLOR_LIGHT_GRAY);
    game.computer_may_move_at = milliseconds_since_start() + PAUSE_BEFORE_COMPUTER_MOVES;
    play_sound_effect(SOUND_GAME_START);
    current_screen = SCREEN_GAME;
    board_needs_redraw = true;
    screen_needs_redraw = true;
}

// Called all the time by the main loop
void game_update() {
    u64 now = milliseconds_since_start();
    if (game.animating) {
        if (now - game.animation_start >= (u64)ANIMATION_MILLISECONDS) game.animating = false;
        board_needs_redraw = true;
    }

    bool computers_turn = game.result == STILL_PLAYING && game.players[game.board.side_to_move].is_computer;
    if (computers_turn && !game.animating && now >= game.computer_may_move_at && game.replay_position < 0) {
        //"THINKING"
        game.computer_thinking = true;
        draw_game_screen();
        show_everything();

        Character& character = computer_character();
        AiLevel level = ai_level_for_elo(character.elo, character.max_strength);
        Move move = ai_choose_move(game.board, level, game.position_keys, game.position_key_count);
        game.computer_thinking = false;

        if (current_screen == SCREEN_GAME && game.result == STILL_PLAYING && move.from != move.to) play_move(move);
    }
}
