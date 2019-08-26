#include <sys/types.h>

#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

extern void write_defaults_indent(struct ios *f, value_t v);

static value_t argv_list(int argc, char *argv[])
{
    int i;
    value_t lst = FL_NIL, temp;
    fl_gc_handle(&lst);
    fl_gc_handle(&temp);
    for (i = argc - 1; i >= 0; i--) {
        temp = cvalue_static_cstring(argv[i]);
        lst = fl_cons(temp, lst);
    }
    fl_free_gc_handles(2);
    return lst;
}

int main(int argc, char *argv[])
{
    fl_init(512 * 1024);
    {
        FL_TRY_EXTERN
        {
            if (fl_load_boot_image())
                return 1;

            (void)fl_applyn(1, symbol_value(symbol("__start")),
                            argv_list(argc, argv));
        }
        FL_CATCH_EXTERN
        {
            ios_puts("fatal error:\n", ios_stderr);
            write_defaults_indent(ios_stderr, fl_lasterror);
            ios_putc('\n', ios_stderr);
            return 1;
        }
    }
    return 0;
}
