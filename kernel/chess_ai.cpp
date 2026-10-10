#include "chess_ai.h"
#include "memory.h"
#include "timer.h"
#include "math_utils.h"
#include "text_utils.h"

// How much each piece is worth for capture_order_score
static const int PIECE_VALUE[7] = { 0, 100, 320, 330, 500, 900, 20000 };

// The evaluation uses PeSTO numbers
// chessprogramming.org/PeSTO's_Evaluation_Function

                                        //pawn knight bishop rook queen king
static const int MIDDLEGAME_PIECE_VALUE[7] = { 0,   82,  337,  365,  477, 1025,  0 };
static const int ENDGAME_PIECE_VALUE[7]    = { 0,   94,  281,  297,  512,  936,  0 };

static const int MIDDLEGAME_SQUARE_BONUS[7][64] = {
    { 0 },
    // pawn
    {
         0,   0,   0,   0,   0,   0,   0,   0,
        98, 134,  61,  95,  68, 126,  34, -11,
        -6,   7,  26,  31,  65,  56,  25, -20,
       -14,  13,   6,  21,  23,  12,  17, -23,
       -27,  -2,  -5,  12,  17,   6,  10, -25,
       -26,  -4,  -4, -10,   3,   3,  33, -12,
       -35,  -1, -20, -23, -15,  24,  38, -22,
         0,   0,   0,   0,   0,   0,   0,   0 },
    // knight
    {
      -167, -89, -34, -49,  61, -97, -15,-107,
       -73, -41,  72,  36,  23,  62,   7, -17,
       -47,  60,  37,  65,  84, 129,  73,  44,
        -9,  17,  19,  53,  37,  69,  18,  22,
       -13,   4,  16,  13,  28,  19,  21,  -8,
       -23,  -9,  12,  10,  19,  17,  25, -16,
       -29, -53, -12,  -3,  -1,  18, -14, -19,
      -105, -21, -58, -33, -17, -28, -19, -23 },
    // bishop
    {
       -29,   4, -82, -37, -25, -42,   7,  -8,
       -26,  16, -18, -13,  30,  59,  18, -47,
       -16,  37,  43,  40,  35,  50,  37,  -2,
        -4,   5,  19,  50,  37,  37,   7,  -2,
        -6,  13,  13,  26,  34,  12,  10,   4,
         0,  15,  15,  15,  14,  27,  18,  10,
         4,  15,  16,   0,   7,  21,  33,   1,
       -33,  -3, -14, -21, -13, -12, -39, -21 },
    // rook
    {
        32,  42,  32,  51,  63,   9,  31,  43,
        27,  32,  58,  62,  80,  67,  26,  44,
        -5,  19,  26,  36,  17,  45,  61,  16,
       -24, -11,   7,  26,  24,  35,  -8, -20,
       -36, -26, -12,  -1,   9,  -7,   6, -23,
       -45, -25, -16, -17,   3,   0,  -5, -33,
       -44, -16, -20,  -9,  -1,  11,  -6, -71,
       -19, -13,   1,  17,  16,   7, -37, -26 },
    // queen
    {
       -28,   0,  29,  12,  59,  44,  43,  45,
       -24, -39,  -5,   1, -16,  57,  28,  54,
       -13, -17,   7,   8,  29,  56,  47,  57,
       -27, -27, -16, -16,  -1,  17,  -2,   1,
        -9, -26,  -9, -10,  -2,  -4,   3,  -3,
       -14,   2, -11,  -2,  -5,   2,  14,   5,
       -35,  -8,  11,   2,   8,  15,  -3,   1,
        -1, -18,  -9,  10, -15, -25, -31, -50 },
    // king
    {
       -65,  23,  16, -15, -56, -34,   2,  13,
        29,  -1, -20,  -7,  -8,  -4, -38, -29,
        -9,  24,   2, -16, -20,   6,  22, -22,
       -17, -20, -12, -27, -30, -25, -14, -36,
       -49,  -1, -27, -39, -46, -44, -33, -51,
       -14, -14, -22, -46, -44, -30, -15, -27,
         1,   7,  -8, -64, -43, -16,   9,   8,
       -15,  36,  12, -54,   8, -28,  24,  14 },
};

static const int ENDGAME_SQUARE_BONUS[7][64] = {
    { 0 },
    // pawn
    {
         0,   0,   0,   0,   0,   0,   0,   0,
       178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
         4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
         0,   0,   0,   0,   0,   0,   0,   0 },
    // knight
    {
       -58, -38, -13, -28, -31, -27, -63, -99,
       -25,  -8, -25,  -2,  -9, -25, -24, -52,
       -24, -20,  10,   9,  -1,  -9, -19, -41,
       -17,   3,  22,  22,  22,  11,   8, -18,
       -18,  -6,  16,  25,  16,  17,   4, -18,
       -23,  -3,  -1,  15,  10,  -3, -20, -22,
       -42, -20, -10,  -5,  -2, -20, -23, -44,
       -29, -51, -23, -15, -22, -18, -50, -64 },
    // bishop
    {
       -14, -21, -11,  -8,  -7,  -9, -17, -24,
        -8,  -4,   7, -12,  -3, -13,  -4, -14,
         2,  -8,   0,  -1,  -2,   6,   0,   4,
        -3,   9,  12,   9,  14,  10,   3,   2,
        -6,   3,  13,  19,   7,  10,  -3,  -9,
       -12,  -3,   8,  10,  13,   3,  -7, -15,
       -14, -18,  -7,  -1,   4,  -9, -15, -27,
       -23,  -9, -23,  -5,  -9, -16,  -5, -17 },
    // rook
    {
        13,  10,  18,  15,  12,  12,   8,   5,
        11,  13,  13,  11,  -3,   3,   8,   3,
         7,   7,   7,   5,   4,  -3,  -5,  -3,
         4,   3,  13,   1,   2,   1,  -1,   2,
         3,   5,   8,   4,  -5,  -6,  -8, -11,
        -4,   0,  -5,  -1,  -7, -12,  -8, -16,
        -6,  -6,   0,   2,  -9,  -9, -11,  -3,
        -9,   2,   3,  -1,  -5, -13,   4, -20 },
    // queen
    {
        -9,  22,  22,  27,  27,  19,  10,  20,
       -17,  20,  32,  41,  58,  25,  30,   0,
       -20,   6,   9,  49,  47,  35,  19,   9,
         3,  22,  24,  45,  57,  40,  57,  36,
       -18,  28,  19,  47,  31,  34,  39,  23,
       -16, -27,  15,   6,   9,  17,  10,   5,
       -22, -23, -30, -16, -16, -23, -36, -32,
       -33, -28, -22, -43,  -5, -32, -20, -41 },
    // king
    {
       -74, -35, -18, -18, -11,  15,   4, -17,
       -12,  17,  14,  17,  17,  38,  23,  11,
        10,  17,  23,  15,  20,  45,  44,  13,
        -8,  22,  24,  27,  26,  33,  26,   3,
       -18,  -4,  21,  24,  27,  23,   9, -11,
       -19,  -3,  11,  21,  23,  16,   7,  -9,
       -27, -11,   4,  13,  14,   4,  -5, -17,
       -53, -34, -21, -11, -28, -14, -24, -43 },
};

//passed pawn
static const int PASSED_PAWN_BONUS[8] = { 0, 5, 10, 20, 35, 60, 100, 0 };
const int DOUBLED_PAWN_PENALTY_MIDDLEGAME = 10, DOUBLED_PAWN_PENALTY_ENDGAME = 15;
const int ISOLATED_PAWN_PENALTY_MIDDLEGAME = 10, ISOLATED_PAWN_PENALTY_ENDGAME = 12;
const int ROOK_OPEN_FILE_BONUS = 20;          // no pawns at all on the rook's file
const int ROOK_HALF_OPEN_FILE_BONUS = 10;     // only enemy pawns on the file
const int BISHOP_PAIR_BONUS_MIDDLEGAME = 30, BISHOP_PAIR_BONUS_ENDGAME = 40;


const int PHASE_AT_START = 24;
static const int PHASE_WEIGHT[7] = { 0, 0, 1, 1, 2, 4, 0 };

static bool is_passed_pawn(const ChessBoard& board, int square, int color) {
    int forward = color_sign(color);
    int enemy_pawn = -color_sign(color) * PAWN;
    int file = square_file(square);
    for (int f = file - 1; f <= file + 1; f++) {
        if (f < 0 || f > 7) continue;
        for (int rank = square_rank(square) + forward; rank >= 0 && rank < 8; rank += forward) {
            if (board.squares[make_square(f, rank)] == enemy_pawn) return false;
        }
    }
    return true;
}


static int evaluate(const ChessBoard& board) {
    int pawns_on_file[2][8];
    memset(pawns_on_file, 0, sizeof(pawns_on_file));
    int bishop_count[2] = { 0, 0 };
    int phase = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {
        int piece = board.squares[square];
        if (piece == EMPTY) continue;
        int type = piece_type(piece);
        if (type == PAWN) pawns_on_file[piece_color(piece)][square_file(square)]++;
        if (type == BISHOP) bishop_count[piece_color(piece)]++;
        phase += PHASE_WEIGHT[type];
    }
    if (phase > PHASE_AT_START) phase = PHASE_AT_START;

    //one for the middlegame and one for the endgame and blend them by the game phase at the end.
    int middlegame = 0;
    int endgame = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {

        int piece = board.squares[square];

        if (piece == EMPTY) continue;

        int type = piece_type(piece);
        int color = piece_color(piece);
        int file = square_file(square);

        int table_index = color == WHITE ? make_square(file, 7 - square_rank(square)) : square;

        int mid = MIDDLEGAME_PIECE_VALUE[type] + MIDDLEGAME_SQUARE_BONUS[type][table_index];
        int end = ENDGAME_PIECE_VALUE[type] + ENDGAME_SQUARE_BONUS[type][table_index];

        if (type == PAWN) {

            if (pawns_on_file[color][file] > 1) { 
                mid -= DOUBLED_PAWN_PENALTY_MIDDLEGAME;
                end -= DOUBLED_PAWN_PENALTY_ENDGAME;
            }

            bool no_left_friend = file == 0 || pawns_on_file[color][file - 1] == 0;
            bool no_right_friend = file == 7 || pawns_on_file[color][file + 1] == 0;

            if (no_left_friend && no_right_friend) { 
                mid -= ISOLATED_PAWN_PENALTY_MIDDLEGAME;
                end -= ISOLATED_PAWN_PENALTY_ENDGAME;
            }

            if (is_passed_pawn(board, square, color)) {
                int ranks_advanced = color == WHITE ? square_rank(square) : 7 - square_rank(square);
                mid += PASSED_PAWN_BONUS[ranks_advanced] / 2;
                end += PASSED_PAWN_BONUS[ranks_advanced];
            }
        }
        if (type == ROOK && pawns_on_file[color][file] == 0) {
            bool enemy_pawns_on_file = pawns_on_file[other_color(color)][file] > 0;
            mid += enemy_pawns_on_file ? ROOK_HALF_OPEN_FILE_BONUS : ROOK_OPEN_FILE_BONUS;
        }

        if (color == WHITE) {
            middlegame += mid;
            endgame += end;
        } else {
            middlegame -= mid;
            endgame -= end;
        }
    }

    if (bishop_count[WHITE] >= 2) { 
        middlegame += BISHOP_PAIR_BONUS_MIDDLEGAME; endgame += BISHOP_PAIR_BONUS_ENDGAME; 
    }

    if (bishop_count[BLACK] >= 2) { 
        middlegame -= BISHOP_PAIR_BONUS_MIDDLEGAME; endgame -= BISHOP_PAIR_BONUS_ENDGAME; 
    }

    int score = (middlegame * phase + endgame * (PHASE_AT_START - phase)) / PHASE_AT_START;
    return board.side_to_move == WHITE ? score : -score;
}

enum ScoreKind { SCORE_UNKNOWN = 0, SCORE_EXACT, SCORE_AT_LEAST, SCORE_AT_MOST };

struct RememberedPosition {
    u64 key;
    i16 score;
    u8 depth;
    u8 kind;
    Move best_move;
};

const int TABLE_SIZE = 1 << 18; 
static RememberedPosition* table = nullptr;

static RememberedPosition& table_entry(u64 key) {
    return table[key % TABLE_SIZE];
}


static int score_to_table(int score, int ply) {
    if (score > MATE_THRESHOLD) return score + ply;
    if (score < -MATE_THRESHOLD) return score - ply;
    return score;
}

static int score_from_table(int score, int ply) {
    if (score > MATE_THRESHOLD) return score - ply;
    if (score < -MATE_THRESHOLD) return score + ply;
    return score;
}

const int CHECK_TIME_EVERY = 2048;
const int MAX_PLY = 128;
const int MAX_REMEMBERED_KEYS = 1400;

static void (*background_work)() = nullptr;
static u64 positions_searched = 0;
static u64 stop_time = 0;
static u64 position_limit = 0;      // 0 = no limit
static bool search_must_stop = false;    // out of time, or looked at enough positions


static u64 key_history[MAX_REMEMBERED_KEYS];
static int key_history_count = 0;

static Move killer_moves[MAX_PLY][2];

static int move_history[2][SQUARE_COUNT][SQUARE_COUNT];

void ai_set_background_work(void (*function)()) {
    background_work = function;
}

void ai_new_game() {
    if (table == nullptr) table = allocate_array<RememberedPosition>(TABLE_SIZE);
    memset(table, 0, sizeof(RememberedPosition) * TABLE_SIZE);
    memset(move_history, 0, sizeof(move_history));
}

static void count_position_and_check_time() {
    positions_searched++;
    if (position_limit != 0 && positions_searched >= position_limit) search_must_stop = true;
    if (positions_searched % CHECK_TIME_EVERY == 0) {
        if (background_work != nullptr) background_work();
        if (milliseconds_since_start() >= stop_time) search_must_stop = true;
    }
}

static void push_key(u64 key) {
    if (key_history_count < MAX_REMEMBERED_KEYS) {
        key_history[key_history_count] = key;
        key_history_count++;
    }
}

static void pop_key() {
    key_history_count--;
}

static bool is_repetition(const ChessBoard& board) {

    // Only positions with the same side to move since the
    // last capture or pawn move can repeat.
    int oldest = key_history_count - 1 - board.halfmove_clock;
    for (int i = key_history_count - 3; i >= 0 && i >= oldest; i -= 2) {
        if (key_history[i] == board.position_key) return true;
    }
    return false;
}

static bool has_pieces_besides_pawns(const ChessBoard& board, int color) {
    for (int square = 0; square < SQUARE_COUNT; square++) {
        int piece = board.squares[square];
        if (piece == EMPTY || piece_color(piece) != color) continue;
        if (piece_type(piece) != PAWN && piece_type(piece) != KING) return true;
    }
    return false;
}

//Alpha beta
const int ORDER_TABLE_MOVE = 1000000;  //the best move from the table try first
const int ORDER_CAPTURE = 100000;      //captures most valuable victim first
const int ORDER_PROMOTION = 95000;
const int ORDER_KILLER_1 = 90000;
const int ORDER_KILLER_2 = 89000;     //killers then everything else by history

static int capture_order_score(const ChessBoard& board, Move move) {
    int victim = piece_type(board.squares[move.to]);
    if (victim == EMPTY) victim = PAWN;                 // en passant
    int attacker = piece_type(board.squares[move.from]);

    return PIECE_VALUE[victim] * 10 - attacker;
}

static void bring_best_move_forward(Move* moves, int* scores, int count, int index) {
    int best = index;
    for (int i = index + 1; i < count; i++) {
        if (scores[i] > scores[best]) best = i;
    }
    Move move = moves[index]; moves[index] = moves[best]; moves[best] = move;
    int score = scores[index]; scores[index] = scores[best]; scores[best] = score;
}

const int DELTA_MARGIN = 200;

static int quiescence_search(ChessBoard& board, int alpha, int beta, int ply) {
    count_position_and_check_time();
    if (search_must_stop) return 0;

    int standing_score = evaluate(board);

    if (standing_score >= beta) return beta;
    if (standing_score > alpha) alpha = standing_score;
    if (ply > MAX_PLY / 2) return alpha;

    Move moves[MAX_MOVES];

    int scores[MAX_MOVES];
    int count = generate_moves(board, moves, true);

    for (int i = 0; i < count; i++) {
        scores[i] = capture_order_score(board, moves[i]) + (moves[i].promotion ? ORDER_PROMOTION : 0);
    }

    for (int i = 0; i < count; i++) {

        bring_best_move_forward(moves, scores, count, i);
        Move move = moves[i];
        int gain = PIECE_VALUE[piece_type(board.squares[move.to])];
        if (!move.promotion && !(move.flags & MOVE_EN_PASSANT) && standing_score + gain + DELTA_MARGIN < alpha) continue;

        int mover = board.side_to_move;
        UndoInfo undo;
        make_move(board, move, undo);
        if (is_in_check(board, mover)) {
            undo_move(board, undo);
            continue;
        }
        int score = -quiescence_search(board, -beta, -alpha, ply + 1);
        undo_move(board, undo);
        if (search_must_stop) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}


const int NULL_MOVE_REDUCTION = 3;
const int NULL_MOVE_MIN_DEPTH = 3;
const int LATE_MOVE_MIN_DEPTH = 3;
const int LATE_MOVE_AFTER = 4;
const int LATE_MOVE_EXTRA_AFTER = 12;
const int HISTORY_LIMIT = 60000;

static int search(ChessBoard& board, int depth, int alpha, int beta, int ply, bool allow_null_move) {
    if (ply > 0) {
        if (board.halfmove_clock >= FIFTY_MOVE_RULE_HALFMOVES) return 0; // 50-move rule
        if (is_repetition(board)) return 0; // repeated position
    }
    bool in_check = is_in_check(board, board.side_to_move);
    if (in_check) depth++;  // look one move further when in check
    if (depth <= 0) return quiescence_search(board, alpha, beta, ply);
    count_position_and_check_time();
    if (search_must_stop) return 0;
    if (ply >= MAX_PLY - 8) return evaluate(board);

    //Did we search this position before?
    RememberedPosition& entry = table_entry(board.position_key);
    Move table_move = no_move();
    if (entry.key == board.position_key) {
        table_move = entry.best_move;
        if (entry.depth >= depth && ply > 0) {
            int score = score_from_table(entry.score, ply);
            if (entry.kind == SCORE_EXACT) return score;
            if (entry.kind == SCORE_AT_LEAST && score >= beta) return score;
            if (entry.kind == SCORE_AT_MOST && score <= alpha) return score;
        }
    }

    if (allow_null_move && !in_check && depth >= NULL_MOVE_MIN_DEPTH && ply > 0 && beta < MATE_THRESHOLD &&
        has_pieces_besides_pawns(board, board.side_to_move) && evaluate(board) >= beta) {
        UndoInfo undo;
        make_null_move(board, undo);
        push_key(board.position_key);
        int score = -search(board, depth - NULL_MOVE_REDUCTION, -beta, -beta + 1, ply + 1, false);
        pop_key();
        undo_null_move(board, undo);
        if (search_must_stop) return 0;
        if (score >= beta) return beta;
    }

    Move moves[MAX_MOVES];
    int scores[MAX_MOVES];
    int count = generate_moves(board, moves, false);
    for (int i = 0; i < count; i++) {
        Move move = moves[i];
        if (same_move(move, table_move))           scores[i] = ORDER_TABLE_MOVE;
        else if (move.flags & MOVE_CAPTURE)        scores[i] = ORDER_CAPTURE + capture_order_score(board, move);
        else if (move.promotion)                   scores[i] = ORDER_PROMOTION;
        else if (same_move(move, killer_moves[ply][0])) scores[i] = ORDER_KILLER_1;
        else if (same_move(move, killer_moves[ply][1])) scores[i] = ORDER_KILLER_2;
        else                                       scores[i] = move_history[board.side_to_move][move.from][move.to];
    }

    int legal_moves = 0;
    int best_score = -2 * MATE_SCORE;
    Move best_move = no_move();
    int original_alpha = alpha;
    for (int i = 0; i < count; i++) {
        bring_best_move_forward(moves, scores, count, i);
        Move move = moves[i];
        int mover = board.side_to_move;
        UndoInfo undo;
        make_move(board, move, undo);
        if (is_in_check(board, mover)) {
            undo_move(board, undo);
            continue;
        }
        legal_moves++;
        push_key(board.position_key);
        bool quiet = !(move.flags & MOVE_CAPTURE) && !move.promotion;

        int score;
        if (legal_moves == 1) {
            score = -search(board, depth - 1, -beta, -alpha, ply + 1, true);
        } else {

            int reduction = 0;
            if (depth >= LATE_MOVE_MIN_DEPTH && legal_moves > LATE_MOVE_AFTER && quiet && !in_check &&
                !is_in_check(board, board.side_to_move)) {
                reduction = legal_moves > LATE_MOVE_EXTRA_AFTER ? 2 : 1;
            }
            score = -search(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);
            if (score > alpha && (reduction > 0 || score < beta)) {
                score = -search(board, depth - 1, -beta, -alpha, ply + 1, true);
            }
        }
        pop_key();
        undo_move(board, undo);
        if (search_must_stop) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {

            if (quiet) {
                if (!same_move(killer_moves[ply][0], move)) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }
                move_history[mover][move.from][move.to] += depth * depth;
                if (move_history[mover][move.from][move.to] > HISTORY_LIMIT) {
                    for (int from = 0; from < SQUARE_COUNT; from++) {
                        for (int to = 0; to < SQUARE_COUNT; to++) move_history[mover][from][to] /= 2;
                    }
                }
            }
            break;
        }
    }

    if (legal_moves == 0) {
        return in_check ? -MATE_SCORE + ply : 0;
    }

    entry.key = board.position_key;
    entry.depth = min_int(depth, 255); 
    entry.best_move = best_move;
    entry.score = score_to_table(best_score, ply);
    if (best_score >= beta)              entry.kind = SCORE_AT_LEAST;
    else if (best_score > original_alpha) entry.kind = SCORE_EXACT;
    else                                  entry.kind = SCORE_AT_MOST;
    return best_score;
}

static void load_game_history(const ChessBoard& board, const u64* earlier_keys, int earlier_count) {
    key_history_count = 0;
    int first = max_int(0, earlier_count - (MAX_REMEMBERED_KEYS - MAX_PLY - 8));
    for (int i = first; i < earlier_count; i++) push_key(earlier_keys[i]);
    if (key_history_count == 0 || key_history[key_history_count - 1] != board.position_key) {
        push_key(board.position_key);
    }
}

SearchResult ai_search(ChessBoard& board, int max_depth, int milliseconds, const u64* earlier_keys, int earlier_count,
                       u64 max_positions) {
    if (table == nullptr) ai_new_game();
    SearchResult result;
    result.found_move = false;
    result.score = 0;
    result.depth = 0;
    result.mate_in_plies = 0;
    result.best_move = no_move();

    Move legal[MAX_MOVES];
    int legal_count = generate_legal_moves(board, legal);
    if (legal_count == 0) {
        result.score = is_in_check(board, board.side_to_move) ? -MATE_SCORE : 0;
        return result;
    }
    result.best_move = legal[0];
    result.found_move = true;

    positions_searched = 0;
    position_limit = max_positions;
    search_must_stop = false;
    stop_time = milliseconds_since_start() + milliseconds;
    memset(killer_moves, 0, sizeof(killer_moves));

    for (int depth = 1; depth <= max_depth; depth++) {
        load_game_history(board, earlier_keys, earlier_count);

        // try the best move of the previous depth first
        for (int i = 0; i < legal_count; i++) {
            if (same_move(legal[i], result.best_move)) {
                Move first = legal[0]; legal[0] = legal[i]; legal[i] = first;
                break;
            }
        }
        int alpha = -2 * MATE_SCORE;
        int beta = 2 * MATE_SCORE;
        int best_score = -2 * MATE_SCORE;
        Move best_move = legal[0];
        for (int i = 0; i < legal_count; i++) {
            UndoInfo undo;
            make_move(board, legal[i], undo);
            push_key(board.position_key);
            int score;
            if (i == 0) {
                score = -search(board, depth - 1, -beta, -alpha, 1, true);
            } else {
                score = -search(board, depth - 1, -alpha - 1, -alpha, 1, true);
                if (score > alpha && !search_must_stop) score = -search(board, depth - 1, -beta, -alpha, 1, true);
            }

            pop_key();
            undo_move(board, undo);
            
            if (search_must_stop) break;
            if (score > best_score) {
                best_score = score;
                best_move = legal[i];
            }
            if (score > alpha) alpha = score;
        }
        if (search_must_stop && depth > 1) break;     // an unfinished search can't be trusted

        result.best_move = best_move;
        result.score = best_score;
        result.depth = depth;
        if (best_score > MATE_THRESHOLD) {
            result.mate_in_plies = MATE_SCORE - best_score;
            if (result.mate_in_plies <= depth) break;     // found the fastest mate: no need to look further
        } else if (best_score < -MATE_THRESHOLD) {
            result.mate_in_plies = -(MATE_SCORE + best_score);
        } else {
            result.mate_in_plies = 0;
        }
        if (milliseconds_since_start() >= stop_time) break;
    }
    return result;
}

//   depth      how many moves ahead it looks
//   positions  how many positions it may look at for one move (the same on fast and slow computers)
//   noise      fuzzy judgement
//   oversight  the chance it only looks at its own move and the captures after it, missing threats
//   blunder    the chance of a real mistake
//   random     the chance of playing any legal move at all.

struct LevelPoint {
    int elo;
    AiLevel level;
};

const int SEARCH_GUARD = 20000000;             // the weak levels stop on depth this many positions is only a guard
const int LEVEL_SAFETY_MILLISECONDS = 20000;   // the levels stop on positions this only guards very slow computers

static const LevelPoint LEVEL_TABLE[] = {
    // elo     depth deeper%  time     positions  noise oversight% blunder% blunder_max random%
    {  200,  {   1,      0,     0,        SEARCH_GUARD,         317,        0,           78,        2067,       0 } },
    {  300,  {   1,      0,     0,        SEARCH_GUARD,         300,        0,           68,        1762,       0 } },
    {  400,  {   1,      0,     0,        SEARCH_GUARD,         297,        0,           58,        1449,       0 } },
    {  500,  {   1,      0,     0,        SEARCH_GUARD,         285,        0,           49,        1268,       0 } },
    {  600,  {   1,      0,     0,        SEARCH_GUARD,         268,        0,           41,        1116,       0 } },
    {  700,  {   1,      0,     0,        SEARCH_GUARD,         251,        0,           36,        1005,       0 } },
    {  800,  {   1,      0,     0,        SEARCH_GUARD,         234,        0,           27,         919,       0 } },
    {  900,  {   1,      0,     0,        SEARCH_GUARD,         216,        0,           20,         827,       0 } },
    { 1000,  {   1,      0,     0,        SEARCH_GUARD,         197,        0,           14,         734,       0 } },
    { 1100,  {   1,     28,     0,        SEARCH_GUARD,         166,       32,            9,         640,       0 } },
    { 1200,  {   1,     67,     0,        SEARCH_GUARD,         146,       40,            7,         586,       0 } },
    { 1300,  {   2,      6,     0,        SEARCH_GUARD,         114,       38,            6,         473,       0 } },
    { 1400,  {   2,     23,     0,        SEARCH_GUARD,         100,       33,            5,         421,       0 } },
    { 1500,  {   2,     51,     0,        SEARCH_GUARD,          83,       25,            4,         365,       0 } },
    { 1600,  {   2,     81,     0,        SEARCH_GUARD,          69,       19,            3,         326,       0 } },
    { 1700,  {   2,     97,     0,        SEARCH_GUARD,          62,       16,            3,         304,       0 } },
    { 1800,  {   3,     22,     0,        SEARCH_GUARD,          54,       14,            2,         278,       0 } },
    { 1900,  {   3,     56,     0,        SEARCH_GUARD,          44,       12,            2,         244,       0 } },
    { 2000,  {  64,      0,     0,                2700,           0,        0,            0,           0,       0 } },
    { 2100,  {  64,      0,     0,                3900,           0,        0,            0,           0,       0 } },
    { 2200,  {  64,      0,     0,                5600,           0,        0,            0,           0,       0 } },
    { 2300,  {  64,      0,     0,                8500,           0,        0,            0,           0,       0 } },
    { 2400,  {  64,      0,     0,               16900,           0,        0,            0,           0,       0 } },
    { 2500,  {  64,      0,     0,               29300,           0,        0,            0,           0,       0 } },
    { 2600,  {  64,      0,     0,               74600,           0,        0,            0,           0,       0 } },
    { 2700,  {  64,      0,     0,              316000,           0,        0,            0,           0,       0 } },
    { 2750,  {  64,      0,     0,              900000,           0,        0,            0,           0,       0 } },
};
const int LEVEL_COUNT = sizeof(LEVEL_TABLE) / sizeof(LEVEL_TABLE[0]);

const AiLevel MAX_STRENGTH_LEVEL = { 80, 0, 7500, 0, 0, 0, 0, 0, 0 };
const int MAX_STRENGTH_ELO = 2750;

static int blend(int a, int b, int part, int whole) {
    return a + (int)((i64)(b - a) * part / whole);
}

AiLevel ai_level_for_elo(int elo, bool max_strength) {
    if (max_strength || elo >= MAX_STRENGTH_ELO) return MAX_STRENGTH_LEVEL;
    if (elo <= LEVEL_TABLE[0].elo) return LEVEL_TABLE[0].level;
    if (elo >= LEVEL_TABLE[LEVEL_COUNT - 1].elo) return LEVEL_TABLE[LEVEL_COUNT - 1].level;

    int above = 1;
    while (LEVEL_TABLE[above].elo < elo) above++;
    const AiLevel& low = LEVEL_TABLE[above - 1].level;
    const AiLevel& high = LEVEL_TABLE[above].level;
    int part = elo - LEVEL_TABLE[above - 1].elo;
    int whole = LEVEL_TABLE[above].elo - LEVEL_TABLE[above - 1].elo;

    // the depth is blended through deeper_percent: 3 + 50% = "3, sometimes 4"
    int low_depth_percent = low.max_depth * 100 + low.deeper_percent;
    int high_depth_percent = high.max_depth * 100 + high.deeper_percent;
    int depth_percent = blend(low_depth_percent, high_depth_percent, part, whole);

    AiLevel level;
    level.max_depth = depth_percent / 100;
    level.deeper_percent = depth_percent % 100;
    level.think_milliseconds = blend(low.think_milliseconds, high.think_milliseconds, part, whole);
    level.max_positions = blend(low.max_positions, high.max_positions, part, whole);
    level.random_noise = blend(low.random_noise, high.random_noise, part, whole);
    level.oversight_percent = blend(low.oversight_percent, high.oversight_percent, part, whole);
    level.blunder_percent = blend(low.blunder_percent, high.blunder_percent, part, whole);
    level.blunder_max = blend(low.blunder_max, high.blunder_max, part, whole);
    level.random_move_percent = blend(low.random_move_percent, high.random_move_percent, part, whole);
    return level;
}

const int BLUNDER_AT_LEAST = 80;
const int OVERSIGHT_DEPTH = 1;

// Scores every legal move by searching it depth moves deep.
static void score_every_move(ChessBoard& board, Move* legal, int legal_count, int depth, int* scores,
                             const u64* earlier_keys, int earlier_count) {
    for (int i = 0; i < legal_count; i++) {
        load_game_history(board, earlier_keys, earlier_count);
        UndoInfo undo;
        make_move(board, legal[i], undo);
        push_key(board.position_key);
        scores[i] = -search(board, depth - 1, -2 * MATE_SCORE, 2 * MATE_SCORE, 1, true);
        undo_move(board, undo);
        if (search_must_stop) {
            for (int j = i; j < legal_count; j++) scores[j] = -MATE_SCORE;   // not searched: treat as bad
            return;
        }
    }
}

// A random number that is usually near 0 and rarely near +-size
static int bell_curve_noise(int size) {
    if (size <= 0) return 0;
    int total = 0;

    for (int i = 0; i < 3; i++){
        total += random_below(2 * size + 1) - size;
    } 

    return total/2;
}

// The levels start every move with a clear memory
static void forget_earlier_searches() {

    if (table == nullptr) table = allocate_array<RememberedPosition>(TABLE_SIZE);

    memset(table, 0, sizeof(RememberedPosition) * TABLE_SIZE);
    memset(move_history, 0, sizeof(move_history));
    memset(killer_moves, 0, sizeof(killer_moves));
}

static void start_level_search(const AiLevel& level) {
    positions_searched = 0;
    position_limit = level.max_positions;
    search_must_stop = false;
    stop_time = milliseconds_since_start() + LEVEL_SAFETY_MILLISECONDS;
}

static const char* WHITE_FIRST_MOVES = "e2e4 d2d4 b1c3 g1f3";    // e4, d4, Nc3, Nf3

struct FirstMoveReplies {
    const char* white_move;
    const char* black_replies;
};
static const FirstMoveReplies BLACK_REPLIES[] = {
    { "e2e4", "e7e5 c7c5 e7e6 c7c6" },
    { "d2d4", "d7d5 g8f6 e7e6 f7f5" },
    { "b1c3", "d7d5 e7e5 g8f6 c7c5" },
    { "g1f3", "d7d5 g8f6 c7c5 g7g6" },
};
const int BLACK_REPLY_COUNT = sizeof(BLACK_REPLIES) / sizeof(BLACK_REPLIES[0]);
const int LETTERS_PER_LISTED_MOVE = 5;     // "e2e4" and a space

// The legal move "e2e4" stands for in this position (no_move() if there is none)
static Move find_listed_move(ChessBoard& board, const char* text) {
    int from = text_to_square(text);
    int to = text_to_square(text + 2);
    Move legal[MAX_MOVES];
    int count = generate_legal_moves(board, legal);
    for (int i = 0; i < count; i++) {
        if (legal[i].from == from && legal[i].to == to) return legal[i];
    }
    return no_move();
}

// Random moves from a list
static Move random_listed_move(ChessBoard& board, const char* list) {
    int count = (text_length(list) + 1) / LETTERS_PER_LISTED_MOVE;
    return find_listed_move(board, list + random_below(count) * LETTERS_PER_LISTED_MOVE);
}

static bool opening_book_move(ChessBoard& board, Move& book_move) {
    if (board.move_number != 1) return false;
    ChessBoard start;
    set_starting_position(start);

    if (board.side_to_move == WHITE) {
        if (board.position_key != start.position_key) return false;
        book_move = random_listed_move(board, WHITE_FIRST_MOVES);
        return !same_move(book_move, no_move());
    }

    for (int i = 0; i < BLACK_REPLY_COUNT; i++) {
        ChessBoard after = start;
        Move white_move = find_listed_move(after, BLACK_REPLIES[i].white_move);
        UndoInfo undo;
        make_move(after, white_move, undo);
        if (after.position_key == board.position_key) {
            book_move = random_listed_move(board, BLACK_REPLIES[i].black_replies);
            return !same_move(book_move, no_move());
        }
    }
    return false;
}

Move ai_choose_move(ChessBoard& board, const AiLevel& level, const u64* earlier_keys, int earlier_count) {

    Move book_move;
    if (opening_book_move(board, book_move)) return book_move;

    Move legal[MAX_MOVES];
    int legal_count = generate_legal_moves(board, legal);
    if (legal_count == 0) return no_move();
    if (legal_count == 1) return legal[0];

    // full strength the plain search
    bool full_strength = level.max_positions == 0;
    if (full_strength) {
        return ai_search(board, level.max_depth, level.think_milliseconds, earlier_keys, earlier_count).best_move;
    }

    if (random_below(100) < level.random_move_percent) {
        return legal[random_below(legal_count)];
    }

    forget_earlier_searches();
    int depth = level.max_depth + (random_below(100) < level.deeper_percent ? 1 : 0);
    bool perfect = level.random_noise == 0 && level.oversight_percent == 0 && level.blunder_percent == 0;
    if (perfect) {
        return ai_search(board, depth, LEVEL_SAFETY_MILLISECONDS, earlier_keys, earlier_count, level.max_positions).best_move;
    }

    // Weaker levels give every move its own score
    start_level_search(level);
    int scores[MAX_MOVES];
    score_every_move(board, legal, legal_count, depth, scores, earlier_keys, earlier_count);
    int best = 0;
    for (int i = 1; i < legal_count; i++) {
        if (scores[i] > scores[best]) best = i;
    }
    if (scores[best] > MATE_THRESHOLD) return legal[best];

    if (random_below(100) < level.blunder_percent) {
        int candidates[MAX_MOVES];
        int candidate_count = 0;
        for (int i = 0; i < legal_count; i++) {
            int worse_by = scores[best] - scores[i];
            if (worse_by > BLUNDER_AT_LEAST && worse_by <= level.blunder_max) {
                candidates[candidate_count] = i;
                candidate_count++;
            }
        }
        if (candidate_count > 0) return legal[candidates[random_below(candidate_count)]];
    }

    if (random_below(100) < level.oversight_percent && depth > OVERSIGHT_DEPTH) {
        start_level_search(level);
        score_every_move(board, legal, legal_count, OVERSIGHT_DEPTH, scores, earlier_keys, earlier_count);
    }

    // the best move after adding some noise to every score
    int chosen = 0;
    int chosen_score = -3 * MATE_SCORE;
    for (int i = 0; i < legal_count; i++) {
        int score = scores[i] + bell_curve_noise(level.random_noise);
        if (score > chosen_score) {
            chosen_score = score;
            chosen = i;
        }
    }
    return legal[chosen];
}
