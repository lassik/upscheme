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

static value_t argv_list(int argc, const char **argv)
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
"version   show version information"
"\n"
"help      show this help"
"\n";

const char *script_file;
value_t os_command_line;
int command_line_offset;

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

static const char **long_option(const char **argv, const char *option)
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
    } else if (!strcmp("version", name)) {
        versionflag = 1;
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

static const char **short_option(const char **argv, int option)
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

static const char **parse_command_line_flags(const char **argv)
{
    const char *arg;
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
    const char **command_line;

    command_line = parse_command_line_flags((const char **)(argv + 1));
    if (helpflag) {
        generic_usage(stdout, 0);
    }
    fl_init(512 * 1024);
    {
        fl_gc_handle(&os_command_line);
        os_command_line = argv_list(argc, (const char **)argv);
        command_line_offset = (command_line - (const char **)argv) / sizeof(*argv);
        FL_TRY_EXTERN
        {
            if (versionflag) {
                version();
            }
            if (fl_load_boot_image())
                return 1;
            script_file = command_line[0];
            if (script_file) {
                script_file = realpath(command_line[0], 0);
            }
            (void)fl_applyn(1, symbol_value(symbol("__start")),
                            os_command_line);
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
