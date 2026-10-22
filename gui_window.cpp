/* ===========================================================================
 *  gui_window.cpp - Windows GUI Window for CheckMate (100% MinGW Compatible)
 *  Team: The Binary Guild | Graphic Era University
 * =========================================================================== */
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include "dsa.h"
#include "chess_rules.h"
#include "minimax.h"

#define TILE_SIZE 70
#define BOARD_OFFSET_X 30
#define BOARD_OFFSET_Y 40
#define SIDEBAR_X 610

enum GameMode { MODE_PVC = 0, MODE_PVP = 1 };

static GameState  g_gs;
static UndoStack  g_undo;
static MoveList   g_history;
static HashTable  g_seen;

static Move       g_legal[MAX_MOVES];
static int        g_nLegal = 0;

static int        g_selR = -1, g_selC = -1;
static bool       g_hasSelection = false;

static Move       g_lastMove;
static bool       g_hasLast = false;

static int        g_status = GS_ONGOING;
static int        g_repCount = 0;
static GameMode   g_mode = MODE_PVC;
static int        g_aiLevel = 3;

static const wchar_t* get_piece_glyph(char p) {
    switch (p) {
        case 'K': return L"\u2654";
        case 'Q': return L"\u2655";
        case 'R': return L"\u2656";
        case 'B': return L"\u2657";
        case 'N': return L"\u2658";
        case 'P': return L"\u2659";
        case 'k': return L"\u265A";
        case 'q': return L"\u265B";
        case 'r': return L"\u265C";
        case 'b': return L"\u265D";
        case 'n': return L"\u265E";
        case 'p': return L"\u265F";
        default:  return L"";
    }
}

static void update_game_status() {
    g_repCount = ht_get(&g_seen, position_hash(&g_gs));
    if (g_repCount >= 3) g_status = GS_DRAW_REPETITION;
    else                 g_status = game_status(&g_gs);
}

static void apply_gui_move(HWND hwnd, Move m) {
    make_move(&g_gs, &m);
    stack_push(&g_undo, &m);
    list_append(&g_history, &m);
    ht_increment(&g_seen, position_hash(&g_gs));

    g_lastMove = m;
    g_hasLast = true;
    g_selR = g_selC = -1;
    g_hasSelection = false;

    g_nLegal = generate_legal_moves(&g_gs, g_legal, MAX_MOVES, 0);
    update_game_status();
    InvalidateRect(hwnd, NULL, TRUE);

    // AI Trigger agar Mode PvC ho
    if (g_mode == MODE_PVC && g_status == GS_ONGOING && !g_gs.whiteToMove) {
        UpdateWindow(hwnd);
        Move best;
        SearchStats stats;
        if (find_best_move(&g_gs, g_aiLevel, &best, &stats)) {
            make_move(&g_gs, &best);
            stack_push(&g_undo, &best);
            list_append(&g_history, &best);
            ht_increment(&g_seen, position_hash(&g_gs));
            g_lastMove = best;
            g_hasLast = true;
            g_nLegal = generate_legal_moves(&g_gs, g_legal, MAX_MOVES, 0);
            update_game_status();
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }
}

static void reset_gui_game(HWND hwnd) {
    stack_clear(&g_undo);
    list_clear(&g_history);
    ht_clear(&g_seen);
    gs_init(&g_gs);
    ht_increment(&g_seen, position_hash(&g_gs));

    g_selR = g_selC = -1;
    g_hasSelection = false;
    g_hasLast = false;
    g_nLegal = generate_legal_moves(&g_gs, g_legal, MAX_MOVES, 0);
    update_game_status();
    InvalidateRect(hwnd, NULL, TRUE);
}

static void handle_undo(HWND hwnd) {
    if (!stack_is_empty(&g_undo)) {
        Move m;
        if (stack_pop(&g_undo, &m)) {
            ht_decrement(&g_seen, position_hash(&g_gs));
            unmake_move(&g_gs, &m);
            list_remove_tail(&g_history, NULL);
            if (g_mode == MODE_PVC && !stack_is_empty(&g_undo)) {
                stack_pop(&g_undo, &m);
                ht_decrement(&g_seen, position_hash(&g_gs));
                unmake_move(&g_gs, &m);
                list_remove_tail(&g_history, NULL);
            }
            g_hasSelection = false;
            g_nLegal = generate_legal_moves(&g_gs, g_legal, MAX_MOVES, 0);
            update_game_status();
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            stack_init(&g_undo);
            list_init(&g_history);
            ht_init(&g_seen, 1031);
            reset_gui_game(hwnd);
            break;

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            // Button: Undo
            if (mx >= SIDEBAR_X && mx <= SIDEBAR_X + 130 && my >= 280 && my <= 315) {
                handle_undo(hwnd);
                return 0;
            }
            // Button: Mode Toggle (PvP / PvC)
            if (mx >= SIDEBAR_X && mx <= SIDEBAR_X + 220 && my >= 330 && my <= 365) {
                g_mode = (g_mode == MODE_PVC) ? MODE_PVP : MODE_PVC;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            // Button: New Game
            if (mx >= SIDEBAR_X && mx <= SIDEBAR_X + 130 && my >= 380 && my <= 415) {
                reset_gui_game(hwnd);
                return 0;
            }

            // Board coordinate detection
            int col = (mx - BOARD_OFFSET_X) / TILE_SIZE;
            int row = (my - BOARD_OFFSET_Y) / TILE_SIZE;

            if (row >= 0 && row < 8 && col >= 0 && col < 8 && g_status == GS_ONGOING) {
                if (!g_hasSelection) {
                    char p = g_gs.board[row][col];
                    bool own = g_gs.whiteToMove ? is_white_piece(p) : is_black_piece(p);
                    if (p != EMPTY && own) {
                        g_selR = row; g_selC = col;
                        g_hasSelection = true;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                } else {
                    if (g_selR == row && g_selC == col) {
                        g_hasSelection = false;
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        int chosen = -1;
                        for (int i = 0; i < g_nLegal; i++) {
                            if (g_legal[i].fromRow == g_selR && g_legal[i].fromCol == g_selC &&
                                g_legal[i].toRow == row && g_legal[i].toCol == col) {
                                chosen = i;
                                break;
                            }
                        }
                        if (chosen >= 0) {
                            apply_gui_move(hwnd, g_legal[chosen]);
                        } else {
                            char p = g_gs.board[row][col];
                            bool own = g_gs.whiteToMove ? is_white_piece(p) : is_black_piece(p);
                            if (p != EMPTY && own) {
                                g_selR = row; g_selC = col;
                                g_hasSelection = true;
                            } else {
                                g_hasSelection = false;
                            }
                            InvalidateRect(hwnd, NULL, TRUE);
                        }
                    }
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            SelectObject(memDC, memBitmap);

            // Dark Theme Window Background
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 33, 39));
            FillRect(memDC, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            // Board Brushes
            HBRUSH lightBrush  = CreateSolidBrush(RGB(240, 217, 181));
            HBRUSH darkBrush   = CreateSolidBrush(RGB(181, 136, 99));
            HBRUSH selBrush    = CreateSolidBrush(RGB(246, 246, 105));
            HBRUSH targetBrush = CreateSolidBrush(RGB(186, 202, 68));

            HFONT hFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Symbol");
            SelectObject(memDC, hFont);
            SetBkMode(memDC, TRANSPARENT);

            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    RECT tileRect = {
                        BOARD_OFFSET_X + c * TILE_SIZE,
                        BOARD_OFFSET_Y + r * TILE_SIZE,
                        BOARD_OFFSET_X + (c + 1) * TILE_SIZE,
                        BOARD_OFFSET_Y + (r + 1) * TILE_SIZE
                    };

                    bool isTarget = false;
                    if (g_hasSelection) {
                        for (int i = 0; i < g_nLegal; i++) {
                            if (g_legal[i].fromRow == g_selR && g_legal[i].fromCol == g_selC &&
                                g_legal[i].toRow == r && g_legal[i].toCol == c) {
                                isTarget = true;
                                break;
                            }
                        }
                    }

                    if (g_hasSelection && g_selR == r && g_selC == c) {
                        FillRect(memDC, &tileRect, selBrush);
                    } else if (isTarget) {
                        FillRect(memDC, &tileRect, targetBrush);
                    } else if ((r + c) % 2 == 0) {
                        FillRect(memDC, &tileRect, lightBrush);
                    } else {
                        FillRect(memDC, &tileRect, darkBrush);
                    }

                    char p = g_gs.board[r][c];
                    if (p != EMPTY) {
                        if (is_white_piece(p)) {
                            SetTextColor(memDC, RGB(255, 255, 255));
                        } else {
                            SetTextColor(memDC, RGB(20, 20, 20));
                        }
                        const wchar_t* glyph = get_piece_glyph(p);
                        DrawTextW(memDC, glyph, -1, &tileRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }
            }

            // Right Sidebar Text & Live DSA Indicators
            HFONT textFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            SelectObject(memDC, textFont);

            SetTextColor(memDC, RGB(255, 215, 0));
            TextOutW(memDC, SIDEBAR_X, 40, L"CHECKMATE (PBL GUI)", (int)wcslen(L"CHECKMATE (PBL GUI)"));

            SetTextColor(memDC, RGB(220, 220, 220));
            wchar_t infoBuf[128];
            wsprintfW(infoBuf, L"Turn: %s", g_gs.whiteToMove ? L"White" : L"Black");
            TextOutW(memDC, SIDEBAR_X, 80, infoBuf, (int)wcslen(infoBuf));

            wsprintfW(infoBuf, L"Mode: %s", (g_mode == MODE_PVC) ? L"Player vs AI (PvC)" : L"2-Players (PvP)");
            TextOutW(memDC, SIDEBAR_X, 110, infoBuf, (int)wcslen(infoBuf));

            SetTextColor(memDC, RGB(78, 205, 196));
            wsprintfW(infoBuf, L"[Stack Undo Size]: %d", g_undo.size);
            TextOutW(memDC, SIDEBAR_X, 150, infoBuf, (int)wcslen(infoBuf));

            wsprintfW(infoBuf, L"[Linked List Moves]: %d", g_history.size);
            TextOutW(memDC, SIDEBAR_X, 180, infoBuf, (int)wcslen(infoBuf));

            wsprintfW(infoBuf, L"[Hash Table Hits]: %dx", g_repCount);
            TextOutW(memDC, SIDEBAR_X, 210, infoBuf, (int)wcslen(infoBuf));

            // On-screen Clickable Buttons
            HBRUSH btnBrush = CreateSolidBrush(RGB(50, 75, 110));
            RECT rUndo = { SIDEBAR_X, 280, SIDEBAR_X + 130, 315 };
            FillRect(memDC, &rUndo, btnBrush);
            SetTextColor(memDC, RGB(255, 255, 255));
            DrawTextW(memDC, L"UNDO MOVE", -1, &rUndo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rMode = { SIDEBAR_X, 330, SIDEBAR_X + 220, 365 };
            FillRect(memDC, &rMode, btnBrush);
            DrawTextW(memDC, L"TOGGLE PVP / PVC", -1, &rMode, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rNew = { SIDEBAR_X, 380, SIDEBAR_X + 130, 415 };
            FillRect(memDC, &rNew, btnBrush);
            DrawTextW(memDC, L"NEW GAME", -1, &rNew, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            DeleteObject(btnBrush);
            DeleteObject(lightBrush);
            DeleteObject(darkBrush);
            DeleteObject(selBrush);
            DeleteObject(targetBrush);
            DeleteObject(hFont);
            DeleteObject(textFont);

            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            stack_clear(&g_undo);
            list_clear(&g_history);
            ht_free(&g_seen);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* Standalone Standard main: Works on all MinGW/GCC without WinMain linker errors */
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"CheckMateGUIWindow";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"CheckMateGUIWindow",
        L"CheckMate - The Binary Guild (Graphic Era CSE)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 920, 660,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}