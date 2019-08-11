// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

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
#include "fs.h"
#include "random.h"
#include "llt.h"

#include "ieee754.h"

#include "flisp.h"

#include "buf.h"
#include "env.h"
#include "opcodes.h"

#include "stringfuncs.h"
#include "libraries.h"

struct builtin_procedure {
    char *name;
    builtin_t fptr;
    uint32_t lib_id_mask;
};

struct builtin_library {
    char *path;
    uint32_t lib_id;
};

// R7RS libraries
#define R7RS_BASE (1 << 0)
#define R7RS_CHAR (1 << 1)
#define R7RS_CXR (1 << 2)
#define R7RS_FILE (1 << 3)
#define R7RS_INEXACT (1 << 4)
#define R7RS_PROCESS_CONTEXT (1 << 5)
#define R7RS_READ (1 << 6)
#define R7RS_WRITE (1 << 7)

// SRFI libraries
#define SRFI_0 (1 << 8)
#define SRFI_13 (1 << 9)    // String Libraries
#define SRFI_170 (1 << 10)  // POSIX API

// Up Scheme libraries
#define UP_2019 (1 << 11)

static struct builtin_procedure builtin_procedures[] = {
#if 0
    { "create-directory", fs_create_directory, SRFI_170 | UP_2019 },
    { "file-info", fs_file_info, SRFI_170 | UP_2019 },
    { "string-null?", string_null_p, SRFI_13, UP_2019 },
    { "make-string", string_make_string, SRFI_13 | UP_2019 },
#endif

    { "string?", fl_stringp, SRFI_13 | R7RS_BASE | UP_2019 },
    { "string-reverse", fl_string_reverse, SRFI_13 | UP_2019 },
    { "substring", fl_string_sub, R7RS_BASE | UP_2019 },

    { 0, 0, 0 },
};

static struct builtin_library builtin_libraries[] = {
    { "scheme/base", R7RS_BASE }, { "scheme/char", R7RS_CHAR },
    { "scheme/cxr", R7RS_CXR },   { "scheme/file", R7RS_FILE },
    { "srfi/13", SRFI_13 },       { "srfi/170", SRFI_170 },
    { "upscheme/2019", UP_2019 }, { 0, 0 },
};

static struct builtin_library *builtin_library_by_path(const char *path)
{
    struct builtin_library *lib;

    for (lib = builtin_libraries; lib->path; lib++) {
        if (!strcmp(lib->path, path)) {
            return lib;
        }
    }
    return 0;
}

static void parse_library_name(struct buf *path, value_t libname, int pathsep)
{
    value_t part;

    if (libname == FL_NIL) {
        lerror(ArgError, "library name is the empty list");
    }
    for (;;) {
        if (!iscons(libname)) {
            lerror(ArgError, "library name is not a proper list");
        }
        part = car_(libname);
        if (issymbol(part)) {
            buf_puts(path, symbol_name(part));
        } else if (isfixnum(part)) {
            buf_putu(path, tofixnum(part, "library name part"));
        } else {
            lerror(ArgError, "library name part is not a symbol or a fixnum");
        }
        if ((libname = cdr_(libname)) == FL_NIL) {
            break;
        }
        buf_putc(path, pathsep);
    }
}

static void import_set(value_t impset)
{
    value_t head;
    struct buf *path;
    const char *name;
    struct builtin_library *lib;
    struct builtin_procedure *proc;

    if (impset == FL_NIL) {
        lerror(ArgError, "import: empty list given");
    } else if (!iscons(impset)) {
        lerror(ArgError, "import: non-list argument given");
    }
    head = car_(impset);
    if (issymbol(head)) {
        name = symbol_name(head);
        if (!strcmp(name, "only")) {
            lerror(ArgError, "import: not implemented: only");
        } else if (!strcmp(name, "except")) {
            lerror(ArgError, "import: not implemented: except");
        } else if (!strcmp(name, "prefix")) {
            lerror(ArgError, "import: not implemented: prefix");
        } else if (!strcmp(name, "rename")) {
            lerror(ArgError, "import: not implemented: rename");
        }
    }
    path = buf_new();
    parse_library_name(path, impset, '/');
    // buf_puts(path, ".sld");
    buf_putc(path, 0);
    if (!(lib = builtin_library_by_path(path->bytes))) {
        lerror(ArgError, "import: library not found");
    }
    for (proc = builtin_procedures; proc->name; proc++) {
        if (proc->lib_id_mask & lib->lib_id) {
            // fprintf(stderr, "Importing %s\n", proc->name);
            setc(symbol(proc->name), cbuiltin(proc->name, proc->fptr));
        }
    }
}

value_t builtin_import(value_t *args, uint32_t nargs)
{
    uint32_t i;

    for (i = 0; i < nargs; i++) {
        import_set(args[i]);
    }
    return FL_NIL;
}
