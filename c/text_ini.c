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
#include "os.h"
#include "random.h"
#include "llt.h"

#include "flisp.h"

#include "error.h"

#include "argcount.h"
#include "buf.h"

static void push(value_t *tailp, value_t elt)
{
    value_t new_tail;

    new_tail = cdr_(*tailp) = fl_cons(elt, FL_NIL);
    *tailp = new_tail;
}

value_t builtin_read_ini_file(value_t *args, uint32_t nargs)
{
    static const char newline[] = "\n\r";
    static const char ends_var[] = "\n\r\"";
    static const char anywhite[] = " \t\n\r";
    static const char linewhite[] = " \t";
    static const char varname[] = "_ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    struct buf *b;
    struct ios *s;
    value_t head, tail, name, value;

    argcount("read-ini-file", nargs, 1);
    fl_toiostream(args[0], "read-ini-file");
    s = value2c(struct ios *, args[0]);
    b = buf_new();
    buf_put_ios(b, s);
    head = tail = fl_cons(FL_NIL, FL_NIL);
    for (;;) {
        buf_scan_while(b, anywhite);
        if (buf_scan_end(b)) {
            break;
        }
        if (buf_scan_byte(b, '#')) {
            buf_scan_while_not(b, newline);
            continue;
        }
        buf_scan_mark(b);
        buf_scan_while(b, varname);
        name = string_from_cstrn(b->bytes + b->mark, b->scan - b->mark);
        buf_scan_while(b, linewhite);
        if (!buf_scan_byte(b, '=')) {
            buf_scan_while_not(b, newline);
            continue;
        }
        buf_scan_while(b, linewhite);
        buf_scan_byte(b, '"');
        buf_scan_mark(b);
        buf_scan_while_not(b, ends_var);
        value = string_from_cstrn(b->bytes + b->mark, b->scan - b->mark);
        buf_scan_while(b, linewhite);
        push(&tail, fl_cons(name, value));
        if (!buf_scan_while(b, newline)) {
            buf_scan_while_not(b, newline);
            continue;
        }
    }
    return head;
}

value_t builtin_write_ini_file(value_t *args, uint32_t nargs)
{
    argcount("write-ini-file", nargs, 2);
    (void)args;
    return FL_NIL;
}
