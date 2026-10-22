/* ===========================================================================
 *  event_queue.c  -  FIFO queue (linked list) for move & UI event pipeline.
 *  enqueue / dequeue run in strict O(1).
 * =========================================================================== */
#include "dsa.h"
#include <stdlib.h>

void queue_init(EventQueue *q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

int queue_enqueue(EventQueue *q, const Event *e)
{
    QueueNode *n = (QueueNode *)malloc(sizeof(QueueNode));
    if (!n) return 0;
    n->ev = *e;
    n->next = NULL;
    if (q->tail) q->tail->next = n;
    else         q->head = n;
    q->tail = n;
    q->size++;
    return 1;
}

int queue_dequeue(EventQueue *q, Event *out)
{
    QueueNode *n;
    if (!q->head) return 0;
    n = q->head;
    if (out) *out = n->ev;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    free(n);
    q->size--;
    return 1;
}

int queue_is_empty(const EventQueue *q)
{
    return q->head == NULL;
}

int queue_size(const EventQueue *q)
{
    return q->size;
}

int queue_peek_at(const EventQueue *q, int index, Event *out)
{
    QueueNode *cur;
    int i = 0;
    if (index < 0 || index >= q->size) return 0;
    for (cur = q->head; cur; cur = cur->next, i++) {
        if (i == index) {
            if (out) *out = cur->ev;
            return 1;
        }
    }
    return 0;
}

void queue_clear(EventQueue *q)
{
    while (q->head) queue_dequeue(q, NULL);
}