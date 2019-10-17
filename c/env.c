#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

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

static value_t build_c_type_bits_list(void)
{
    struct accum acc;

    accum_init(&acc);
    accum_name_value1(&acc, "int", fixnum(sizeof(int) * CHAR_BIT));
    accum_name_value1(&acc, "long", fixnum(sizeof(long) * CHAR_BIT));
    accum_name_value1(&acc, "float", fixnum(sizeof(float) * CHAR_BIT));
    accum_name_value1(&acc, "double", fixnum(sizeof(double) * CHAR_BIT));
    accum_name_value1(&acc, "pointer", fixnum(sizeof(void *) * CHAR_BIT));
    accum_name_value1(&acc, "size_t", fixnum(sizeof(size_t) * CHAR_BIT));
    accum_name_value1(&acc, "value_t", fixnum(sizeof(value_t) * CHAR_BIT));
    return acc.list;
}

static value_t build_stable_specs_list(void)
{
    struct accum acc;
    const int *p;

    accum_init(&acc);
    for (p = upscheme_stable_specs; *p; p++) {
        accum_elt(&acc, fixnum(*p));
    }
    return acc.list;
}

static value_t build_platform_list(void)
{
    struct accum acc;
    const char *kernel;
    const char *userland;
    const char *computer;
    const char *endian;

    // <http://predef.sf.net/>

    kernel = userland = computer = endian = "unknown";

#ifdef __FreeBSD__
    kernel = userland = "freebsd";
#endif
#ifdef __FreeBSD_kernel__
    kernel = "freebsd";
#endif
#ifdef __GLIBC__
    userland = "gnu";
#endif
#ifdef __OpenBSD__
    userland = kernel = "openbsd";
#endif
#ifdef __NetBSD__
    userland = kernel = "netbsd";
#endif
#ifdef __DragonFly__
    userland = kernel = "dragonfly";
#endif
#ifdef __sun
    userland = kernel = "solaris";
#endif
#ifdef __minix
    userland = kernel = "minix";
#endif
#ifdef __HAIKU__
    userland = "beos";
    kernel = "haiku";
#endif
#ifdef __APPLE__
    userland = kernel = "darwin";
#endif
#ifdef _WIN32
    kernel = userland = "windows-nt";
#endif

#ifdef __i386
    computer = "x86";
#endif
#ifdef _M_IX86
    computer = "x86";
#endif
#ifdef __X86__
    computer = "x86";
#endif
#ifdef __I86__
    computer = "x86";
#endif
#ifdef __amd64
    computer = "x86-64";
#endif
#ifdef __x86_64
    computer = "x86-64";
#endif
#ifdef _M_AMD64
    computer = "x86-64";
#endif
#ifdef __ppc__
    computer = "ppc";
#endif
#ifdef _M_PPC
    computer = "ppc";
#endif
#ifdef __PPC64__
    computer = "ppc-64";
#endif
#ifdef __mips64__
    computer = "mips-64";
#endif
#ifdef __arm__
    computer = "arm";
#endif
#ifdef __aarch64__
    computer = "arm";
#endif
#ifdef __sparc
    computer = "sparc";
#endif
#ifdef __sparc__
    computer = "sparc";
#endif
#ifdef __sparc64__
    computer = "sparc";
#endif
#ifdef __mips__
    computer = "mips";
#endif
#ifdef __mips64__
    computer = "mips";
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    endian = "big";
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    endian = "little";
#endif

    accum_init(&acc);
    accum_name_value1(&acc, "kernel", symbol(kernel));
    accum_name_value1(&acc, "userland", symbol(userland));
    accum_name_value1(&acc, "computer", symbol(computer));
    accum_name_value1(&acc, "endian", symbol(endian));
    return acc.list;
}

value_t get_version_alist(void)
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
        accum_name_value(&acc, "platform", build_platform_list());
        accum_name_value(&acc, "c-type-bits", build_c_type_bits_list());
        accum_name_value1(&acc, "c-compiler-version",
                          string_from_cstr(SCHEME_C_COMPILER_NAME
                                           " " SCHEME_C_COMPILER_VERSION));
        accum_name_value1(&acc, "c-compiler-command",
                          string_from_cstr(env_build_cc));
        accum_name_value1(&acc, "c-compiler-flags",
                          string_from_cstr(env_build_cflags));
        accum_name_value1(&acc, "c-linker-flags",
                          string_from_cstr(env_build_lflags));
        accum_name_value1(&acc, "revision",
                          string_from_cstr(env_build_revision));
        accum_name_value1(&acc, "build-date",
                          string_from_cstr(env_build_date));
        accum_name_value1(&acc, "release", string_from_cstr(env_release));
        accum_name_value1(&acc, "release-date",
                          string_from_cstr(env_release_date));
        accum_name_value(&acc, "upscheme/stable-specs",
                         build_stable_specs_list());
        accum_name_value1(&acc, "upscheme/unstable-spec",
                          fixnum(upscheme_unstable_spec));
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
