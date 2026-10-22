/* ===========================================================================
 *  board_hash.c  -  Separate-chaining hash table keyed by 64-bit FNV-1a hash.
 *  Detects 3-fold repetition in expected O(1) time.
 * =========================================================================== */
#include "dsa.h"
#include <stdlib.h>

uint64_t fnv1a_64(const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = 14695981039346656037ULL;
    size_t i;
    for (i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

int ht_init(HashTable *t, size_t nbuckets)
{
    if (nbuckets == 0) nbuckets = 1;
    t->buckets = (HashNode **)calloc(nbuckets, sizeof(HashNode *));
    t->nbuckets = t->buckets ? nbuckets : 0;
    t->count = 0;
    return t->buckets != NULL;
}

static HashNode *ht_find(const HashTable *t, uint64_t key)
{
    HashNode *n;
    if (!t->nbuckets) return NULL;
    for (n = t->buckets[key % t->nbuckets]; n; n = n->next)
        if (n->key == key) return n;
    return NULL;
}

int ht_increment(HashTable *t, uint64_t key)
{
    HashNode *n = ht_find(t, key);
    size_t b;
    if (n) return ++n->count;
    if (!t->nbuckets) return 0;
    n = (HashNode *)malloc(sizeof(HashNode));
    if (!n) return 0;
    b = (size_t)(key % t->nbuckets);
    n->key = key;
    n->count = 1;
    n->next = t->buckets[b];
    t->buckets[b] = n;
    t->count++;
    return 1;
}

int ht_decrement(HashTable *t, uint64_t key)
{
    HashNode *cur, *prev = NULL;
    size_t b;
    if (!t->nbuckets) return 0;
    b = (size_t)(key % t->nbuckets);
    for (cur = t->buckets[b]; cur; prev = cur, cur = cur->next) {
        if (cur->key == key) {
            cur->count--;
            if (cur->count <= 0) {
                if (prev) prev->next = cur->next;
                else      t->buckets[b] = cur->next;
                free(cur);
                t->count--;
                return 0;
            }
            return cur->count;
        }
    }
    return 0;
}

int ht_get(const HashTable *t, uint64_t key)
{
    HashNode *n = ht_find(t, key);
    return n ? n->count : 0;
}

void ht_clear(HashTable *t)
{
    size_t i;
    for (i = 0; i < t->nbuckets; i++) {
        HashNode *n = t->buckets[i];
        while (n) {
            HashNode *next = n->next;
            free(n);
            n = next;
        }
        t->buckets[i] = NULL;
    }
    t->count = 0;
}

void ht_free(HashTable *t)
{
    ht_clear(t);
    free(t->buckets);
    t->buckets = NULL;
    t->nbuckets = 0;
}