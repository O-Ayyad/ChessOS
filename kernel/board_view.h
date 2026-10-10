#pragma once
#include "types.h"
#include "drawing.h"
#include "bitmap.h"

const int BOARD_VIEW_X = 4 * CHAR_WIDTH;
const int BOARD_VIEW_Y = 13 * CHAR_HEIGHT;
const int BOARD_VIEW_WIDTH = 80 * CHAR_WIDTH;
const int BOARD_VIEW_HEIGHT = 34 * CHAR_HEIGHT;
const int VIEW_COUNT = 8;

// What to draw
struct BoardViewState {
    const i8* squares;       // 64 squares
    int selected_square;     // -1 is none
    u64 target_squares;      // bit N set where square N is a legal destination for the selected piece
    int last_move_from;      // -1 is none
    int last_move_to;
    int king_in_check_square;   //-1 means no king is in check
    int hovered_square;      //under the mouse
    bool animating;          //a piece is moving from animation_from to animation_to
    int animation_piece;
    int animation_from;
    int animation_to;
    float animation_progress;   // 0 to 1
};

//[color][piece type][size: small, medium, large][rotation]
const int PIECE_SIZE_COUNT = 3;
const int PIECE_ROTATIONS = 8;
struct PiecePictures {

    Bitmap picture[2][7][PIECE_SIZE_COUNT][PIECE_ROTATIONS];
    VisibleArea visible[2][7][PIECE_SIZE_COUNT][PIECE_ROTATIONS];

};

void setup_board_view(PiecePictures* pictures);
void set_board_view(int view_number);
int  current_board_view();
const char* board_view_name();
void draw_board(const BoardViewState& state);
Image& board_view_image();
int  square_at_view_position(int x, int y);
