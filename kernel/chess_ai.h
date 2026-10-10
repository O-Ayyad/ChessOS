#pragma once
#include "types.h"
#include "chess_rules.h"

const int CENTIPAWNS_PER_PAWN = 100;
const int MATE_SCORE = 32767;         // a checkmate is worth more than any amount of material
const int MATE_THRESHOLD = MATE_SCORE - 500;   // scores above this mean a forced mate was found

struct AiLevel {
    int max_depth;            //how many moves ahead it looks
    int deeper_percent;       //chance it looks one move further this time
    int think_milliseconds;   //most time it may think (the level tables use max_positions instead)

    // most positions it may look at per move, 0 = no limit
    int max_positions;
    int random_noise;         //centipawns of fuzziness in its judgement

    //chance it misses the opponent's threats this move
    int oversight_percent;
    //chance of a clearly worse move
    int blunder_percent;
    int blunder_max;          // how much worse a blunder may be, in centipawns
    int random_move_percent;  // chance of playing any legal move
};
AiLevel ai_level_for_elo(int elo, bool max_strength);

struct SearchResult {
    Move best_move;
    bool found_move;
    int score;
    int depth;
    int mate_in_plies;
};

// called often while the computer thinks to stop os from locking while ai thinks.
void ai_set_background_work(void (*function)());
void ai_new_game();


SearchResult ai_search(ChessBoard& board, int max_depth, int milliseconds, const u64* earlier_keys, int earlier_count, u64 max_positions = 0);
Move ai_choose_move(ChessBoard& board, const AiLevel& level, const u64* earlier_keys, int earlier_count);
