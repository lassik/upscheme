#include <sys/types.h>

#include <sys/stat.h>

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
#include <unistd.h>

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
#include "os.h"

void path_to_dirname(char *path)
{
    char *p;

    if (!(p = strrchr(path, '/'))) {
        p = path;
    }
    *p = '\0';
}

void get_cwd(char *buf, size_t size) { getcwd(buf, size); }

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

    if (stat(path, &st) == -1) {
        return FL_F;
    }
    return FL_T;
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
