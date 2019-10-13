/*
  functions common to all hash table instantiations
*/

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

struct htable *htable_new(struct htable *h, size_t size)
{
    size_t i;

    if (size <= HT_N_INLINE / 2) {
        h->size = size = HT_N_INLINE;
        h->table = &h->_space[0];
    } else {
        size = nextipow2(size);
        size *= 2;  // 2 pointers per key/value pair
        size *= 2;  // aim for 50% occupancy
        h->size = size;
        h->table = (void **)malloc(size * sizeof(void *));
    }
    if (h->table == NULL)
        return NULL;
    for (i = 0; i < size; i++)
        h->table[i] = HT_NOTFOUND;
    return h;
}

void htable_free(struct htable *h)
{
    if (h->table != &h->_space[0])
        free(h->table);
}

// empty and reduce size
void htable_reset(struct htable *h, size_t sz)
{
    size_t i, hsz;

    sz = nextipow2(sz);
    if (h->size > sz * 4 && h->size > HT_N_INLINE) {
        size_t newsz = sz * 4;
        void **newtab = (void **)realloc(h->table, newsz * sizeof(void *));
        if (newtab == NULL)
            return;
        h->size = newsz;
        h->table = newtab;
    }
    hsz = h->size;
    for (i = 0; i < hsz; i++)
        h->table[i] = HT_NOTFOUND;
}
