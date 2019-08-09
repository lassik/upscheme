/*
  This file defines sane integer types for our target platforms. This
  library only runs on machines with the following characteristics:

  - supports integer word sizes of 8, 16, 32, and 64 bits
  - uses unsigned and signed 2's complement representations
  - all pointer types are the same size
  - there is an integer type with the same size as a pointer

  Some features require:
  - IEEE 754 single- and double-precision floating point

  We assume the LP64 convention for 64-bit platforms.
*/

#if defined(__gnu_linux__)
#define LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define MACOSX
#elif defined(__OpenBSD__)
#define OPENBSD
#elif defined(__FreeBSD__)
#define FREEBSD
#elif defined(_WIN32)
#define WIN32
#else
#error "unknown platform"
#endif

#undef BITS32   // TODO
#define BITS64  // TODO

#if defined(WIN32)
#define STDCALL __stdcall
#if defined(IMPORT_EXPORTS)
#define DLLEXPORT __declspec(dllimport)
#else
#define DLLEXPORT __declspec(dllexport)
#endif
#else
#define STDCALL
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#define LLT_ALLOC(n) malloc(n)
#define LLT_REALLOC(p, n) realloc((p), (n))
#define LLT_FREE(x) free(x)

typedef int bool_t;

#ifdef BITS64
#define TOP_BIT 0x8000000000000000
#define NBITS 64
#else
#define TOP_BIT 0x80000000
#define NBITS 32
#endif

#define LLT_ALIGN(x, sz) (((x) + (sz - 1)) & (-sz))

// branch prediction annotations
#ifdef __GNUC__
#define __unlikely(x) __builtin_expect(!!(x), 0)
#define __likely(x) __builtin_expect(!!(x), 1)
#else
#define __unlikely(x) (x)
#define __likely(x) (x)
#endif

#define DBL_MAXINT 9007199254740992LL
#define FLT_MAXINT 16777216
#define U64_MAX 18446744073709551615ULL
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1LL)
#define BIT63 0x8000000000000000LL
#define U32_MAX 4294967295L
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1L)
#define BIT31 0x80000000

#define DBL_EPSILON 2.2204460492503131e-16
#define FLT_EPSILON 1.192092896e-7
#define DBL_MAX 1.7976931348623157e+308
#define DBL_MIN 2.2250738585072014e-308
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define LOG2_10 3.3219280948873626
#define rel_zero(a, b) (fabs((a) / (b)) < DBL_EPSILON)
#define sign_bit(r) ((*(int64_t *)&(r)) & BIT63)
#define LABS(n) (((n) ^ ((n) >> (NBITS - 1))) - ((n) >> (NBITS - 1)))
#define NBABS(n, nb) (((n) ^ ((n) >> ((nb)-1))) - ((n) >> ((nb)-1)))
#define DFINITE(d) \
    (((*(int64_t *)&(d)) & 0x7ff0000000000000LL) != 0x7ff0000000000000LL)
#define DNAN(d) ((d) != (d))

extern double D_PNAN;
extern double D_NNAN;
extern double D_PINF;
extern double D_NINF;
extern float F_PNAN;
extern float F_NNAN;
extern float F_PINF;
extern float F_NINF;
