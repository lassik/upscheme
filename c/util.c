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
