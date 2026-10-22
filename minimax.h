/* ===========================================================================
 *  minimax.h  -  AI decision engine: Minimax + Alpha-Beta pruning,
 *  insertion-sort move ordering, iterative deepening, quiescence search.
 * =========================================================================== */
#ifndef MINIMAX_H
#define MINIMAX_H

#include "chess_rules.h"

typedef struct SearchStats {
    long   nodes;
    int    depth;
    int    score;
    double ms;
} SearchStats;

int  evaluate(const GameState *gs);
void order_moves(Move *moves, int n);
int  find_best_move(GameState *gs, int depth, Move *best, SearchStats *stats);

#endif /* MINIMAX_H */