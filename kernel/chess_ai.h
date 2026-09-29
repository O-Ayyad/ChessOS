
#pragma once
#include "types.h"
#include "chess_rules.h"

const int CENTIPAWNS_PER_PAWN = 100;
const int MATE_SCORE = 32767;         //a checkmate is worth more than any amount of material i6 max
const int MATE_THRESHOLD = MATE_SCORE - 500;   //scores above this mean forced mate found

// How strong the computer plays
struct AiLevel {
    int max_depth;
    int think_milliseconds;
    int random_noise;
    int blunder_percent;
    int random_move_percent;
};
AiLevel ai_level_for_elo(int elo, bool max_strength);

struct SearchResult {
    Move best_move;
    bool found_move;
    int score;
    int depth;
    int mate_in_plies;
};


void ai_set_background_work(void (*function)());
void ai_new_game();


SearchResult ai_search(ChessBoard& board, int max_depth, int milliseconds, const u64* earlier_keys, int earlier_count);
Move ai_choose_move(ChessBoard& board, const AiLevel& level, const u64* earlier_keys, int earlier_count);
