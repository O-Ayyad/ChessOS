#pragma once
#include "types.h"

enum PieceType { EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };
enum Color { WHITE = 0, BLACK = 1 };

static inline int piece_type(int piece)  { return piece < 0 ? -piece : piece; }
static inline int piece_color(int piece) { return piece > 0 ? WHITE : BLACK; }
static inline int other_color(int color) { return color == WHITE ? BLACK : WHITE; }
static inline int color_sign(int color)  { return color == WHITE ? 1 : -1; }

const int NO_SQUARE = -1;
const int SQUARE_COUNT = 64;
static inline int square_file(int square) { return square % 8; }
static inline int square_rank(int square) { return square / 8; }
static inline int make_square(int file, int rank) { return rank * 8 + file; }
static inline bool is_on_board(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

const int SQUARE_A1 = 0, SQUARE_E1 = 4, SQUARE_H1 = 7;
const int SQUARE_A8 = 56, SQUARE_E8 = 60, SQUARE_H8 = 63;

int  text_to_square(const char* text);
void square_to_text(int square, char* out);

const u8 MOVE_CAPTURE = 1;
const u8 MOVE_EN_PASSANT = 2;
const u8 MOVE_CASTLE = 4;
const u8 MOVE_PAWN_DOUBLE_STEP = 8;
const u8 MOVE_PROMOTION = 16;

const int MAX_MOVES = 256;
const int FIFTY_MOVE_RULE_HALFMOVES = 100;

struct Move {
    u8 from;
    u8 to;
    i8 promotion;
    u8 flags;
};
bool same_move(Move a, Move b);
Move no_move();

const u8 WHITE_KING_SIDE = 1;
const u8 WHITE_QUEEN_SIDE = 2;
const u8 BLACK_KING_SIDE = 4;
const u8 BLACK_QUEEN_SIDE = 8;
const u8 ALL_CASTLING = 15;

struct ChessBoard {
    i8 squares[64];
    int side_to_move;
    u8 castling_rights;
    int en_passant_square;
    int halfmove_clock;
    int move_number;
    int king_square[2];
    u64 position_key;
};

// Everything needed to take a move back again.
struct UndoInfo {
    Move move;
    i8 captured_piece;
    u8 castling_rights;
    int en_passant_square;
    int halfmove_clock;
    u64 position_key;
};

void setup_chess_rules();
void set_starting_position(ChessBoard& board);
bool is_square_attacked(const ChessBoard& board, int square, int by_color);
bool is_in_check(const ChessBoard& board, int color);
int  cheapest_attacker(const ChessBoard& board, int square, int by_color);
int  generate_moves(const ChessBoard& board, Move* moves, bool captures_only);
int  generate_legal_moves(ChessBoard& board, Move* moves);

void make_move(ChessBoard& board, Move move, UndoInfo& undo);
void undo_move(ChessBoard& board, const UndoInfo& undo);
void make_null_move(ChessBoard& board, UndoInfo& undo);
void undo_null_move(ChessBoard& board, const UndoInfo& undo);
void move_to_notation(ChessBoard& board, Move move, char* out);
