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

const char *env_get_os_name(void) { return "windows"; }

value_t builtin_environment_stack(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("environment-stack", nargs, 0);
    return FL_NIL;
}
