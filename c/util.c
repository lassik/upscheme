#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

static void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
    return realloc(ptr, nmemb * size);
}

//

void accum_elt(struct accum *accum, value_t elt)
{
    value_t newtail;

    newtail = fl_cons(elt, FL_NIL);
    if (accum->tail != FL_NIL) {
        cdr_(accum->tail) = newtail;
    } else {
        accum->list = newtail;
    }
    accum->tail = newtail;
}

void accum_pair(struct accum *accum, value_t a, value_t d)
{
    accum_elt(accum, fl_cons(a, d));
}

void accum_name_value(struct accum *accum, const char *name, value_t value)
{
    accum_pair(accum, string_from_cstr(name), value);
}

//

void sv_accum_init(struct sv_accum *accum)
{
    accum->fill = 0;
    accum->cap = 8;
    accum->vec = calloc(accum->cap, sizeof(char *));
    if (!accum->vec) {
        lerror(MemoryError, "out of memory");
    }
}

void sv_accum_strdup(struct sv_accum *accum, const char *str)
{
    char *dup;

    if (__unlikely(accum->cap <= accum->fill + 1)) {
        while (accum->cap <= accum->fill + 1) {
            accum->cap *= 2;
        }
        accum->vec = reallocarray(accum->vec, accum->cap, sizeof(char *));
        if (!accum->vec) {
            lerror(MemoryError, "out of memory");
        }
    }
    dup = strdup(str);
    if (!dup) {
        lerror(MemoryError, "out of memory");
    }
    accum->vec[accum->fill++] = dup;
    accum->vec[accum->fill] = 0;
}
