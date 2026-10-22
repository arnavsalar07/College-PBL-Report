/* ===========================================================================
 *  dsa.h  -  CHECKMATE : Intelligent Chess Game Using Data Structures
 *  Team "The Binary Guild" | Graphic Era (Deemed to be University)
 *
 *  Shared header for custom manual C data structures (no STL):
 *    - Move struct
 *    - Stack (LIFO, singly linked list)  -> Undo operations
 *    - Doubly linked list                 -> Sequential move history
 *    - Queue (FIFO, linked list)         -> Event / input pipeline
 *    - Chained hash table (FNV-1a 64-bit) -> 3-fold repetition detection
 * =========================================================================== */
#ifndef DSA_H
#define DSA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ Move */
#define MF_NONE        0
#define MF_CASTLE_K    1   /* kingside castling                      */
#define MF_CASTLE_Q    2   /* queenside castling                     */
#define MF_EN_PASSANT  4   /* en-passant capture                     */
#define MF_DOUBLE_PUSH 8   /* pawn two-square push                   */
#define MF_PROMOTION   16  /* pawn promotion                         */

typedef struct Move {
    int  fromRow, fromCol;
    int  toRow,   toCol;
    char piece;        /* moving piece                               */
    char captured;     /* captured piece, '.' if none                */
    char promoted;     /* promotion piece, '.' if none               */
    int  score;        /* move-ordering search score                 */
    int  flags;
    int  prevCastle;
    int  prevEpCol;
    int  prevHalfmove;
} Move;

static inline void move_clear(Move *m)
{
    m->fromRow = m->fromCol = m->toRow = m->toCol = 0;
    m->piece = '.';
    m->captured = '.';
    m->promoted = '.';
    m->score = 0;
    m->flags = 0;
    m->prevCastle = 0;
    m->prevEpCol = -1;
    m->prevHalfmove = 0;
}

/* ----------------------------------------------- Stack (LIFO) : UNDO ---- */
typedef struct StackNode {
    Move mv;
    struct StackNode *next;
} StackNode;

typedef struct UndoStack {
    StackNode *top;
    int size;
} UndoStack;

void stack_init(UndoStack *s);
int  stack_push(UndoStack *s, const Move *m);
int  stack_pop(UndoStack *s, Move *out);
int  stack_peek(const UndoStack *s, Move *out);
int  stack_is_empty(const UndoStack *s);
void stack_clear(UndoStack *s);

/* ------------------------------ Doubly linked list : MOVE HISTORY -------- */
typedef struct ListNode {
    Move mv;
    int  ply;
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct MoveList {
    ListNode *head;
    ListNode *tail;
    int size;
} MoveList;

void      list_init(MoveList *l);
int       list_append(MoveList *l, const Move *m);
int       list_remove_tail(MoveList *l, Move *out);
ListNode *list_find_move(const MoveList *l, const Move *m);
void      list_clear(MoveList *l);

/* ------------------------------------ Queue (FIFO) : EVENT PIPELINE ------ */
typedef enum EventType {
    EV_NONE = 0,
    EV_CLICK_SQUARE,
    EV_MOVE_REQUEST,
    EV_AI_MOVE,
    EV_UNDO,
    EV_NEW_GAME,
    EV_TOGGLE_MODE,
    EV_TOGGLE_HISTORY,
    EV_TOGGLE_QUEUE,
    EV_CYCLE_LEVEL,
    EV_TOGGLE_GLYPHS,
    EV_QUIT
} EventType;

typedef struct Event {
    int type;
    int r1, c1, r2, c2;
} Event;

typedef struct QueueNode {
    Event ev;
    struct QueueNode *next;
} QueueNode;

typedef struct EventQueue {
    QueueNode *head;
    QueueNode *tail;
    int size;
} EventQueue;

void queue_init(EventQueue *q);
int  queue_enqueue(EventQueue *q, const Event *e);
int  queue_dequeue(EventQueue *q, Event *out);
int  queue_is_empty(const EventQueue *q);
int  queue_size(const EventQueue *q);
int  queue_peek_at(const EventQueue *q, int index, Event *out);
void queue_clear(EventQueue *q);

/* ------------------- Chained hash table, FNV-1a 64-bit : REPETITION ------ */
typedef struct HashNode {
    uint64_t key;
    int count;
    struct HashNode *next;
} HashNode;

typedef struct HashTable {
    HashNode **buckets;
    size_t nbuckets;
    size_t count;
} HashTable;

uint64_t fnv1a_64(const void *data, size_t len);
int  ht_init(HashTable *t, size_t nbuckets);
int  ht_increment(HashTable *t, uint64_t key);
int  ht_decrement(HashTable *t, uint64_t key);
int  ht_get(const HashTable *t, uint64_t key);
void ht_clear(HashTable *t);
void ht_free(HashTable *t);

#ifdef __cplusplus
}
#endif

#endif /* DSA_H */