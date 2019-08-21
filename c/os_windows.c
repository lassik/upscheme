#include <sys/types.h>

#include <sys/stat.h>

#include <windows.h>

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

    p = strchr(path, 0);
    while (p > path) {
        p--;
        if ((*p == '/') || (*p == '\\')) {
            break;
        }
    }
    *p = '\0';
}

void get_cwd(char *buf, size_t size) { GetCurrentDirectory(size, buf); }

int set_cwd(char *buf)
{
    if (SetCurrentDirectory(buf) == 0)
        return 1;
    return 0;
}

char *get_exename(char *buf, size_t size)
{
    if (GetModuleFileName(NULL, buf, size) == 0)
        return NULL;
    return buf;
}

value_t builtin_pid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("pid", nargs, 0);
    return fixnum(GetCurrentProcessId());
}

value_t builtin_parent_pid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("parent-pid", nargs, 0);
    // TODO: OpenProcess() to prevent parent pid from being reused if parent
    // dies
    return FL_F;
}

value_t builtin_process_group(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("process-group", nargs, 0);
    return FL_F;
}

value_t builtin_user_effective_gid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-effective-gid", nargs, 0);
    return FL_F;
}

value_t builtin_user_effective_uid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-effective-uid", nargs, 0);
    return FL_F;
}

value_t builtin_user_real_gid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-real-gid", nargs, 0);
    return FL_F;
}

value_t builtin_user_real_uid(value_t *args, uint32_t nargs)
{
    (void)args;
    argcount("user-real-uid", nargs, 0);
    return FL_F;
}

int os_path_exists(const char *path)
{
    struct stat st;

    if (_stat(path, &st) == -1) {
        return FL_F;
    }
    return FL_T;
}

void os_setenv(const char *name, const char *value)
{
    if (!SetEnvironmentVariable(name, value)) {
        if (value) {
            lerror(ArgError, "os.setenv: cannot set environment variable");
        } else {
            lerror(ArgError, "os.setenv: cannot unset environment variable");
        }
    }
}

value_t builtin_spawn(value_t *args, uint32_t nargs)
{
    (void)args;
    (void)nargs;
    return FL_F;
}
