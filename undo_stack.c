/* ===========================================================================
 *  undo_stack.c  -  LIFO stack (singly linked list) used for UNDO.
 *  push / pop / peek run in strict O(1).
 * =========================================================================== */
#include "dsa.h"
#include <stdlib.h>

void stack_init(UndoStack *s)
{
    s->top = NULL;
    s->size = 0;
}

int stack_push(UndoStack *s, const Move *m)
{
    StackNode *n = (StackNode *)malloc(sizeof(StackNode));
    if (!n) return 0;
    n->mv = *m;
    n->next = s->top;
    s->top = n;
    s->size++;
    return 1;
}

int stack_pop(UndoStack *s, Move *out)
{
    StackNode *n;
    if (!s->top) return 0;
    n = s->top;
    if (out) *out = n->mv;
    s->top = n->next;
    free(n);
    s->size--;
    return 1;
}

int stack_peek(const UndoStack *s, Move *out)
{
    if (!s->top) return 0;
    if (out) *out = s->top->mv;
    return 1;
}

int stack_is_empty(const UndoStack *s)
{
    return s->top == NULL;
}

void stack_clear(UndoStack *s)
{
    while (s->top) stack_pop(s, NULL);
}