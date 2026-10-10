#pragma once
#include "types.h"
#include "chess_rules.h"
#include "characters.h"
#include "keyboard_mouse.h"

enum GameResult {
    STILL_PLAYING, CHECKMATE, STALEMATE, DRAW_FIFTY_MOVES, DRAW_REPETITION,
    DRAW_NOT_ENOUGH_PIECES, WON_BY_RESIGNATION, DRAW_AGREED, DRAW_GAME_TOO_LONG
};

struct Player {
    bool is_computer;
    char name[20];
    int portrait;
    int character;
};

struct PlayedMove {
    Move move;
    UndoInfo undo;
    char short_text[20];
    char notation[10];
    float evaluation;
    int mate_in;
};

const int MAX_GAME_MOVES = 1024;

struct Game {
    Player players[2];                 // [WHITE] and [BLACK]
    ChessBoard board;
    Move legal_moves[MAX_MOVES];
    int legal_move_count;
    PlayedMove moves[MAX_GAME_MOVES];
    int move_count;
    u64 position_keys[MAX_GAME_MOVES + 1];   // every position so far
    int position_key_count;
    GameResult result;
    int winner;                        // WHITE or BLACK (when someone won)
    int draw_offered_by;               // -1 = nobody

    float evaluation;                  // in pawns, + is good for White (the bar on the left)
    int mate_in;                       // forced mate: + for White, - for Black, 0 = none

    int selected_square;               //-1 = none
    //where the selected piece can go
    u64 target_squares;
    int hovered_square;
    int drag_from_square;

    int replay_position;               // -1 = live game; otherwise how many moves are shown
    ChessBoard replay_board;

    // a piece is sliding to its new square
    bool animating;
    u64 animation_start;
    int animation_piece, animation_from, animation_to;

    u64 computer_may_move_at;          // a short pause before the computer moves
    bool computer_thinking;

    Face face;                         //the computer opponent's expression
    char speech[160];                  //what the opponent is saying
    int evaluation_before_human_move;  // to notice when the human blunders

    char message[160];                 // the message line in the panel
    u32 message_color;
    char typed_command[72];            // what is being typed in the command line
    char last_command[72];
};
extern Game game;

const int ANIMATION_MILLISECONDS = 230;
const float DECIDED_EVALUATION = 99;         // an evaluation this big (in pawns) means someone has won
// white starts slightly better
const float STARTING_EVALUATION = 0.2f;

void start_new_game();
// animations, and the computer's turn
void game_update();
void game_handle_key(const InputEvent& event);
void game_handle_mouse(const InputEvent& event);
void game_mouse_moved();
void draw_game_screen();

// Used by game.cpp, commands.cpp and game_screen.cpp
bool is_computer_game();
int computer_color();
Character& computer_character();
bool is_humans_turn();
void show_message(const char* text, u32 color);
void say(const char* text);
void select_square(int square);
void clear_selection();
bool try_human_move(int from, int to, int promotion);
void take_back_move();
void resign_game();
void offer_draw();
void run_command_line(const char* line);
void set_face(Face face);
void react_to_position(int mover, bool rook_blundered);
