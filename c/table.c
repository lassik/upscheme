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

#include "equalhash.h"

static value_t tablesym;
static struct fltype *tabletype;

void print_htable(value_t v, struct ios *f)
{
    struct htable *h = (struct htable *)cv_data((struct cvalue *)ptr(v));
    size_t i;
    int first = 1;
    fl_print_str("#table(", f);
    for (i = 0; i < h->size; i += 2) {
        if (h->table[i + 1] != HT_NOTFOUND) {
            if (!first)
                fl_print_str("  ", f);
            fl_print_child(f, (value_t)h->table[i]);
            fl_print_chr(' ', f);
            fl_print_child(f, (value_t)h->table[i + 1]);
            first = 0;
        }
    }
    fl_print_chr(')', f);
}

void print_traverse_htable(value_t self)
{
    struct htable *h = (struct htable *)cv_data((struct cvalue *)ptr(self));
    size_t i;
    for (i = 0; i < h->size; i += 2) {
        if (h->table[i + 1] != HT_NOTFOUND) {
            print_traverse((value_t)h->table[i]);
            print_traverse((value_t)h->table[i + 1]);
        }
    }
}

void free_htable(value_t self)
{
    struct htable *h = (struct htable *)cv_data((struct cvalue *)ptr(self));
    htable_free(h);
}

void relocate_htable(value_t oldv, value_t newv)
{
    size_t i;
    struct htable *oldh;
    struct htable *h;

    oldh = (struct htable *)cv_data((struct cvalue *)ptr(oldv));
    h = (struct htable *)cv_data((struct cvalue *)ptr(newv));
    if (oldh->table == &oldh->_space[0])
        h->table = &h->_space[0];
    for (i = 0; i < h->size; i++) {
        if (h->table[i] != HT_NOTFOUND)
            h->table[i] = (void *)relocate_lispvalue((value_t)h->table[i]);
    }
}

struct cvtable table_vtable = { print_htable, relocate_htable, free_htable,
                                print_traverse_htable };

int ishashtable(value_t v)
{
    return iscvalue(v) && cv_class((struct cvalue *)ptr(v)) == tabletype;
}

value_t fl_tablep(value_t *args, uint32_t nargs)
{
    argcount("table?", nargs, 1);
    return ishashtable(args[0]) ? FL_T : FL_F;
}

static struct htable *totable(value_t v, char *fname)
{
    if (!ishashtable(v))
        type_error(fname, "table", v);
    return (struct htable *)cv_data((struct cvalue *)ptr(v));
}

value_t fl_table(value_t *args, uint32_t nargs)
{
    struct htable *h;
    value_t nt, k, arg;
    size_t cnt;
    uint32_t i;

    cnt = (size_t)nargs;
    if (cnt & 1)
        lerror(ArgError, "table: arguments must come in pairs");
    // prevent small tables from being added to finalizer list
    if (cnt <= HT_N_INLINE) {
        tabletype->vtable->finalize = NULL;
        nt = cvalue(tabletype, sizeof(struct htable));
        tabletype->vtable->finalize = free_htable;
    } else {
        nt = cvalue(tabletype, 2 * sizeof(void *));
    }
    h = (struct htable *)cv_data((struct cvalue *)ptr(nt));
    htable_new(h, cnt / 2);
    k = FL_NIL;
    arg = FL_NIL;
    FOR_ARGS(i, 0, arg, args)
    {
        if (i & 1)
            equalhash_put(h, (void *)k, (void *)arg);
        else
            k = arg;
    }
    return nt;
}

// (put! table key value)
value_t fl_table_put(value_t *args, uint32_t nargs)
{
    struct htable *h;
    void **table0;

    argcount("put!", nargs, 3);
    h = totable(args[0], "put!");
    table0 = h->table;
    equalhash_put(h, (void *)args[1], (void *)args[2]);
    // register finalizer if we outgrew inline space
    if (table0 == &h->_space[0] && h->table != &h->_space[0]) {
        struct cvalue *cv = (struct cvalue *)ptr(args[0]);
        add_finalizer(cv);
        cv->len = 2 * sizeof(void *);
    }
    return args[0];
}

static void key_error(char *fname, value_t key)
{
    lerrorf(fl_list2(KeyError, key), "%s: key not found", fname);
}

// (get table key [default])
value_t fl_table_get(value_t *args, uint32_t nargs)
{
    struct htable *h;
    value_t v;

    if (nargs != 3)
        argcount("get", nargs, 2);
    h = totable(args[0], "get");
    v = (value_t)equalhash_get(h, (void *)args[1]);
    if (v == (value_t)HT_NOTFOUND) {
        if (nargs == 3)
            return args[2];
        key_error("get", args[1]);
    }
    return v;
}

// (has? table key)
value_t fl_table_has(value_t *args, uint32_t nargs)
{
    struct htable *h;

    argcount("has", nargs, 2);
    h = totable(args[0], "has");
    return equalhash_has(h, (void *)args[1]) ? FL_T : FL_F;
}

// (del! table key)
value_t fl_table_del(value_t *args, uint32_t nargs)
{
    struct htable *h;

    argcount("del!", nargs, 2);
    h = totable(args[0], "del!");
    if (!equalhash_remove(h, (void *)args[1]))
        key_error("del!", args[1]);
    return args[0];
}

value_t fl_table_foldl(value_t *args, uint32_t nargs)
{
    struct htable *h;
    void **table;
    size_t i, n;
    value_t f, zero, t;

    argcount("table.foldl", nargs, 3);
    f = args[0];
    zero = args[1];
    t = args[2];
    h = totable(t, "table.foldl");
    n = h->size;
    table = h->table;
    fl_gc_handle(&f);
    fl_gc_handle(&zero);
    fl_gc_handle(&t);
    for (i = 0; i < n; i += 2) {
        if (table[i + 1] != HT_NOTFOUND) {
            zero =
            fl_applyn(3, f, (value_t)table[i], (value_t)table[i + 1], zero);
            // reload pointer
            h = (struct htable *)cv_data((struct cvalue *)ptr(t));
            if (h->size != n)
                lerror(EnumerationError, "table.foldl: table modified");
            table = h->table;
        }
    }
    fl_free_gc_handles(3);
    return zero;
}

static struct builtinspec tablefunc_info[] = { { "table", fl_table },
                                               { "table?", fl_tablep },
                                               { "put!", fl_table_put },
                                               { "get", fl_table_get },
                                               { "has?", fl_table_has },
                                               { "del!", fl_table_del },
                                               { "table.foldl",
                                                 fl_table_foldl },
                                               { NULL, NULL } };

void table_init(void)
{
    tablesym = symbol("table");
    tabletype =
    define_opaque_type(tablesym, sizeof(struct htable), &table_vtable, NULL);
    assign_global_builtins(tablefunc_info);
}
