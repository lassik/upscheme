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

#define VERSION_STRING "0.1.0"

static void version(void)
{
    printf("(version \"%s\")\n", VERSION_STRING);
    printf("(canonical-command \"upscheme\")\n");
    printf("(scheme-id upscheme)\n");
    printf("(languages scheme r7rs)\n");
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
            usage();
        boot_env = BOOT_ENV_NULL;
    } else if (!strcmp("r7rs", name)) {
        if (value)
            usage();
        boot_env = BOOT_ENV_R7RS;
    } else if (!strcmp("unstable", name)) {
        if (value)
            usage();
        boot_env = BOOT_ENV_UNSTABLE;
    } else if (!strcmp("debug", name)) {
        if (!value)
            usage();
    } else if (!strcmp("search", name)) {
        if (!value)
            usage();
    } else {
        usage();
    }
}

static void runtime_options(const char *arg)
{
    char *whole;
    char *name;
    char *value;
    char *limit;

    if (!(name = whole = strdup(arg))) {
        usage();  // TODO: out of memory
    }
    while (name[0]) {
        if (!(limit = strchr(name, ','))) {
            limit = strchr(name, 0);
        }
        if ((value = strchr(name, '='))) {
            if (value < limit) {
                *value++ = 0;
            } else {
                value = 0;
            }
        }
        runtime_option(name, value);
        name = limit;
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
    case ':':
        if (!argv[0])
            usage();
        runtime_options(*argv++);
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
    char **newargv;

    newargv = parse_command_line_flags(argv + 1);
    if (helpflag) {
        generic_usage(stdout, 0);
    } else if (versionflag) {
        version();
    }
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
