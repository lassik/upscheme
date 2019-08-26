#include <sys/types.h>

#include <mach-o/dyld.h>

#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "scheme.h"

char *get_exename(char *buf, size_t size)
{
    uint32_t bufsize = (uint32_t)size;
    if (_NSGetExecutablePath(buf, &bufsize))
        return NULL;
    return buf;
}
