/* ===========================================================================
 *  move_history.c  -  Doubly linked list holding the chronological game record.
 *  Tail pointer makes append / remove-last O(1); search is linear O(n).
 * =========================================================================== */
#include "dsa.h"
#include <stdlib.h>

void list_init(MoveList *l)
{
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
}

int list_append(MoveList *l, const Move *m)
{
    ListNode *n = (ListNode *)malloc(sizeof(ListNode));
    if (!n) return 0;
    n->mv = *m;
    n->next = NULL;
    n->prev = l->tail;
    n->ply = l->size + 1;
    if (l->tail) l->tail->next = n;
    else         l->head = n;
    l->tail = n;
    l->size++;
    return 1;
}

int list_remove_tail(MoveList *l, Move *out)
{
    ListNode *n = l->tail;
    if (!n) return 0;
    if (out) *out = n->mv;
    l->tail = n->prev;
    if (l->tail) l->tail->next = NULL;
    else         l->head = NULL;
    free(n);
    l->size--;
    return 1;
}

ListNode *list_find_move(const MoveList *l, const Move *m)
{
    ListNode *cur;
    for (cur = l->head; cur; cur = cur->next) {
        if (cur->mv.fromRow == m->fromRow && cur->mv.fromCol == m->fromCol &&
            cur->mv.toRow   == m->toRow   && cur->mv.toCol   == m->toCol   &&
            cur->mv.piece   == m->piece)
            return cur;
    }
    return NULL;
}

void list_clear(MoveList *l)
{
    while (l->tail) list_remove_tail(l, NULL);
}