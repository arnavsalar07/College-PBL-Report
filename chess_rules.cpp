/* ===========================================================================
 *  chess_rules.cpp  -  Legal move generation, ray casting, check verification,
 *  castling, en passant, promotions, and draw conditions.
 * =========================================================================== */
#include "chess_rules.h"
#include <stdio.h>
#include <string.h>

static const int KNIGHT_D[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
static const int KING_D[8][2]   = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
static const int ROOK_D[4][2]   = {{-1,0},{1,0},{0,-1},{0,1}};
static const int BISHOP_D[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};

static char to_upper_piece(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

void gs_init(GameState *gs)
{
    static const char *start[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    int r, c;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            gs->board[r][c] = start[r][c];
    gs->whiteToMove = 1;
    gs->castle = CR_WK | CR_WQ | CR_BK | CR_BQ;
    gs->epCol = -1;
    gs->halfmove = 0;
    gs->fullmove = 1;
}

int piece_value(char p)
{
    switch (to_upper_piece(p)) {
        case 'P': return 100;
        case 'N': return 320;
        case 'B': return 330;
        case 'R': return 500;
        case 'Q': return 900;
        case 'K': return 20000;
        default:  return 0;
    }
}

int square_attacked(const GameState *gs, int r, int c, int byWhite)
{
    int i, d, nr, nc, pr;
    char pawn   = byWhite ? 'P' : 'p';
    char knight = byWhite ? 'N' : 'n';
    char king   = byWhite ? 'K' : 'k';
    char rook   = byWhite ? 'R' : 'r';
    char bishop = byWhite ? 'B' : 'b';
    char queen  = byWhite ? 'Q' : 'q';

    pr = byWhite ? r + 1 : r - 1;
    if (in_bounds(pr, c - 1) && gs->board[pr][c - 1] == pawn) return 1;
    if (in_bounds(pr, c + 1) && gs->board[pr][c + 1] == pawn) return 1;

    for (i = 0; i < 8; i++) {
        nr = r + KNIGHT_D[i][0]; nc = c + KNIGHT_D[i][1];
        if (in_bounds(nr, nc) && gs->board[nr][nc] == knight) return 1;
        nr = r + KING_D[i][0];   nc = c + KING_D[i][1];
        if (in_bounds(nr, nc) && gs->board[nr][nc] == king) return 1;
    }
    for (d = 0; d < 4; d++) {
        nr = r + ROOK_D[d][0]; nc = c + ROOK_D[d][1];
        while (in_bounds(nr, nc)) {
            char t = gs->board[nr][nc];
            if (t != EMPTY) {
                if (t == rook || t == queen) return 1;
                break;
            }
            nr += ROOK_D[d][0]; nc += ROOK_D[d][1];
        }
        nr = r + BISHOP_D[d][0]; nc = c + BISHOP_D[d][1];
        while (in_bounds(nr, nc)) {
            char t = gs->board[nr][nc];
            if (t != EMPTY) {
                if (t == bishop || t == queen) return 1;
                break;
            }
            nr += BISHOP_D[d][0]; nc += BISHOP_D[d][1];
        }
    }
    return 0;
}

int king_in_check(const GameState *gs, int white)
{
    char k = white ? 'K' : 'k';
    int r, c;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (gs->board[r][c] == k)
                return square_attacked(gs, r, c, !white);
    return 0;
}

static void push_move(Move *out, int *n, int max, int fr, int fc, int tr, int tc,
                      char piece, char captured, char promoted, int flags)
{
    Move *m;
    if (*n >= max) return;
    m = &out[*n];
    m->fromRow = fr; m->fromCol = fc;
    m->toRow = tr;   m->toCol = tc;
    m->piece = piece;
    m->captured = captured;
    m->promoted = promoted;
    m->score = 0;
    m->flags = flags;
    m->prevCastle = 0;
    m->prevEpCol = -1;
    m->prevHalfmove = 0;
    (*n)++;
}

static void push_pawn_move(Move *out, int *n, int max, int fr, int fc, int tr, int tc,
                           char piece, char captured, int flags, int white, int promo)
{
    if (!promo) {
        push_move(out, n, max, fr, fc, tr, tc, piece, captured, EMPTY, flags);
    } else {
        const char *set = white ? "QRBN" : "qrbn";
        int i;
        for (i = 0; i < 4; i++)
            push_move(out, n, max, fr, fc, tr, tc, piece, captured, set[i], flags | MF_PROMOTION);
    }
}

static void gen_slider(const GameState *gs, Move *out, int *n, int max, int r, int c,
                       const int (*dirs)[2], int nd, int capturesOnly)
{
    char p = gs->board[r][c];
    int white = is_white_piece(p);
    int d;
    for (d = 0; d < nd; d++) {
        int nr = r + dirs[d][0], nc = c + dirs[d][1];
        while (in_bounds(nr, nc)) {
            char t = gs->board[nr][nc];
            if (t == EMPTY) {
                if (!capturesOnly) push_move(out, n, max, r, c, nr, nc, p, EMPTY, EMPTY, 0);
            } else {
                if (is_white_piece(t) != white)
                    push_move(out, n, max, r, c, nr, nc, p, t, EMPTY, 0);
                break;
            }
            nr += dirs[d][0]; nc += dirs[d][1];
        }
    }
}

int generate_pseudo_moves(const GameState *gs, Move *out, int max, int capturesOnly)
{
    int n = 0, r, c, i;
    int white = gs->whiteToMove;

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            char p = gs->board[r][c];
            if (p == EMPTY || (is_white_piece(p) != 0) != (white != 0)) continue;

            switch (to_upper_piece(p)) {
            case 'P': {
                int dir = white ? -1 : 1;
                int startRow = white ? 6 : 1;
                int lastRow  = white ? 0 : 7;
                int nr = r + dir, dc;
                if (in_bounds(nr, c) && gs->board[nr][c] == EMPTY) {
                    if (!capturesOnly || nr == lastRow)
                        push_pawn_move(out, &n, max, r, c, nr, c, p, EMPTY, 0, white, nr == lastRow);
                    if (!capturesOnly && r == startRow && gs->board[r + 2 * dir][c] == EMPTY)
                        push_move(out, &n, max, r, c, r + 2 * dir, c, p, EMPTY, EMPTY, MF_DOUBLE_PUSH);
                }
                for (dc = -1; dc <= 1; dc += 2) {
                    int nc = c + dc;
                    char t;
                    if (!in_bounds(nr, nc)) continue;
                    t = gs->board[nr][nc];
                    if (t != EMPTY && (is_white_piece(t) != 0) != (white != 0)) {
                        push_pawn_move(out, &n, max, r, c, nr, nc, p, t, 0, white, nr == lastRow);
                    } else if (t == EMPTY && gs->epCol == nc && r == (white ? 3 : 4)) {
                        char ep = gs->board[r][nc];
                        if (ep == (white ? 'p' : 'P'))
                            push_move(out, &n, max, r, c, nr, nc, p, ep, EMPTY, MF_EN_PASSANT);
                    }
                }
                break;
            }
            case 'N':
                for (i = 0; i < 8; i++) {
                    int nr = r + KNIGHT_D[i][0], nc = c + KNIGHT_D[i][1];
                    char t;
                    if (!in_bounds(nr, nc)) continue;
                    t = gs->board[nr][nc];
                    if (t == EMPTY) {
                        if (!capturesOnly) push_move(out, &n, max, r, c, nr, nc, p, EMPTY, EMPTY, 0);
                    } else if ((is_white_piece(t) != 0) != (white != 0)) {
                        push_move(out, &n, max, r, c, nr, nc, p, t, EMPTY, 0);
                    }
                }
                break;
            case 'B': gen_slider(gs, out, &n, max, r, c, BISHOP_D, 4, capturesOnly); break;
            case 'R': gen_slider(gs, out, &n, max, r, c, ROOK_D,   4, capturesOnly); break;
            case 'Q':
                gen_slider(gs, out, &n, max, r, c, ROOK_D,   4, capturesOnly);
                gen_slider(gs, out, &n, max, r, c, BISHOP_D, 4, capturesOnly);
                break;
            case 'K':
                for (i = 0; i < 8; i++) {
                    int nr = r + KING_D[i][0], nc = c + KING_D[i][1];
                    char t;
                    if (!in_bounds(nr, nc)) continue;
                    t = gs->board[nr][nc];
                    if (t == EMPTY) {
                        if (!capturesOnly) push_move(out, &n, max, r, c, nr, nc, p, EMPTY, EMPTY, 0);
                    } else if ((is_white_piece(t) != 0) != (white != 0)) {
                        push_move(out, &n, max, r, c, nr, nc, p, t, EMPTY, 0);
                    }
                }
                if (!capturesOnly) {
                    int row = white ? 7 : 0;
                    int kRight = white ? CR_WK : CR_BK;
                    int qRight = white ? CR_WQ : CR_BQ;
                    char rk = white ? 'R' : 'r';
                    if (r == row && c == 4 && !square_attacked(gs, row, 4, !white)) {
                        if ((gs->castle & kRight) && gs->board[row][7] == rk &&
                            gs->board[row][5] == EMPTY && gs->board[row][6] == EMPTY &&
                            !square_attacked(gs, row, 5, !white) &&
                            !square_attacked(gs, row, 6, !white))
                            push_move(out, &n, max, row, 4, row, 6, p, EMPTY, EMPTY, MF_CASTLE_K);
                        if ((gs->castle & qRight) && gs->board[row][0] == rk &&
                            gs->board[row][1] == EMPTY && gs->board[row][2] == EMPTY &&
                            gs->board[row][3] == EMPTY &&
                            !square_attacked(gs, row, 3, !white) &&
                            !square_attacked(gs, row, 2, !white))
                            push_move(out, &n, max, row, 4, row, 2, p, EMPTY, EMPTY, MF_CASTLE_Q);
                    }
                }
                break;
            default: break;
            }
        }
    }
    return n;
}

static void strip_rights(GameState *gs, int r, int c)
{
    if (r == 7 && c == 0) gs->castle &= ~CR_WQ;
    else if (r == 7 && c == 7) gs->castle &= ~CR_WK;
    else if (r == 0 && c == 0) gs->castle &= ~CR_BQ;
    else if (r == 0 && c == 7) gs->castle &= ~CR_BK;
    else if (r == 7 && c == 4) gs->castle &= ~(CR_WK | CR_WQ);
    else if (r == 0 && c == 4) gs->castle &= ~(CR_BK | CR_BQ);
}

void make_move(GameState *gs, Move *m)
{
    char piece = gs->board[m->fromRow][m->fromCol];

    m->prevCastle = gs->castle;
    m->prevEpCol = gs->epCol;
    m->prevHalfmove = gs->halfmove;
    m->piece = piece;

    if (m->flags & MF_EN_PASSANT) {
        m->captured = gs->board[m->fromRow][m->toCol];
        gs->board[m->fromRow][m->toCol] = EMPTY;
    } else {
        m->captured = gs->board[m->toRow][m->toCol];
    }

    gs->board[m->toRow][m->toCol] = (m->flags & MF_PROMOTION) ? m->promoted : piece;
    gs->board[m->fromRow][m->fromCol] = EMPTY;

    if (m->flags & MF_CASTLE_K) {
        gs->board[m->fromRow][5] = gs->board[m->fromRow][7];
        gs->board[m->fromRow][7] = EMPTY;
    } else if (m->flags & MF_CASTLE_Q) {
        gs->board[m->fromRow][3] = gs->board[m->fromRow][0];
        gs->board[m->fromRow][0] = EMPTY;
    }

    strip_rights(gs, m->fromRow, m->fromCol);
    strip_rights(gs, m->toRow, m->toCol);

    gs->epCol = (m->flags & MF_DOUBLE_PUSH) ? m->fromCol : -1;

    if (to_upper_piece(piece) == 'P' || m->captured != EMPTY) gs->halfmove = 0;
    else gs->halfmove++;

    if (!gs->whiteToMove) gs->fullmove++;
    gs->whiteToMove = !gs->whiteToMove;
}

void unmake_move(GameState *gs, const Move *m)
{
    gs->whiteToMove = !gs->whiteToMove;
    if (!gs->whiteToMove) gs->fullmove--;

    gs->board[m->fromRow][m->fromCol] = m->piece;
    if (m->flags & MF_EN_PASSANT) {
        gs->board[m->toRow][m->toCol] = EMPTY;
        gs->board[m->fromRow][m->toCol] = m->captured;
    } else {
        gs->board[m->toRow][m->toCol] = m->captured;
    }

    if (m->flags & MF_CASTLE_K) {
        gs->board[m->fromRow][7] = gs->board[m->fromRow][5];
        gs->board[m->fromRow][5] = EMPTY;
    } else if (m->flags & MF_CASTLE_Q) {
        gs->board[m->fromRow][0] = gs->board[m->fromRow][3];
        gs->board[m->fromRow][3] = EMPTY;
    }

    gs->castle = m->prevCastle;
    gs->epCol = m->prevEpCol;
    gs->halfmove = m->prevHalfmove;
}

int generate_legal_moves(GameState *gs, Move *out, int max, int capturesOnly)
{
    Move tmp[MAX_MOVES];
    int np = generate_pseudo_moves(gs, tmp, MAX_MOVES, capturesOnly);
    int side = gs->whiteToMove;
    int n = 0, i;
    for (i = 0; i < np; i++) {
        Move m = tmp[i];
        int illegal;
        make_move(gs, &m);
        illegal = king_in_check(gs, side);
        unmake_move(gs, &m);
        if (!illegal && n < max) out[n++] = tmp[i];
    }
    return n;
}

int insufficient_material(const GameState *gs)
{
    int minors = 0, r, c;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            char p = to_upper_piece(gs->board[r][c]);
            if (p == 'P' || p == 'R' || p == 'Q') return 0;
            if (p == 'N' || p == 'B') minors++;
        }
    return minors <= 1;
}

int game_status(GameState *gs)
{
    Move tmp[MAX_MOVES];
    int n = generate_legal_moves(gs, tmp, MAX_MOVES, 0);
    if (n == 0)
        return king_in_check(gs, gs->whiteToMove) ? GS_CHECKMATE : GS_STALEMATE;
    if (gs->halfmove >= 100) return GS_DRAW_50;
    if (insufficient_material(gs)) return GS_DRAW_MATERIAL;
    return GS_ONGOING;
}

uint64_t position_hash(const GameState *gs)
{
    unsigned char buf[72];
    int r, c, k = 0;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            buf[k++] = (unsigned char)gs->board[r][c];
    buf[k++] = gs->whiteToMove ? 'w' : 'b';
    buf[k++] = (unsigned char)('0' + gs->castle);
    buf[k++] = (unsigned char)('1' + gs->epCol);
    return fnv1a_64(buf, (size_t)k);
}

void move_to_string(const Move *m, char *buf, size_t n)
{
    char tmp[24];
    int i = 0;
    char p = to_upper_piece(m->piece);

    if (m->flags & MF_CASTLE_K) { snprintf(buf, n, "O-O");   return; }
    if (m->flags & MF_CASTLE_Q) { snprintf(buf, n, "O-O-O"); return; }

    if (p != 'P') tmp[i++] = p;
    tmp[i++] = (char)('a' + m->fromCol);
    tmp[i++] = (char)('8' - m->fromRow);
    tmp[i++] = (m->captured != EMPTY) ? 'x' : '-';
    tmp[i++] = (char)('a' + m->toCol);
    tmp[i++] = (char)('8' - m->toRow);
    if (m->flags & MF_PROMOTION) {
        tmp[i++] = '=';
        tmp[i++] = to_upper_piece(m->promoted);
    }
    tmp[i] = '\0';
    snprintf(buf, n, "%s", tmp);
}