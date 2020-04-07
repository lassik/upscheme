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

#include "scheme.h"

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
#define SRFI_175 (1 << 11)  // ASCII character library
#define SRFI_176 (1 << 11)  // Version flag

// Up Scheme libraries
#define UP_2019 (1 << 20)

const int upscheme_stable_specs[] = { 0 };
const int upscheme_unstable_spec = 2019;

static struct builtin_procedure builtin_procedures[] = {
#if 0
    { "create-directory", fs_create_directory, SRFI_170 | UP_2019 },
    { "file-info", fs_file_info, SRFI_170 | UP_2019 },
    { "string-null?", string_null_p, SRFI_13, UP_2019 },
    { "make-string", string_make_string, SRFI_13 | UP_2019 },
#endif

    { "features", builtin_features, R7RS_BASE | UP_2019 },
    { "version-alist", builtin_version_alist, SRFI_176 | UP_2019 },
    { "os-executable-file", builtin_os_executable_file, UP_2019 },

    { "string?", fl_stringp, SRFI_13 | R7RS_BASE | UP_2019 },
    { "string-reverse", fl_string_reverse, SRFI_13 | UP_2019 },
    { "string-split", builtin_string_split, UP_2019 },
    { "substring", fl_string_sub, R7RS_BASE | UP_2019 },

    { "environment-stack", builtin_environment_stack, UP_2019 },

    { "read-ini-file", builtin_read_ini_file, UP_2019 },

    { "pid", builtin_pid, SRFI_170 | UP_2019 },
    { "parent-pid", builtin_parent_pid, SRFI_170 | UP_2019 },
    { "process-group", builtin_process_group, SRFI_170 | UP_2019 },

    { "user-effective-gid", builtin_user_effective_gid, SRFI_170 | UP_2019 },
    { "user-effective-uid", builtin_user_effective_uid, SRFI_170 | UP_2019 },
    { "user-real-gid", builtin_user_real_gid, SRFI_170 | UP_2019 },
    { "user-real-uid", builtin_user_real_uid, SRFI_170 | UP_2019 },

    { "term-init", builtin_term_init, UP_2019 },
    { "term-exit", builtin_term_exit, UP_2019 },

    { "spawn", builtin_spawn, SRFI_170 | UP_2019 },

    { "color-name->rgb24", builtin_color_name_to_rgb24, UP_2019 },

    { "file-exists?", builtin_file_exists, R7RS_FILE | UP_2019 },

    { "open-directory", builtin_os_open_directory, SRFI_170 | UP_2019 },
    { "read-directory", builtin_os_read_directory, SRFI_170 | UP_2019 },
    { "close-directory", builtin_os_close_directory, SRFI_170 | UP_2019 },

    { "get-environment-variables", builtin_get_environment_variables,
      R7RS_PROCESS_CONTEXT | UP_2019 },
    { "get-environment-variable", builtin_get_environment_variable,
      R7RS_PROCESS_CONTEXT | UP_2019 },
    { "set-environment-variable", builtin_set_environment_variable,
      R7RS_PROCESS_CONTEXT | UP_2019 },

    { "ascii-codepoint?", builtin_ascii_codepoint_p, SRFI_175 | UP_2019 },
    //{ "ascii-bytevector?", builtin_ascii_bytevector_p, SRFI_175 | UP_2019 },
    { "ascii-char?", builtin_ascii_char_p, SRFI_175 | UP_2019 },
    //{ "ascii-string?", builtin_ascii_string_p, SRFI_175 | UP_2019 },
    { "ascii-control?", builtin_ascii_control_p, SRFI_175 | UP_2019 },
    { "ascii-display?", builtin_ascii_display_p, SRFI_175 | UP_2019 },
    { "ascii-space-or-tab?", builtin_ascii_space_or_tab_p,
      SRFI_175 | UP_2019 },
    { "ascii-punctuation?", builtin_ascii_punctuation_p, SRFI_175 | UP_2019 },
    { "ascii-alphanumeric?", builtin_ascii_alphanumeric_p,
      SRFI_175 | UP_2019 },
    { "ascii-alphabetic?", builtin_ascii_alphabetic_p, SRFI_175 | UP_2019 },
    { "ascii-numeric?", builtin_ascii_numeric_p, SRFI_175 | UP_2019 },
    { "ascii-whitespace?", builtin_ascii_whitespace_p, SRFI_175 | UP_2019 },
    { "ascii-upper-case?", builtin_ascii_upper_case_p, SRFI_175 | UP_2019 },
    { "ascii-lower-case?", builtin_ascii_lower_case_p, SRFI_175 | UP_2019 },
    { "ascii-upcase", builtin_ascii_upcase, SRFI_175 | UP_2019 },
    { "ascii-downcase", builtin_ascii_downcase, SRFI_175 | UP_2019 },
    { "ascii-open-bracket", builtin_ascii_open_bracket, SRFI_175 | UP_2019 },
    { "ascii-close-bracket", builtin_ascii_close_bracket,
      SRFI_175 | UP_2019 },
    { "ascii-mirror-bracket", builtin_ascii_mirror_bracket,
      SRFI_175 | UP_2019 },
    { "ascii-control->display", builtin_ascii_control_to_display,
      SRFI_175 | UP_2019 },
    { "ascii-display->control", builtin_ascii_display_to_control,
      SRFI_175 | UP_2019 },
    { "ascii-nth-digit", builtin_ascii_nth_digit, SRFI_175 | UP_2019 },
    { "ascii-nth-upper-case", builtin_ascii_nth_upper_case,
      SRFI_175 | UP_2019 },
    { "ascii-nth-lower-case", builtin_ascii_nth_lower_case,
      SRFI_175 | UP_2019 },
    { "ascii-digit-value", builtin_ascii_digit_value, SRFI_175 | UP_2019 },
    { "ascii-upper-case-value", builtin_ascii_upper_case_value,
      SRFI_175 | UP_2019 },
    { "ascii-lower-case-value", builtin_ascii_lower_case_value,
      SRFI_175 | UP_2019 },

    { 0, 0, 0 },
};

static struct builtin_library builtin_libraries[] = {
    { "scheme/base", R7RS_BASE },
    { "scheme/char", R7RS_CHAR },
    { "scheme/cxr", R7RS_CXR },
    { "scheme/file", R7RS_FILE },
    { "srfi/13", SRFI_13 },
    { "srfi/170", SRFI_170 },
    { "srfi/175", SRFI_175 },
    { "srfi/176", SRFI_176 },
    { "upscheme/2019/unstable", UP_2019 },
    { 0, 0 },
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
