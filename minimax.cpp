/* ===========================================================================
 *  minimax.cpp  -  Minimax with Alpha-Beta pruning.
 *  White is maximizing player, Black is minimizing player.
 * =========================================================================== */
#include "minimax.h"
#include <stdlib.h>
#include <time.h>

#define INF_SCORE   1000000
#define MATE_SCORE  100000
#define QS_DEPTH    6

static const int PST_P[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int PST_N[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

static const int PST_B[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

static const int PST_R[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

static const int PST_Q[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

static const int PST_K_MID[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

static const int PST_K_END[64] = {
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50
};

int evaluate(const GameState *gs)
{
    int score = 0, nonPawn = 0, r, c;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            char p = gs->board[r][c];
            if (p == EMPTY) continue;
            switch (p | 32) {
                case 'n': case 'b': case 'r': case 'q': nonPawn += piece_value(p); break;
                default: break;
            }
        }
    }

    int endgame = nonPawn < 2600;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            char p = gs->board[r][c];
            int white, idx, v;
            if (p == EMPTY) continue;
            white = is_white_piece(p);
            idx = white ? (r * 8 + c) : ((7 - r) * 8 + c);
            v = piece_value(p);
            switch (p | 32) {
                case 'p': v += PST_P[idx]; break;
                case 'n': v += PST_N[idx]; break;
                case 'b': v += PST_B[idx]; break;
                case 'r': v += PST_R[idx]; break;
                case 'q': v += PST_Q[idx]; break;
                case 'k': v += endgame ? PST_K_END[idx] : PST_K_MID[idx]; break;
                default: break;
            }
            score += white ? v : -v;
        }
    }
    return score;
}

static void score_moves(Move *moves, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int s = 0;
        if (moves[i].captured != EMPTY)
            s += 1000 + 10 * piece_value(moves[i].captured) - piece_value(moves[i].piece) / 10;
        if (moves[i].flags & MF_PROMOTION) s += 800;
        if (moves[i].flags & (MF_CASTLE_K | MF_CASTLE_Q)) s += 50;
        moves[i].score = s;
    }
}

void order_moves(Move *moves, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        Move key = moves[i];
        j = i - 1;
        while (j >= 0 && moves[j].score < key.score) {
            moves[j + 1] = moves[j];
            j--;
        }
        moves[j + 1] = key;
    }
}

static int quiesce(GameState *gs, int alpha, int beta, int maximizing, long *nodes, int qd)
{
    Move moves[MAX_MOVES];
    int stand, best, n, i;

    (*nodes)++;
    stand = evaluate(gs);
    if (qd <= 0) return stand;

    if (maximizing) {
        if (stand >= beta) return stand;
        if (stand > alpha) alpha = stand;
        best = stand;
    } else {
        if (stand <= alpha) return stand;
        if (stand < beta) beta = stand;
        best = stand;
    }

    n = generate_legal_moves(gs, moves, MAX_MOVES, 1);
    score_moves(moves, n);
    order_moves(moves, n);

    for (i = 0; i < n; i++) {
        Move m = moves[i];
        int v;
        make_move(gs, &m);
        v = quiesce(gs, alpha, beta, !maximizing, nodes, qd - 1);
        unmake_move(gs, &m);
        if (maximizing) {
            if (v > best) best = v;
            if (best > alpha) alpha = best;
        } else {
            if (v < best) best = v;
            if (best < beta) beta = best;
        }
        if (alpha >= beta) break;
    }
    return best;
}

static int minimax(GameState *gs, int depth, int alpha, int beta, int maximizing, long *nodes)
{
    Move moves[MAX_MOVES];
    int n, i, best;

    (*nodes)++;
    if (gs->halfmove >= 100) return 0;
    if (depth <= 0) return quiesce(gs, alpha, beta, maximizing, nodes, QS_DEPTH);

    n = generate_legal_moves(gs, moves, MAX_MOVES, 0);
    if (n == 0) {
        if (king_in_check(gs, gs->whiteToMove))
            return gs->whiteToMove ? -(MATE_SCORE + depth) : (MATE_SCORE + depth);
        return 0;
    }
    score_moves(moves, n);
    order_moves(moves, n);

    if (maximizing) {
        best = -INF_SCORE;
        for (i = 0; i < n; i++) {
            Move m = moves[i];
            int v;
            make_move(gs, &m);
            v = minimax(gs, depth - 1, alpha, beta, 0, nodes);
            unmake_move(gs, &m);
            if (v > best) best = v;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
    } else {
        best = INF_SCORE;
        for (i = 0; i < n; i++) {
            Move m = moves[i];
            int v;
            make_move(gs, &m);
            v = minimax(gs, depth - 1, alpha, beta, 1, nodes);
            unmake_move(gs, &m);
            if (v < best) best = v;
            if (best < beta) beta = best;
            if (alpha >= beta) break;
        }
    }
    return best;
}

int find_best_move(GameState *gs, int depth, Move *best, SearchStats *stats)
{
    Move moves[MAX_MOVES];
    Move bestMove;
    int n, i, d, bestScore = 0;
    long nodes = 0;
    clock_t t0 = clock();

    n = generate_legal_moves(gs, moves, MAX_MOVES, 0);
    if (n == 0) return 0;
    if (depth < 1) depth = 1;

    for (i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Move tmp = moves[i]; moves[i] = moves[j]; moves[j] = tmp;
    }
    bestMove = moves[0];

    for (d = 1; d <= depth; d++) {
        int maximizing = gs->whiteToMove;
        int alpha = -INF_SCORE, beta = INF_SCORE;
        int curBest = maximizing ? -INF_SCORE : INF_SCORE;
        Move curMove = moves[0];

        score_moves(moves, n);
        if (d > 1) {
            for (i = 0; i < n; i++) {
                if (moves[i].fromRow == bestMove.fromRow && moves[i].fromCol == bestMove.fromCol &&
                    moves[i].toRow == bestMove.toRow && moves[i].toCol == bestMove.toCol &&
                    moves[i].promoted == bestMove.promoted)
                    moves[i].score += 1000000;
            }
        }
        order_moves(moves, n);

        for (i = 0; i < n; i++) {
            Move m = moves[i];
            int v;
            make_move(gs, &m);
            v = minimax(gs, d - 1, alpha, beta, !maximizing, &nodes);
            unmake_move(gs, &m);
            if (maximizing) {
                if (v > curBest) { curBest = v; curMove = moves[i]; }
                if (v > alpha) alpha = v;
            } else {
                if (v < curBest) { curBest = v; curMove = moves[i]; }
                if (v < beta) beta = v;
            }
        }
        bestMove = curMove;
        bestScore = curBest;
    }

    bestMove.score = bestScore;
    *best = bestMove;
    if (stats) {
        stats->nodes = nodes;
        stats->depth = depth;
        stats->score = bestScore;
        stats->ms = 1000.0 * (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    }
    return 1;
}