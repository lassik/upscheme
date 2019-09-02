#include <sys/types.h>

#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "scheme.h"

static value_t dirsym;
struct fltype *dirtype;

void path_to_dirname(char *path)
{
    char *p;

    if (!(p = strrchr(path, '/'))) {
        p = path;
    }
    *p = '\0';
}

void get_cwd(char *buf, size_t size)
{
    if (!getcwd(buf, size)) {
        memset(buf, 0, size);
    }
}

int set_cwd(char *buf)
{
    if (chdir(buf) == -1)
        return 1;
    return 0;
}

value_t builtin_pid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("pid", nargs, 0);
    return fixnum(getpid());
}

value_t builtin_parent_pid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("parent-pid", nargs, 0);
    return fixnum(getppid());
}

value_t builtin_process_group(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("process-group", nargs, 0);
    return fixnum(getpgid(0));
}

value_t builtin_user_effective_gid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-effective-gid", nargs, 0);
    return fixnum(getegid());
}

value_t builtin_user_effective_uid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-effective-uid", nargs, 0);
    return fixnum(geteuid());
}

value_t builtin_user_real_gid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-real-gid", nargs, 0);
    return fixnum(getgid());
}

value_t builtin_user_real_uid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-real-uid", nargs, 0);
    return fixnum(getuid());
}

int os_path_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) != -1;
}

void os_setenv(const char *name, const char *value)
{
    if (value) {
        if (setenv(name, value, 1) != 0) {
            lerror(ArgError, "os.setenv: cannot set environment variable");
        }
    } else {
        if (unsetenv(name) != 0) {
            lerror(ArgError, "os.setenv: cannot unset environment variable");
        }
    }
}

int isdirvalue(value_t v)
{
    return iscvalue(v) && cv_class((struct cvalue *)ptr(v)) == dirtype;
}

static DIR **todirhandleptr(value_t v, const char *fname)
{
    if (!isdirvalue(v))
        type_error(fname, "dir", v);
    return value2c(DIR **, v);
}

value_t builtin_os_open_directory(value_t *args, uint32_t nargs)
{
    const char *path;
    DIR *dirhandle;
    DIR **dirhandleptr;
    value_t dirvalue;

    argcount("open-directory", nargs, 1);
    path = tostring(args[0], "path");
    if (!(dirhandle = opendir(path))) {
        lerror(IOError, "cannot open directory");
    }
    dirvalue = cvalue(dirtype, sizeof(DIR *));
    dirhandleptr = value2c(DIR **, dirvalue);
    *dirhandleptr = dirhandle;
    return dirvalue;
}

value_t builtin_os_read_directory(value_t *args, uint32_t nargs)
{
    DIR *dirhandle;
    DIR **dirhandleptr;
    struct dirent *d;

    argcount("read-directory", nargs, 1);
    dirhandleptr = todirhandleptr(args[0], "dir");
    dirhandle = *dirhandleptr;
    for (;;) {
        errno = 0;
        if (!(d = readdir(dirhandle))) {
            break;
        }
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
            continue;
        }
        break;
    }
    if (!d && errno) {
        lerror(IOError, "cannot read directory");
    }
    return d ? string_from_cstr(d->d_name) : FL_EOF;
}

value_t builtin_os_close_directory(value_t *args, uint32_t nargs)
{
    DIR *dirhandle;
    DIR **dirhandleptr;

    argcount("read-directory", nargs, 1);
    dirhandleptr = todirhandleptr(args[0], "dir");
    dirhandle = *dirhandleptr;
    if (dirhandle) {
        if (closedir(dirhandle) == -1) {
            lerror(IOError, "cannot close directory");
        }
        *dirhandleptr = dirhandle = 0;
    }
    return FL_F;
}

static void print_dir(value_t v, struct ios *f)
{
    (void)v;
    fl_print_str("#<directory>", f);
}

static void free_dir(value_t self)
{
    DIR *dirhandle;
    DIR **dirhandleptr;

    dirhandleptr = todirhandleptr(self, "dir");
    dirhandle = *dirhandleptr;
    if (dirhandle) {
        if (closedir(dirhandle) == -1) {
            // lerror(IOError, "cannot close directory");
        }
        *dirhandleptr = dirhandle = 0;
    }
}

static void relocate_dir(value_t oldv, value_t newv)
{
    (void)oldv;
    (void)newv;
}

// TODO: cleanup
static struct termios term_mode_orig;
static struct termios term_mode_raw;

static void term_mode_init(void)
{
    static int done;

    if (!done) {
        done = 1;
        tcgetattr(0, &term_mode_orig);
        cfmakeraw(&term_mode_raw);
    }
}

value_t builtin_term_init(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("term-init", nargs, 0);
    term_mode_init();
    tcsetattr(0, TCSAFLUSH, &term_mode_raw);
    return FL_T;
}

value_t builtin_term_exit(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("term-exit", nargs, 0);
    term_mode_init();
    tcsetattr(0, TCSAFLUSH, &term_mode_orig);
    return FL_T;
}

struct cvtable dir_vtable = { print_dir, relocate_dir, free_dir, NULL };

void os_init(void)
{
    dirsym = symbol("dir");
    dirtype = define_opaque_type(dirsym, sizeof(DIR *), &dir_vtable, NULL);
}
