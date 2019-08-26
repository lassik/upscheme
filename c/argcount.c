#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "scheme.h"

void argcount(const char *fname, uint32_t nargs, uint32_t c)
{
    if (__unlikely(nargs != c))
        lerrorf(ArgError, "%s: too %s arguments", fname,
                nargs < c ? "few" : "many");
}
