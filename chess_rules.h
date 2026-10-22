/* ===========================================================================
 *  chess_rules.h  -  Board representation and chess rule engine.
 *  Row 0 = rank 8 (Black), row 7 = rank 1 (White).
 * =========================================================================== */
#ifndef CHESS_RULES_H
#define CHESS_RULES_H

#include "dsa.h"

#define MAX_MOVES 256
#define EMPTY '.'

enum { CR_WK = 1, CR_WQ = 2, CR_BK = 4, CR_BQ = 8 };

enum {
    GS_ONGOING = 0,
    GS_CHECKMATE,
    GS_STALEMATE,
    GS_DRAW_50,
    GS_DRAW_MATERIAL,
    GS_DRAW_REPETITION
};

typedef struct GameState {
    char board[8][8];
    int  whiteToMove;
    int  castle;
    int  epCol;
    int  halfmove;
    int  fullmove;
} GameState;

static inline int is_white_piece(char c) { return c >= 'A' && c <= 'Z'; }
static inline int is_black_piece(char c) { return c >= 'a' && c <= 'z'; }
static inline int in_bounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

void gs_init(GameState *gs);

/* Move generation */
int  generate_pseudo_moves(const GameState *gs, Move *out, int max, int capturesOnly);
int  generate_legal_moves(GameState *gs, Move *out, int max, int capturesOnly);

/* Make and unmake */
void make_move(GameState *gs, Move *m);
void unmake_move(GameState *gs, const Move *m);

/* Checks and attacks */
int  square_attacked(const GameState *gs, int r, int c, int byWhite);
int  king_in_check(const GameState *gs, int white);

/* Terminal checks */
int  insufficient_material(const GameState *gs);
int  game_status(GameState *gs);

/* Utilities */
int      piece_value(char p);
uint64_t position_hash(const GameState *gs);
void     move_to_string(const Move *m, char *buf, size_t n);

#endif /* CHESS_RULES_H */