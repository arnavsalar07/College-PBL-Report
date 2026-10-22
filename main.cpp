/* ===========================================================================
 *  main.cpp  -  CHECKMATE : Clean High-Contrast Terminal Interface
 *  Team: The Binary Guild | Graphic Era University
 * =========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string>

#include "dsa.h"
#include "chess_rules.h"
#include "minimax.h"

#if defined(_WIN32)
  #include <windows.h>
  #include <io.h>

  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif
  #ifndef FROM_LEFT_1ST_BUTTON_OFFSET
    #define FROM_LEFT_1ST_BUTTON_OFFSET 0x0001
  #endif
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <sys/select.h>
#endif

enum GameMode { MODE_PVC = 0, MODE_PVP = 1 };

struct App {
    GameState   gs;
    UndoStack   undo;
    MoveList    history;
    EventQueue  queue;
    HashTable   seen;

    Move        legal[MAX_MOVES];
    int         nLegal;

    int         selR, selC;
    int         hasSelection;

    Move        lastMove;
    int         hasLast;

    int         status;
    int         repCount;

    int         aiLevel;
    GameMode    mode;

    char        statusMsg[80];
    char        aiStatsMsg[80];
};

static App g_app;

#if defined(_WIN32)
static HANDLE hIn = NULL, hOut = NULL;
static DWORD origInMode = 0, origOutMode = 0;
static UINT origCP = 0, origOutputCP = 0;

static void term_init(void)
{
    hIn  = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hIn,  &origInMode);
    GetConsoleMode(hOut, &origOutMode);
    origCP = GetConsoleCP();
    origOutputCP = GetConsoleOutputCP();

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    DWORD inMode = ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT;
    DWORD outMode = origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hIn,  inMode);
    SetConsoleMode(hOut, outMode);

    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);
}

static void term_restore(void)
{
    if (hOut) {
        CONSOLE_CURSOR_INFO ci;
        GetConsoleCursorInfo(hOut, &ci);
        ci.bVisible = TRUE;
        SetConsoleCursorInfo(hOut, &ci);
        SetConsoleMode(hOut, origOutMode);
    }
    if (hIn)  SetConsoleMode(hIn, origInMode);
    if (origCP) SetConsoleCP(origCP);
    if (origOutputCP) SetConsoleOutputCP(origOutputCP);
}
#endif

/* Unicode chess symbols */
static const char *piece_glyph(char p)
{
    switch (p) {
        case 'K': return "♔";
        case 'Q': return "♕";
        case 'R': return "♖";
        case 'B': return "♗";
        case 'N': return "♘";
        case 'P': return "♙";
        case 'k': return "♚";
        case 'q': return "♛";
        case 'r': return "♜";
        case 'b': return "♝";
        case 'n': return "♞";
        case 'p': return "♟";
        default:  return " ";
    }
}

static void update_status(App *a)
{
    a->repCount = ht_get(&a->seen, position_hash(&a->gs));
    if (a->repCount >= 3)
        a->status = GS_DRAW_REPETITION;
    else
        a->status = game_status(&a->gs);

    switch (a->status) {
        case GS_CHECKMATE:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "CHECKMATE! %s wins!",
                     a->gs.whiteToMove ? "Black" : "White");
            break;
        case GS_STALEMATE:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "STALEMATE - Draw");
            break;
        case GS_DRAW_50:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "DRAW - 50-move rule");
            break;
        case GS_DRAW_MATERIAL:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "DRAW - Insufficient material");
            break;
        case GS_DRAW_REPETITION:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "DRAW - Threefold repetition");
            break;
        default:
            snprintf(a->statusMsg, sizeof(a->statusMsg), "%s to move%s",
                     a->gs.whiteToMove ? "White" : "Black",
                     king_in_check(&a->gs, a->gs.whiteToMove) ? " (CHECK)" : "");
            break;
    }
}

static void apply_move_app(App *a, Move m)
{
    make_move(&a->gs, &m);
    stack_push(&a->undo, &m);
    list_append(&a->history, &m);
    ht_increment(&a->seen, position_hash(&a->gs));

    a->lastMove = m;
    a->hasLast = 1;
    a->selR = a->selC = -1;
    a->hasSelection = 0;

    a->nLegal = generate_legal_moves(&a->gs, a->legal, MAX_MOVES, 0);
    update_status(a);
}

static void reset_game(App *a)
{
    stack_clear(&a->undo);
    list_clear(&a->history);
    queue_clear(&a->queue);
    ht_clear(&a->seen);

    gs_init(&a->gs);
    ht_increment(&a->seen, position_hash(&a->gs));

    a->selR = a->selC = -1;
    a->hasSelection = 0;
    a->hasLast = 0;
    a->aiStatsMsg[0] = '\0';

    a->nLegal = generate_legal_moves(&a->gs, a->legal, MAX_MOVES, 0);
    update_status(a);
}

static void render(App *a)
{
    std::string o;
    o.reserve(4096);

    /* Clean full screen clear sequence */
    o += "\033[2J\033[H";

    o += "\033[1;37;44m ================= CHECKMATE : THE BINARY GUILD (GEU) ================= \033[0m\n";
    
    char buf[256];
    snprintf(buf, sizeof(buf), " Status: \033[1;33m%-25s\033[0m | Mode: \033[1;36m%s\033[0m [Press 'M']\n\n",
             a->statusMsg, (a->mode == MODE_PVC ? "PvC (vs AI)" : "PvP (2-Players)"));
    o += buf;

    for (int r = 0; r < 8; r++) {
        snprintf(buf, sizeof(buf), "  %d ", 8 - r);
        o += buf;

        for (int c = 0; c < 8; c++) {
            int isLight = ((r + c) % 2 == 0);
            /* Contrast board colors: Light Beige vs Dark Mocha Brown */
            const char *bg = isLight ? "\033[48;5;223m" : "\033[48;5;95m";

            if (a->hasSelection && a->selR == r && a->selC == c) {
                bg = "\033[48;5;220m"; /* Selected Square */
            } else if (a->hasLast && ((a->lastMove.fromRow == r && a->lastMove.fromCol == c) ||
                                      (a->lastMove.toRow == r && a->lastMove.toCol == c))) {
                bg = "\033[48;5;177m"; /* Last Move Trail */
            }

            int isTarget = 0;
            if (a->hasSelection) {
                for (int i = 0; i < a->nLegal; i++) {
                    if (a->legal[i].fromRow == a->selR && a->legal[i].fromCol == a->selC &&
                        a->legal[i].toRow == r && a->legal[i].toCol == c) {
                        isTarget = 1;
                        break;
                    }
                }
            }
            if (isTarget) {
                bg = (a->gs.board[r][c] != EMPTY) ? "\033[48;5;203m" : "\033[48;5;150m";
            }

            char p = a->gs.board[r][c];
            o += bg;

            if (p == EMPTY) {
                o += "   ";
            } else {
                int white = is_white_piece(p);
                /* Crisp contrast: White = Glowing Ivory, Black = Solid Dark Charcoal */
                if (white) {
                    o += "\033[1;38;5;231m ";
                } else {
                    o += "\033[1;38;5;232m ";
                }
                o += piece_glyph(p);
                o += " ";
            }
            o += "\033[0m";
        }

        /* Sidebar: Clean Alignment */
        if (r == 0) {
            snprintf(buf, sizeof(buf), "   \033[1;32m[DSA STACK (UNDO)]\033[0m  Size: %d", a->undo.size);
            o += buf;
        } else if (r == 1) {
            snprintf(buf, sizeof(buf), "   \033[1;34m[DSA LINKED LIST]\033[0m   Moves: %d", a->history.size);
            o += buf;
        } else if (r == 2) {
            snprintf(buf, sizeof(buf), "   \033[1;35m[DSA HASH TABLE]\033[0m    Seen: %dx", a->repCount);
            o += buf;
        } else if (r == 3) {
            snprintf(buf, sizeof(buf), "   \033[1;33m[ACTIVE GAME MODE]\033[0m  %s", (a->mode == MODE_PVC ? "Human vs AI" : "Player vs Player"));
            o += buf;
        } else if (r == 4) {
            snprintf(buf, sizeof(buf), "   \033[1;37m[CONTROLS]\033[0m          'U' Undo | 'M' Mode | 'N' New");
            o += buf;
        } else if (r == 5) {
            snprintf(buf, sizeof(buf), "                       'L' Level | 'Q' Quit");
            o += buf;
        } else if (r == 6 && a->mode == MODE_PVC && a->aiStatsMsg[0] != '\0') {
            snprintf(buf, sizeof(buf), "   \033[1;36m%s\033[0m", a->aiStatsMsg);
            o += buf;
        }
        o += "\n";
    }

    o += "     a  b  c  d  e  f  g  h\n\n";
    o += " \033[1;30;47m Controls \033[0m Mouse Click pieces to move | 'U' Undo | 'M' Toggle PvP/PvC | 'Q' Quit\n";

#if defined(_WIN32)
    DWORD written;
    WriteConsoleA(hOut, o.data(), (DWORD)o.size(), &written, NULL);
#else
    printf("%s", o.c_str());
    fflush(stdout);
#endif
}

int main(void)
{
#if defined(_WIN32)
    term_init();
#endif

    App *a = &g_app;
    stack_init(&a->undo);
    list_init(&a->history);
    queue_init(&a->queue);
    ht_init(&a->seen, 1031);

    a->aiLevel = 3;
    a->mode = MODE_PVC;
    reset_game(a);

    /* Initial clear */
    system("cls");

    int running = 1;
    while (running) {
        render(a);

        /* AI Move only triggers in PvC mode for Black */
        if (a->mode == MODE_PVC && a->status == GS_ONGOING && !a->gs.whiteToMove) {
            Move best;
            SearchStats stats;
            if (find_best_move(&a->gs, a->aiLevel, &best, &stats)) {
                snprintf(a->aiStatsMsg, sizeof(a->aiStatsMsg), "AI: depth=%d nodes=%ld time=%.0fms", stats.depth, stats.nodes, stats.ms);
                apply_move_app(a, best);
                continue;
            }
        }

#if defined(_WIN32)
        INPUT_RECORD rec;
        DWORD readCount = 0;
        if (!ReadConsoleInputW(hIn, &rec, 1, &readCount) || readCount == 0) continue;

        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            char ch = (char)rec.Event.KeyEvent.uChar.AsciiChar;
            switch (tolower(ch)) {
                case 'q': running = 0; break;
                case 'u':
                    if (!stack_is_empty(&a->undo)) {
                        Move m;
                        if (stack_pop(&a->undo, &m)) {
                            ht_decrement(&a->seen, position_hash(&a->gs));
                            unmake_move(&a->gs, &m);
                            list_remove_tail(&a->history, NULL);
                            if (a->mode == MODE_PVC && !stack_is_empty(&a->undo)) {
                                stack_pop(&a->undo, &m);
                                ht_decrement(&a->seen, position_hash(&a->gs));
                                unmake_move(&a->gs, &m);
                                list_remove_tail(&a->history, NULL);
                            }
                            a->hasSelection = 0;
                            a->nLegal = generate_legal_moves(&a->gs, a->legal, MAX_MOVES, 0);
                            update_status(a);
                        }
                    }
                    break;
                case 'm':
                    a->mode = (a->mode == MODE_PVC) ? MODE_PVP : MODE_PVC;
                    break;
                case 'n':
                    reset_game(a);
                    break;
                case 'l':
                    a->aiLevel = (a->aiLevel % 4) + 1;
                    break;
            }
        } else if (rec.EventType == MOUSE_EVENT) {
            if (rec.Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_OFFSET) {
                int mx = rec.Event.MouseEvent.dwMousePosition.X;
                int my = rec.Event.MouseEvent.dwMousePosition.Y;
                int boardStartY = 3;
                int boardStartX = 5;

                if (my >= boardStartY && my < boardStartY + 8) {
                    int r = my - boardStartY;
                    int c = (mx - boardStartX) / 3;

                    if (c >= 0 && c < 8 && a->status == GS_ONGOING) {
                        if (!a->hasSelection) {
                            char p = a->gs.board[r][c];
                            int isOwn = a->gs.whiteToMove ? is_white_piece(p) : is_black_piece(p);
                            if (p != EMPTY && isOwn) {
                                a->selR = r; a->selC = c;
                                a->hasSelection = 1;
                            }
                        } else {
                            if (a->selR == r && a->selC == c) {
                                a->hasSelection = 0;
                                a->selR = a->selC = -1;
                            } else {
                                int chosen = -1;
                                for (int i = 0; i < a->nLegal; i++) {
                                    if (a->legal[i].fromRow == a->selR && a->legal[i].fromCol == a->selC &&
                                        a->legal[i].toRow == r && a->legal[i].toCol == c) {
                                        if (!(a->legal[i].flags & MF_PROMOTION) || a->legal[i].promoted == 'Q' || a->legal[i].promoted == 'q') {
                                            chosen = i;
                                            break;
                                        }
                                        if (chosen < 0) chosen = i;
                                    }
                                }
                                if (chosen >= 0) {
                                    apply_move_app(a, a->legal[chosen]);
                                } else {
                                    char p = a->gs.board[r][c];
                                    int isOwn = a->gs.whiteToMove ? is_white_piece(p) : is_black_piece(p);
                                    if (p != EMPTY && isOwn) {
                                        a->selR = r; a->selC = c;
                                        a->hasSelection = 1;
                                    } else {
                                        a->hasSelection = 0;
                                        a->selR = a->selC = -1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
#endif
    }

    stack_clear(&a->undo);
    list_clear(&a->history);
    queue_clear(&a->queue);
    ht_free(&a->seen);

#if defined(_WIN32)
    term_restore();
#endif
    return 0;
}