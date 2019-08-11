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

#include "dtypes.h"
#include "utils.h"
#include "utf8.h"
#include "ios.h"
#include "socket.h"
#include "timefuncs.h"
#include "hashing.h"
#include "htable.h"
#include "htableh_inc.h"
#include "bitvector.h"
#include "fs.h"
#include "random.h"
#include "llt.h"

#include "flisp.h"

#include "argcount.h"

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

static void push(value_t *tailp, value_t elt)
{
    value_t new_tail;

    new_tail = cdr_(*tailp) = fl_cons(elt, FL_NIL);
    *tailp = new_tail;
}

static void push_pair(value_t *tailp, const char *name, value_t value)
{
    push(tailp, fl_cons(symbol(name), value));
}

static value_t envst_language(void)
{
    value_t head, tail;

    head = tail = fl_cons(symbol("language"), FL_NIL);
    push_pair(&tail, "implementation-name", string_from_cstr("Up Scheme"));
    push_pair(&tail, "implementation-version", string_from_cstr("0.1.0"));
    return head;
}

static value_t envst_os(void)
{
    value_t head, tail;

    head = tail = fl_cons(symbol("os"), FL_NIL);
    push_pair(&tail, "implementation-name",
              string_from_cstr(get_global_uname()->sysname));
    push_pair(&tail, "implementation-version",
              string_from_cstr(get_global_uname()->release));
    return head;
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
    value_t head, tail;

    head = tail = fl_cons(symbol("computer"), FL_NIL);
    push_pair(&tail, "architecture",
              string_from_cstr(get_global_uname()->machine));
    push_pair(&tail, "cpu-bits", fixnum(sizeof(uintptr_t) * CHAR_BIT));
    push_pair(&tail, "byte-order", symbol(endianness));
    return head;
}

value_t builtin_environment_stack(value_t *args, uint32_t nargs)
{
    value_t head, tail;

    (void)args;
    argcount("environment-stack", nargs, 0);

    head = tail = fl_cons(envst_language(), FL_NIL);
    push(&tail, envst_os());
    push(&tail, envst_computer());
    return head;
}
