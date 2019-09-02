/*
  Extra femtoLisp builtin functions
*/

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

extern char **environ;

size_t llength(value_t v)
{
    size_t n = 0;
    while (iscons(v)) {
        n++;
        v = cdr_(v);
    }
    return n;
}

static value_t fl_nconc(value_t *args, uint32_t nargs)
{
    value_t lst, first;
    value_t *pcdr;
    struct cons *c;
    uint32_t i;

    if (nargs == 0)
        return FL_NIL;
    first = FL_NIL;
    pcdr = &first;
    i = 0;
    while (1) {
        lst = args[i++];
        if (i >= nargs)
            break;
        if (iscons(lst)) {
            *pcdr = lst;
            c = (struct cons *)ptr(lst);
            while (iscons(c->cdr))
                c = (struct cons *)ptr(c->cdr);
            pcdr = &c->cdr;
        } else if (lst != FL_NIL) {
            type_error("nconc", "cons", lst);
        }
    }
    *pcdr = lst;
    return first;
}

static value_t fl_assq(value_t *args, uint32_t nargs)
{
    value_t item;
    value_t v;
    value_t bind;

    argcount("assq", nargs, 2);
    item = args[0];
    v = args[1];
    while (iscons(v)) {
        bind = car_(v);
        if (iscons(bind) && car_(bind) == item)
            return bind;
        v = cdr_(v);
    }
    return FL_F;
}

static value_t fl_memq(value_t *args, uint32_t nargs)
{
    argcount("memq", nargs, 2);
    while (iscons(args[1])) {
        struct cons *c = (struct cons *)ptr(args[1]);
        if (c->car == args[0])
            return args[1];
        args[1] = c->cdr;
    }
    return FL_F;
}

static value_t fl_length(value_t *args, uint32_t nargs)
{
    value_t a;
    struct cvalue *cv;

    argcount("length", nargs, 1);
    a = args[0];
    if (isvector(a)) {
        return fixnum(vector_size(a));
    } else if (iscprim(a)) {
        cv = (struct cvalue *)ptr(a);
        if (cp_class(cv) == bytetype)
            return fixnum(1);
        else if (cp_class(cv) == wchartype)
            return fixnum(
            u8_charlen(*(uint32_t *)cp_data((struct cprim *)cv)));
    } else if (iscvalue(a)) {
        cv = (struct cvalue *)ptr(a);
        if (cv_class(cv)->eltype != NULL)
            return size_wrap(cvalue_arraylen(a));
    } else if (a == FL_NIL) {
        return fixnum(0);
    } else if (iscons(a)) {
        return fixnum(llength(a));
    }
    type_error("length", "sequence", a);
    return FL_NIL;  // TODO
}

static value_t fl_f_raise(value_t *args, uint32_t nargs)
{
    argcount("raise", nargs, 1);
    fl_raise(args[0]);
    return FL_NIL;  // TODO
}

static value_t fl_exit(value_t *args, uint32_t nargs)
{
    if (nargs > 0)
        exit(tofixnum(args[0], "exit"));
    exit(0);
    return FL_NIL;
}

static value_t fl_symbol(value_t *args, uint32_t nargs)
{
    argcount("symbol", nargs, 1);
    if (!fl_isstring(args[0]))
        type_error("symbol", "string", args[0]);
    return symbol(cvalue_data(args[0]));
}

static value_t fl_keywordp(value_t *args, uint32_t nargs)
{
    argcount("keyword?", nargs, 1);
    return (issymbol(args[0]) && iskeyword((struct symbol *)ptr(args[0])))
           ? FL_T
           : FL_F;
}

static value_t fl_top_level_value(value_t *args, uint32_t nargs)
{
    struct symbol *sym;

    argcount("top-level-value", nargs, 1);
    sym = tosymbol(args[0], "top-level-value");
    if (sym->binding == UNBOUND)
        fl_raise(fl_list2(UnboundError, args[0]));
    return sym->binding;
}

static value_t fl_set_top_level_value(value_t *args, uint32_t nargs)
{
    struct symbol *sym;

    argcount("set-top-level-value!", nargs, 2);
    sym = tosymbol(args[0], "set-top-level-value!");
    if (!isconstant(sym))
        sym->binding = args[1];
    return args[1];
}

static void global_env_list(struct symbol *root, value_t *pv)
{
    while (root != NULL) {
        if (root->name[0] != ':' && (root->binding != UNBOUND)) {
            *pv = fl_cons(tagptr(root, TAG_SYM), *pv);
        }
        global_env_list(root->left, pv);
        root = root->right;
    }
}

extern struct symbol *symtab;

value_t fl_global_env(value_t *args, uint32_t nargs)
{
    value_t lst;

    (void)args;
    argcount("environment", nargs, 0);
    lst = FL_NIL;
    fl_gc_handle(&lst);
    global_env_list(symtab, &lst);
    fl_free_gc_handles(1);
    return lst;
}

extern value_t QUOTE;

static value_t fl_constantp(value_t *args, uint32_t nargs)
{
    argcount("constant?", nargs, 1);
    if (issymbol(args[0]))
        return (isconstant((struct symbol *)ptr(args[0])) ? FL_T : FL_F);
    if (iscons(args[0])) {
        if (car_(args[0]) == QUOTE)
            return FL_T;
        return FL_F;
    }
    return FL_T;
}

static value_t fl_integer_valuedp(value_t *args, uint32_t nargs)
{
    value_t v;
    double d;
    void *data;

    argcount("integer-valued?", nargs, 1);
    v = args[0];
    if (isfixnum(v)) {
        return FL_T;
    } else if (iscprim(v)) {
        numerictype_t nt = cp_numtype((struct cprim *)ptr(v));
        if (nt < T_FLOAT)
            return FL_T;
        data = cp_data((struct cprim *)ptr(v));
        if (nt == T_FLOAT) {
            float f = *(float *)data;
            if (f < 0)
                f = -f;
            if (f <= FLT_MAXINT && (float)(int32_t)f == f)
                return FL_T;
        } else {
            assert(nt == T_DOUBLE);
            d = *(double *)data;
            if (d < 0)
                d = -d;
            if (d <= DBL_MAXINT && (double)(int64_t)d == d)
                return FL_T;
        }
    }
    return FL_F;
}

static value_t fl_integerp(value_t *args, uint32_t nargs)
{
    value_t v;

    argcount("integer?", nargs, 1);
    v = args[0];
    return (isfixnum(v) ||
            (iscprim(v) && cp_numtype((struct cprim *)ptr(v)) < T_FLOAT))
           ? FL_T
           : FL_F;
}

static value_t fl_fixnum(value_t *args, uint32_t nargs)
{
    argcount("fixnum", nargs, 1);
    if (isfixnum(args[0])) {
        return args[0];
    } else if (iscprim(args[0])) {
        struct cprim *cp = (struct cprim *)ptr(args[0]);
        return fixnum(conv_to_long(cp_data(cp), cp_numtype(cp)));
    }
    type_error("fixnum", "number", args[0]);
    return FL_NIL;  // TODO
}

static value_t fl_truncate(value_t *args, uint32_t nargs)
{
    argcount("truncate", nargs, 1);
    if (isfixnum(args[0]))
        return args[0];
    if (iscprim(args[0])) {
        struct cprim *cp = (struct cprim *)ptr(args[0]);
        void *data = cp_data(cp);
        numerictype_t nt = cp_numtype(cp);
        double d;
        if (nt == T_FLOAT)
            d = (double)*(float *)data;
        else if (nt == T_DOUBLE)
            d = *(double *)data;
        else
            return args[0];
        if (d > 0) {
            if (d > (double)U64_MAX)
                return args[0];
            return return_from_uint64((uint64_t)d);
        }
        if (d > (double)S64_MAX || d < (double)S64_MIN)
            return args[0];
        return return_from_int64((int64_t)d);
    }
    type_error("truncate", "number", args[0]);
    return FL_NIL;  // TODO
}

static value_t fl_vector_alloc(value_t *args, uint32_t nargs)
{
    fixnum_t i;
    value_t f, v;
    int k;

    if (nargs == 0)
        lerror(ArgError, "vector.alloc: too few arguments");
    i = (fixnum_t)toulong(args[0], "vector.alloc");
    if (i < 0)
        lerror(ArgError, "vector.alloc: invalid size");
    v = alloc_vector((unsigned)i, 0);
    if (nargs == 2)
        f = args[1];
    else
        f = FL_UNSPECIFIED;
    for (k = 0; k < i; k++)
        vector_elt(v, k) = f;
    return v;
}

static double todouble(value_t a, char *fname)
{
    if (isfixnum(a))
        return (double)numval(a);
    if (iscprim(a)) {
        struct cprim *cp = (struct cprim *)ptr(a);
        numerictype_t nt = cp_numtype(cp);
        return conv_to_double(cp_data(cp), nt);
    }
    type_error(fname, "number", a);
    return FL_NIL;  // TODO
}

static value_t fl_path_cwd(value_t *args, uint32_t nargs)
{
    char *ptr;

    if (nargs > 1)
        argcount("path.cwd", nargs, 1);
    if (nargs == 0) {
        char buf[1024];
        get_cwd(buf, sizeof(buf));
        return string_from_cstr(buf);
    }
    ptr = tostring(args[0], "path.cwd");
    if (set_cwd(ptr))
        lerrorf(IOError, "path.cwd: could not cd to %s", ptr);
    return FL_T;
}

value_t builtin_file_exists(value_t *args, uint32_t nargs)
{
    char *str;

    argcount("file-exists?", nargs, 1);
    str = tostring(args[0], "file-exists?");
    return os_path_exists(str) ? FL_T : FL_F;
}

value_t builtin_get_environment_variables(value_t *args, uint32_t nargs)
{
    struct accum acc = ACCUM_EMPTY;
    char **pairs;
    const char *pair;
    const char *pivot;
    value_t name, value;

    (void)args;
    argcount("get-environment-variables", nargs, 0);
    for (pairs = environ; (pair = *pairs); pairs++) {
        pivot = strchr(pair, '=');
        if (!pivot) {
            continue;
        }
        name = string_from_cstrn(pair, pivot - pair);
        value = string_from_cstr(pivot + 1);
        accum_pair(&acc, name, value);
    }
    return acc.list;
}

value_t builtin_get_environment_variable(value_t *args, uint32_t nargs)
{
    char *name;
    char *val;

    argcount("get-environment-variable", nargs, 1);
    name = tostring(args[0], "get-environment-variable");
    val = getenv(name);
    if (val == NULL)
        return FL_F;
    if (*val == 0)
        return symbol_value(emptystringsym);
    return cvalue_static_cstring(val);
}

value_t builtin_set_environment_variable(value_t *args, uint32_t nargs)
{
    const char *name;
    const char *value;

    argcount("set-environment-variable", nargs, 2);
    name = tostring(args[0], "set-environment-variable");
    if (args[1] == FL_F) {
        value = 0;
    } else {
        value = tostring(args[1], "set-environment-variable");
    }
    os_setenv(name, value);
    return FL_T;
}

static value_t fl_rand(value_t *args, uint32_t nargs)
{
    fixnum_t r;

    (void)args;
    (void)nargs;
#ifdef BITS64
    r = ((((uint64_t)random()) << 32) | random()) & 0x1fffffffffffffffLL;
#else
    r = random() & 0x1fffffff;
#endif
    return fixnum(r);
}

static value_t fl_rand32(value_t *args, uint32_t nargs)
{
    uint32_t r;

    (void)args;
    (void)nargs;
    r = random();
#ifdef BITS64
    return fixnum(r);
#else
    return mk_uint32(r);
#endif
}

static value_t fl_rand64(value_t *args, uint32_t nargs)
{
    uint64_t r;

    (void)args;
    (void)nargs;
    r = (((uint64_t)random()) << 32) | random();
    return mk_uint64(r);
}

static value_t fl_randd(value_t *args, uint32_t nargs)
{
    (void)args;
    (void)nargs;
    return mk_double(rand_double());
}

static value_t fl_randf(value_t *args, uint32_t nargs)
{
    (void)args;
    (void)nargs;
    return mk_float(rand_float());
}

#define MATH_FUNC_1ARG(name)                                 \
    static value_t fl_##name(value_t *args, uint32_t nargs)  \
    {                                                        \
        argcount(#name, nargs, 1);                           \
        if (iscprim(args[0])) {                              \
            struct cprim *cp = (struct cprim *)ptr(args[0]); \
            numerictype_t nt = cp_numtype(cp);               \
            if (nt == T_FLOAT) {                             \
                float f = *(float *)cp_data(cp);             \
                return mk_float(name((double)f));            \
            }                                                \
        }                                                    \
        return mk_double(name(todouble(args[0], #name)));    \
    }

MATH_FUNC_1ARG(sqrt)
MATH_FUNC_1ARG(exp)
MATH_FUNC_1ARG(log)
MATH_FUNC_1ARG(sin)
MATH_FUNC_1ARG(cos)
MATH_FUNC_1ARG(tan)
MATH_FUNC_1ARG(asin)
MATH_FUNC_1ARG(acos)
MATH_FUNC_1ARG(atan)

static const char help_text[] =
""
"----------------------------------------------------------------------\n"
"You are in Up Scheme.\n"
"Type (exit) to exit.\n"
"----------------------------------------------------------------------\n";

static value_t builtin_help_star(value_t *args, uint32_t nargs)
{
    (void)args;
    (void)nargs;
    ios_puts(help_text, ios_stdout);
    return FL_T;
}

extern void stringfuncs_init(void);
extern void table_init(void);
extern void iostream_init(void);
extern void print_init(void);

static struct builtinspec builtin_info[] = {
    { "environment", fl_global_env },
    { "constant?", fl_constantp },
    { "top-level-value", fl_top_level_value },
    { "set-top-level-value!", fl_set_top_level_value },
    { "raise", fl_f_raise },
    { "exit", fl_exit },
    { "symbol", fl_symbol },
    { "keyword?", fl_keywordp },

    { "fixnum", fl_fixnum },
    { "truncate", fl_truncate },
    { "integer?", fl_integerp },
    { "integer-valued?", fl_integer_valuedp },
    { "nconc", fl_nconc },
    { "append!", fl_nconc },
    { "assq", fl_assq },
    { "memq", fl_memq },
    { "length", fl_length },

    { "vector.alloc", fl_vector_alloc },

    { "rand", fl_rand },
    { "rand.uint32", fl_rand32 },
    { "rand.uint64", fl_rand64 },
    { "rand.double", fl_randd },
    { "rand.float", fl_randf },

    { "sqrt", fl_sqrt },
    { "exp", fl_exp },
    { "log", fl_log },
    { "sin", fl_sin },
    { "cos", fl_cos },
    { "tan", fl_tan },
    { "asin", fl_asin },
    { "acos", fl_acos },
    { "atan", fl_atan },

    { "path.cwd", fl_path_cwd },

    { "help*", builtin_help_star },

    { "import-procedure", builtin_import },

    { NULL, NULL }
};

void builtins_init(void)
{
    assign_global_builtins(builtin_info);
    stringfuncs_init();
    table_init();
    iostream_init();
    print_init();
    os_init();
}
