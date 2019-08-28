#include <sys/types.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>

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

static const struct utsname *get_global_uname(void)
{
    static struct utsname buf;

    if (!buf.sysname[0]) {
        if (uname(&buf) == -1) {
            memset(&buf, 0, sizeof(buf));
        }
    }
    return &buf;
}

const char *env_get_os_name(void) { return get_global_uname()->sysname; }

static value_t envst_language(void)
{
    struct accum acc = ACCUM_EMPTY;

    accum_elt(&acc, symbol("language"));
    accum_name_value(&acc, "implementation-name",
                     string_from_cstr("Up Scheme"));
    accum_name_value(&acc, "implementation-version",
                     string_from_cstr("0.1.0"));
    return acc.list;
}

static value_t envst_language_c(void)
{
    struct accum acc = ACCUM_EMPTY;

    accum_elt(&acc, symbol("language"));
    accum_name_value(&acc, "implementation-name",
                     string_from_cstr(SCHEME_C_COMPILER_NAME));
    accum_name_value(&acc, "implementation-version",
                     string_from_cstr(SCHEME_C_COMPILER_VERSION));
    return acc.list;
}

static value_t envst_os(void)
{
    struct accum acc = ACCUM_EMPTY;

    accum_elt(&acc, symbol("os"));
    accum_name_value(&acc, "implementation-name",
                     string_from_cstr(get_global_uname()->sysname));
    accum_name_value(&acc, "implementation-version",
                     string_from_cstr(get_global_uname()->release));
    return acc.list;
}

static const char endianness[] =
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
"big-endian"
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
"little-endian"
#endif
;

static value_t envst_computer(void)
{
    struct accum acc = ACCUM_EMPTY;

    accum_elt(&acc, symbol("computer"));
    accum_name_value(&acc, "architecture",
                     string_from_cstr(get_global_uname()->machine));
    accum_name_value(&acc, "cpu-bits", fixnum(sizeof(uintptr_t) * CHAR_BIT));
    accum_name_value(&acc, "byte-order", symbol(endianness));
    return acc.list;
}

value_t builtin_environment_stack(value_t *args, uint32_t nargs)
{
    struct accum acc = ACCUM_EMPTY;

    (void)args;
    argcount("environment-stack", nargs, 0);
    accum_elt(&acc, envst_language());
    accum_elt(&acc, envst_language_c());
    accum_elt(&acc, envst_os());
    accum_elt(&acc, envst_computer());
    return acc.list;
}
