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

#define BOOT_ENV_NULL 0
#define BOOT_ENV_R7RS 1
#define BOOT_ENV_UNSTABLE 2

static value_t get_features_list(void)
{
    static struct accum acc;
    static int initialized;

    if (!initialized) {
        initialized = 1;
        accum_init(&acc);
#ifdef BITS64
        accum_elt(&acc, symbol("64-bit"));
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        accum_elt(&acc, symbol("big-endian"));
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        accum_elt(&acc, symbol("little-endian"));
#endif
        accum_elt(&acc, symbol("r7rs"));
    }
    return acc.list;
}

static value_t get_version_alist(void)
{
    static struct accum acc;
    static int initialized;

    if (!initialized) {
        initialized = 1;
        accum_init(&acc);
        accum_name_value1(&acc, "command", string_from_cstr("upscheme"));
        accum_name_value1(&acc, "scheme-id", symbol("upscheme"));
        accum_name_value(
        &acc, "language",
        fl_cons(symbol("scheme"), fl_cons(symbol("r7rs"), FL_NIL)));
        accum_name_value(&acc, "features", get_features_list());
    }
    return acc.list;
}

value_t builtin_features(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("features", nargs, 0);
    return get_features_list();
}

value_t builtin_version_alist(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("version-alist", nargs, 0);
    return get_version_alist();
}

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

static const char usage_message[] =
"usage: upscheme -h|--help"
"\n"
"usage: upscheme -V|--version"
"\n"
"usage: upscheme [options]"
"\n"
"usage: upscheme [options] -e expression..."
"\n"
"usage: upscheme [options] script-file [script-arg...]"
"\n";

static const char runtime_usage_message[] =
"usage: upscheme [-:option,option...] ..."
"\n"
"The -: flag sets Up Scheme runtime options. An option is one of:"
"\n"
"\n"
"null      start in an environment with only import and cond-expand"
"\n"
"r7rs      start in R7RS environment with no Up Scheme extensions"
"\n"
"unstable  start with the very latest in-development Up Scheme extensions"
"\n"
"debug     set debugging options"
"\n"
"search    set module search path"
"\n"
"help      show this help"
"\n";

static int evalflag;
static int helpflag;
static int versionflag;
static int boot_env;

static void generic_usage(FILE *out, int status)
{
    fprintf(out, "%s", usage_message);
    exit(status);
}

static void usage(void) { generic_usage(stderr, 2); }

static void generic_runtime_usage(FILE *out, int status)
{
    fprintf(out, "%s", runtime_usage_message);
    exit(status);
}

static void runtime_usage(void) { generic_runtime_usage(stderr, 2); }

static void version(void)
{
    value_t list;

    for (list = get_version_alist(); iscons(list); list = cdr_(list)) {
        write_simple_defaults(ios_stdout, car_(list));
        ios_putc('\n', ios_stdout);
    }
    exit(0);
}

static char **long_option(char **argv, const char *option)
{
    if (!strcmp(option, "--help")) {
        helpflag = 1;
    } else if (!strcmp(option, "--version")) {
        versionflag = 1;
    } else {
        usage();
    }
    return argv;
}

static void runtime_option(const char *name, const char *value)
{
    if (!strcmp("null", name)) {
        if (value)
            runtime_usage();
        boot_env = BOOT_ENV_NULL;
    } else if (!strcmp("r7rs", name)) {
        if (value)
            runtime_usage();
        boot_env = BOOT_ENV_R7RS;
    } else if (!strcmp("unstable", name)) {
        if (value)
            runtime_usage();
        boot_env = BOOT_ENV_UNSTABLE;
    } else if (!strcmp("debug", name)) {
        if (!value)
            runtime_usage();
    } else if (!strcmp("search", name)) {
        if (!value)
            runtime_usage();
    } else if (!strcmp("help", name)) {
        generic_runtime_usage(stdout, 0);
    } else {
        runtime_usage();
    }
}

static void runtime_options(const char *arg)
{
    char *name;
    char *value;
    char *whole;
    char *limit;

    if (!(whole = strdup(arg))) {
        runtime_usage();  // TODO: out of memory
    }
    for (name = whole; name; name = limit) {
        if ((limit = strchr(name, ','))) {
            *limit++ = 0;
        }
        if ((value = strchr(name, '='))) {
            *value++ = 0;
        }
        if (*name) {
            runtime_option(name, value);
        } else if (value) {
            runtime_usage();
        }
    }
    free(whole);
}

static char **short_option(char **argv, int option)
{
    switch (option) {
    case 'e':
        evalflag = 1;
        break;
    case 'h':
        helpflag = 1;
        break;
    case 'V':
        versionflag = 1;
        break;
    default:
        usage();
        break;
    }
    return argv;
}

static char **parse_command_line_flags(char **argv)
{
    char *arg;
    int option;

    while ((arg = *argv)) {
        if (arg[0] == '-') {
            if (arg[1] == '-') {
                if (arg[2] == '-') {
                    usage();
                } else if (!arg[2]) {
                    break;
                } else {
                    argv++;
                    argv = long_option(argv, arg);
                }
            } else if (arg[1] == ':') {
                argv++;
                runtime_options(&arg[2]);
            } else if (!arg[1]) {
                break;
            } else {
                argv++;
                for (arg++; (option = *arg); arg++) {
                    argv = short_option(argv, option);
                }
            }
        } else {
            break;
        }
    }
    return argv;
}

int main(int argc, char **argv)
{
    parse_command_line_flags(argv + 1);
    if (helpflag) {
        generic_usage(stdout, 0);
    }
    fl_init(512 * 1024);
    {
        FL_TRY_EXTERN
        {
            if (versionflag) {
                version();
            }
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