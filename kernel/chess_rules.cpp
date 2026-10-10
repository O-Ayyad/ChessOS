#include "chess_rules.h"
#include "text_utils.h"

//https://en.wikipedia.org/wiki/Zobrist_hashing

const int PIECE_KINDS = 13;         //6 black pieces empty  6 white pieces
static u64 key_for_piece[PIECE_KINDS][SQUARE_COUNT];
static u64 key_for_black_to_move;
static u64 key_for_castling[16];
static u64 key_for_en_passant_file[8];

static u64 key_random_state = 0x9E3779B97F4A7C15ULL;
static u64 random_64_bits() {
    key_random_state ^= key_random_state << 13;
    key_random_state ^= key_random_state >> 7;
    key_random_state ^= key_random_state << 17;
    return key_random_state;
}

static u64 piece_key(int piece, int square) {
    return key_for_piece[piece + 6][square];
}

void setup_chess_rules() {
    for (int piece = 0; piece < PIECE_KINDS; piece++) {
        for (int square = 0; square < SQUARE_COUNT; square++) {
            key_for_piece[piece][square] = random_64_bits();
        }
    }
    key_for_black_to_move = random_64_bits();
    for (int i = 0; i < 16; i++) key_for_castling[i] = random_64_bits();
    for (int i = 0; i < 8; i++) key_for_en_passant_file[i] = random_64_bits();
}

static u64 calculate_position_key(const ChessBoard& board) {
    u64 key = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {
        if (board.squares[square] != EMPTY) key ^= piece_key(board.squares[square], square);
    }
    if (board.side_to_move == BLACK) key ^= key_for_black_to_move;
    key ^= key_for_castling[board.castling_rights];
    if (board.en_passant_square != NO_SQUARE) key ^= key_for_en_passant_file[square_file(board.en_passant_square)];
    return key;
}

int text_to_square(const char* text) {
    char file = to_lower_case(text[0]);
    if (file < 'a' || file > 'h') return NO_SQUARE;

    char rank = text[1];

    if (rank < '1' || rank > '8') return NO_SQUARE;
    return make_square(file - 'a', rank - '1');
}

void square_to_text(int square, char* out) {
    out[0] = 'a' + square_file(square);
    out[1] = '1' + square_rank(square);
    out[2] = 0;
}

bool same_move(Move a, Move b) {
    return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

Move no_move() {
    Move move = { 0, 0, 0, 0 };
    return move;
}

void set_starting_position(ChessBoard& board) {
    const int BACK_ROW[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
    const int WHITE_BACK_RANK = 0, WHITE_PAWN_RANK = 1, BLACK_PAWN_RANK = 6, BLACK_BACK_RANK = 7;

    for (int square = 0; square < SQUARE_COUNT; square++) board.squares[square] = EMPTY;

    for (int file = 0; file < 8; file++) {
        board.squares[make_square(file, WHITE_BACK_RANK)] = BACK_ROW[file];
        board.squares[make_square(file, WHITE_PAWN_RANK)] = PAWN;
        board.squares[make_square(file, BLACK_PAWN_RANK)] = -PAWN;
        board.squares[make_square(file, BLACK_BACK_RANK)] = -BACK_ROW[file];
    }

    board.side_to_move = WHITE;
    board.castling_rights = ALL_CASTLING;
    board.en_passant_square = NO_SQUARE;
    board.halfmove_clock = 0;
    board.move_number = 1;
    board.king_square[WHITE] = SQUARE_E1;
    board.king_square[BLACK] = SQUARE_E8;
    board.position_key = calculate_position_key(board);
}

// legal moves
struct Direction { int file_step, rank_step; };
static const Direction KNIGHT_JUMPS[8] = { {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2} };
static const Direction KING_STEPS[8]   = { {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
static const Direction DIAGONALS[4]    = { {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };
static const Direction STRAIGHTS[4]    = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };


// Is the piece wanted one of the given steps away from the square?
static bool piece_one_step_away(const ChessBoard& board, int square, const Direction* steps, int step_count, int wanted) {
    int file = square_file(square), rank = square_rank(square);
    for (int i = 0; i < step_count; i++) {
        int f = file + steps[i].file_step;
        int r = rank + steps[i].rank_step;
        if (is_on_board(f, r) && board.squares[make_square(f, r)] == wanted) return true;
    }
    return false;
}

//Is the first piece met along one of the four directions wanted1 or"wanted2?
static bool piece_along_lines(const ChessBoard& board, int square, const Direction* directions, int wanted1, int wanted2) {
    int file = square_file(square), rank = square_rank(square);
    for (int i = 0; i < 4; i++) {
        int f = file + directions[i].file_step;
        int r = rank + directions[i].rank_step;
        while (is_on_board(f, r)) {
            int piece = board.squares[make_square(f, r)];
            if (piece != EMPTY) {
                if (piece == wanted1 || piece == wanted2) return true;
                //something is in the way
                break;
            }
            f += directions[i].file_step;
            r += directions[i].rank_step;
        }
    }
    return false;
}

bool is_square_attacked(const ChessBoard& board, int square, int by_color) {
    int sign = color_sign(by_color);
    int file = square_file(square), rank = square_rank(square);

    //an attacking pawn stands one rank back on the file to the left or right
    int pawn_rank = rank - sign;
    for (int side = -1; side <= 1; side += 2) {
        if (is_on_board(file + side, pawn_rank) && board.squares[make_square(file + side, pawn_rank)] == sign * PAWN) return true;
    }
    if (piece_one_step_away(board, square, KNIGHT_JUMPS, 8, sign * KNIGHT)) return true;
    if (piece_one_step_away(board, square, KING_STEPS, 8, sign * KING)) return true;
    if (piece_along_lines(board, square, DIAGONALS, sign * BISHOP, sign * QUEEN)) return true;
    if (piece_along_lines(board, square, STRAIGHTS, sign * ROOK, sign * QUEEN)) return true;
    return false;
}

bool is_in_check(const ChessBoard& board, int color) {
    return is_square_attacked(board, board.king_square[color], other_color(color));
}

int cheapest_attacker(const ChessBoard& board, int square, int by_color) {
    int sign = color_sign(by_color);
    int file = square_file(square), rank = square_rank(square);
    int pawn_rank = rank - sign;
    for (int side = -1; side <= 1; side += 2) {
        if (is_on_board(file + side, pawn_rank) && board.squares[make_square(file + side, pawn_rank)] == sign * PAWN) return PAWN;
    }

    if (piece_one_step_away(board, square, KNIGHT_JUMPS, 8, sign * KNIGHT)) return KNIGHT;
    if (piece_along_lines(board, square, DIAGONALS, sign * BISHOP, sign * BISHOP)) return BISHOP;
    if (piece_along_lines(board, square, STRAIGHTS, sign * ROOK, sign * ROOK)) return ROOK;
    if (piece_along_lines(board, square, DIAGONALS, sign * QUEEN, sign * QUEEN)) return QUEEN;
    if (piece_along_lines(board, square, STRAIGHTS, sign * QUEEN, sign * QUEEN)) return QUEEN;
    if (piece_one_step_away(board, square, KING_STEPS, 8, sign * KING)) return KING;

    return EMPTY;
}

struct MoveList {
    Move* moves;
    int count;
};

static void add_move(MoveList& list, int from, int to, u8 flags, int promotion) {
    Move& move = list.moves[list.count];
    move.from = from;
    move.to = to;
    move.promotion = promotion;
    move.flags = flags;
    list.count++;
}

//pawn promotion is 4 moves
static void add_pawn_move(MoveList& list, int from, int to, u8 flags) {
    int to_rank = square_rank(to);
    if (to_rank == 0 || to_rank == 7) {
        add_move(list, from, to, flags | MOVE_PROMOTION, QUEEN);
        add_move(list, from, to, flags | MOVE_PROMOTION, ROOK);
        add_move(list, from, to, flags | MOVE_PROMOTION, BISHOP);
        add_move(list, from, to, flags | MOVE_PROMOTION, KNIGHT);
    } else {
        add_move(list, from, to, flags, 0);
    }
}

static void add_pawn_moves(const ChessBoard& board, MoveList& list, int square, bool captures_only) {
    int side = board.side_to_move;
    int forward = color_sign(side);
    int file = square_file(square), rank = square_rank(square);
    int next_rank = rank + forward;
    bool promotes = next_rank == 0 || next_rank == 7;
    int start_rank = side == WHITE ? 1 : 6;

    //normal
    if (is_on_board(file, next_rank) && board.squares[make_square(file, next_rank)] == EMPTY) {
        if (!captures_only || promotes) add_pawn_move(list, square, make_square(file, next_rank), 0);
        int two_ahead = make_square(file, rank + 2 * forward);
        if (!captures_only && rank == start_rank && board.squares[two_ahead] == EMPTY) {
            add_move(list, square, two_ahead, MOVE_PAWN_DOUBLE_STEP, 0);
        }
    }

    //captures
    for (int side_step = -1; side_step <= 1; side_step += 2) {
        if (!is_on_board(file + side_step, next_rank)) continue;
        int target = make_square(file + side_step, next_rank);
        int piece = board.squares[target];
        if (piece != EMPTY && piece_color(piece) != side) {
            add_pawn_move(list, square, target, MOVE_CAPTURE);
        } else if (target == board.en_passant_square) {
            add_move(list, square, target, MOVE_CAPTURE | MOVE_EN_PASSANT, 0);
        }
    }
}

// king and knight
static void add_step_moves(const ChessBoard& board, MoveList& list, int square, const Direction* steps, bool captures_only) {
    int file = square_file(square), rank = square_rank(square);
    for (int i = 0; i < 8; i++) {
        int f = file + steps[i].file_step;
        int r = rank + steps[i].rank_step;
        if (!is_on_board(f, r)) continue;
        int target = make_square(f, r);
        int piece = board.squares[target];
        if (piece == EMPTY) {
            if (!captures_only) add_move(list, square, target, 0, 0);
        } else if (piece_color(piece) != board.side_to_move) {
            add_move(list, square, target, MOVE_CAPTURE, 0);
        }
    }
}

//Queen rook bishop
static void add_sliding_moves(const ChessBoard& board, MoveList& list, int square, const Direction* directions, bool captures_only) {
    int file = square_file(square), rank = square_rank(square);
    for (int i = 0; i < 4; i++) {
        int f = file + directions[i].file_step;
        int r = rank + directions[i].rank_step;
        while (is_on_board(f, r)) {
            int target = make_square(f, r);
            int piece = board.squares[target];
            if (piece == EMPTY) {
                if (!captures_only) add_move(list, square, target, 0, 0);
            } else {
                if (piece_color(piece) != board.side_to_move) add_move(list, square, target, MOVE_CAPTURE, 0);
                break;
            }
            f += directions[i].file_step;
            r += directions[i].rank_step;
        }
    }
}

static void add_castling_moves(const ChessBoard& board, MoveList& list) {

    int side = board.side_to_move;
    int enemy = other_color(side);

    int home = side == WHITE ? SQUARE_E1 : SQUARE_E8;
    int my_rook = color_sign(side) * ROOK;

    u8 king_side_right = side == WHITE ? WHITE_KING_SIDE : BLACK_KING_SIDE;
    u8 queen_side_right = side == WHITE ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE;

    if (board.king_square[side] != home || is_square_attacked(board, home, enemy)) return;

    if ((board.castling_rights & king_side_right) &&
        board.squares[home + 1] == EMPTY && board.squares[home + 2] == EMPTY && board.squares[home + 3] == my_rook &&
        !is_square_attacked(board, home + 1, enemy) && !is_square_attacked(board, home + 2, enemy)) {
        add_move(list, home, home + 2, MOVE_CASTLE, 0);
    }

    if ((board.castling_rights & queen_side_right) &&
        board.squares[home - 1] == EMPTY && board.squares[home - 2] == EMPTY && board.squares[home - 3] == EMPTY &&
        board.squares[home - 4] == my_rook &&
        !is_square_attacked(board, home - 1, enemy) && !is_square_attacked(board, home - 2, enemy)) {
        add_move(list, home, home - 2, MOVE_CASTLE, 0);
    }
}

int generate_moves(const ChessBoard& board, Move* moves, bool captures_only) {
    MoveList list = { moves, 0 };
    for (int square = 0; square < SQUARE_COUNT; square++) {
        int piece = board.squares[square];
        if (piece == EMPTY || piece_color(piece) != board.side_to_move) continue;

        int type = piece_type(piece);

        switch (type) {
            case PAWN:
                add_pawn_moves(board, list, square, captures_only);
                break;

            case KNIGHT:
                add_step_moves(board, list, square, KNIGHT_JUMPS, captures_only);
                break;

            case BISHOP:
                add_sliding_moves(board, list, square, DIAGONALS, captures_only);
                break;

            case ROOK:
                add_sliding_moves(board, list, square, STRAIGHTS, captures_only);
                break;

            case QUEEN:
                add_sliding_moves(board, list, square, DIAGONALS, captures_only);
                add_sliding_moves(board, list, square, STRAIGHTS, captures_only);
                break;

            case KING:
                add_step_moves(board, list, square, KING_STEPS, captures_only);
                if (!captures_only) add_castling_moves(board, list);
                break;

            default:
                break;
        }
    }
    return list.count;
}

//prevent pinned moves 
int generate_legal_moves(ChessBoard& board, Move* moves) {

    Move candidates[MAX_MOVES];
    int candidate_count = generate_moves(board, candidates, false);
    int legal_count = 0;

    for (int i = 0; i < candidate_count; i++) {
        int mover = board.side_to_move;
        UndoInfo undo;
        make_move(board, candidates[i], undo);

        if (!is_in_check(board, mover)) {
            moves[legal_count] = candidates[i];
            legal_count++;
        }

        undo_move(board, undo);
    }
    return legal_count;
}


static u8 castling_rights_after(u8 rights, int from, int to) {
    for (int i = 0; i < 2; i++) {
        int square = i == 0 ? from : to;
        
        switch (square) {
            case SQUARE_E1:
                rights &= ~(WHITE_KING_SIDE | WHITE_QUEEN_SIDE);
                break;
            case SQUARE_H1:
                rights &= ~WHITE_KING_SIDE;
                break;
            case SQUARE_A1:
                rights &= ~WHITE_QUEEN_SIDE;
                break;
            case SQUARE_E8:
                rights &= ~(BLACK_KING_SIDE | BLACK_QUEEN_SIDE);
                break;
            case SQUARE_H8:
                rights &= ~BLACK_KING_SIDE;
                break;
            case SQUARE_A8:
                rights &= ~BLACK_QUEEN_SIDE;
                break;
            default:
                break;
        }

    }
    return rights;
}


void make_move(ChessBoard& board, Move move, UndoInfo& undo) {

    undo.move = move;
    undo.castling_rights = board.castling_rights;

    undo.en_passant_square = board.en_passant_square;
    undo.halfmove_clock = board.halfmove_clock;
    undo.position_key = board.position_key;
    undo.captured_piece = board.squares[move.to];

    int piece = board.squares[move.from];
    int sign = piece > 0 ? 1 : -1;
    u64 key = board.position_key;

    if (board.en_passant_square != NO_SQUARE) key ^= key_for_en_passant_file[square_file(board.en_passant_square)];
    key ^= key_for_castling[board.castling_rights];

    //remove a captured piece
    if (move.flags & MOVE_EN_PASSANT) {

        int captured_square = move.to - 8 * sign;
        undo.captured_piece = board.squares[captured_square];

        key ^= piece_key(board.squares[captured_square], captured_square);
        board.squares[captured_square] = EMPTY;

    } else if (undo.captured_piece != EMPTY) {
        key ^= piece_key(undo.captured_piece, move.to);
    }

    //move
    int new_piece = (move.flags & MOVE_PROMOTION) ? sign * move.promotion : piece;
    key ^= piece_key(piece, move.from);
    key ^= piece_key(new_piece, move.to);
    board.squares[move.to] = new_piece;
    board.squares[move.from] = EMPTY;

    //castling
    if (move.flags & MOVE_CASTLE) {
        bool king_side = move.to > move.from;
        int rook_from = king_side ? move.from + 3 : move.from - 4;
        int rook_to = king_side ? move.from + 1 : move.from - 1;
        int rook = board.squares[rook_from];
        key ^= piece_key(rook, rook_from);
        key ^= piece_key(rook, rook_to);
        board.squares[rook_to] = rook;
        board.squares[rook_from] = EMPTY;
    }

    if (piece_type(piece) == KING) board.king_square[piece_color(piece)] = move.to;
    board.castling_rights = castling_rights_after(board.castling_rights, move.from, move.to);

    //enable enpassant
    board.en_passant_square = (move.flags & MOVE_PAWN_DOUBLE_STEP) ? (move.from + move.to) / 2 : NO_SQUARE;
    key ^= key_for_castling[board.castling_rights];
    if (board.en_passant_square != NO_SQUARE) key ^= key_for_en_passant_file[square_file(board.en_passant_square)];

    //fifty move rule
    if (piece_type(piece) == PAWN || undo.captured_piece != EMPTY) {
        board.halfmove_clock = 0;
    } else {
        board.halfmove_clock++;
    }

    if (board.side_to_move == BLACK) board.move_number++;

    board.side_to_move = other_color(board.side_to_move);
    key ^= key_for_black_to_move;
    board.position_key = key;
}

void undo_move(ChessBoard& board, const UndoInfo& undo) {
    Move move = undo.move;
    board.side_to_move = other_color(board.side_to_move);
    if (board.side_to_move == BLACK) board.move_number--;
    int sign = color_sign(board.side_to_move);

    //unpremote pawn
    int piece = board.squares[move.to];
    if (move.flags & MOVE_PROMOTION) piece = sign * PAWN;
    board.squares[move.from] = piece;

    //put a captured piece back
    if (move.flags & MOVE_EN_PASSANT) {
        board.squares[move.to] = EMPTY;
        board.squares[move.to - 8 * sign] = undo.captured_piece;
    } else {
        board.squares[move.to] = undo.captured_piece;
    }

    if (move.flags & MOVE_CASTLE) {
        bool king_side = move.to > move.from;
        int rook_from = king_side ? move.from + 3 : move.from - 4;
        int rook_to = king_side ? move.from + 1 : move.from - 1;
        board.squares[rook_from] = board.squares[rook_to];
        board.squares[rook_to] = EMPTY;
    }

    if (piece_type(piece) == KING) board.king_square[board.side_to_move] = move.from;
    board.castling_rights = undo.castling_rights;
    board.en_passant_square = undo.en_passant_square;
    board.halfmove_clock = undo.halfmove_clock;
    board.position_key = undo.position_key;
}

//A move that doesnt happen on board 
void make_null_move(ChessBoard& board, UndoInfo& undo) {
    undo.move = no_move();
    undo.captured_piece = EMPTY;
    undo.castling_rights = board.castling_rights;
    undo.en_passant_square = board.en_passant_square;
    undo.halfmove_clock = board.halfmove_clock;
    undo.position_key = board.position_key;

    if (board.en_passant_square != NO_SQUARE) board.position_key ^= key_for_en_passant_file[square_file(board.en_passant_square)];
    board.en_passant_square = NO_SQUARE;
    board.side_to_move = other_color(board.side_to_move);
    board.position_key ^= key_for_black_to_move;
}

void undo_null_move(ChessBoard& board, const UndoInfo& undo) {
    board.side_to_move = other_color(board.side_to_move);
    board.en_passant_square = undo.en_passant_square;
    board.position_key = undo.position_key;
}

static const char* PIECE_LETTERS = " PNBRQK";

void move_to_notation(ChessBoard& board, Move move, char* out) {
    Text text;
    int type = piece_type(board.squares[move.from]);
    char target[3];
    square_to_text(move.to, target);

    if (move.flags & MOVE_CASTLE) {
        text.add(move.to > move.from ? "O-O" : "O-O-O");
    } else {
        if (type == PAWN) {
            if (move.flags & MOVE_CAPTURE) {
                text.add_char('a' + square_file(move.from));
                text.add_char('x');
            }

        } else {

            text.add_char(PIECE_LETTERS[type]);
            Move moves[MAX_MOVES];

            int count = generate_legal_moves(board, moves);
            bool another = false, same_file = false, same_rank = false;

            for (int i = 0; i < count; i++) {

                if (moves[i].to != move.to || moves[i].from == move.from) continue;
                if (piece_type(board.squares[moves[i].from]) != type) continue;

                another = true;

                if (square_file(moves[i].from) == square_file(move.from)) same_file = true;
                if (square_rank(moves[i].from) == square_rank(move.from)) same_rank = true;
            }
            if (another) {

                if (!same_file) {
                    text.add_char('a' + square_file(move.from));
                } else if (!same_rank) {
                    text.add_char('1' + square_rank(move.from));
                } else {
                    text.add_char('a' + square_file(move.from));
                    text.add_char('1' + square_rank(move.from));
                }
            }
            if (move.flags & MOVE_CAPTURE) text.add_char('x');
        }

        text.add(target);
        if (move.flags & MOVE_PROMOTION) {
            text.add_char('=');
            text.add_char(PIECE_LETTERS[move.promotion]);
        }
    }

    // + check # mate
    UndoInfo undo;
    make_move(board, move, undo);
    if (is_in_check(board, board.side_to_move)) {
        Move replies[MAX_MOVES];
        text.add_char(generate_legal_moves(board, replies) > 0 ? '+' : '#');
    }
    undo_move(board, undo);
    copy_text(out, text.text, 10);
}
