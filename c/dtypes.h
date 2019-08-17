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

#undef BITS32   // TODO
#define BITS64  // TODO

#ifdef __GNUC__
#define EXTERN_NORETURN(ret, args) extern ret args __attribute__((__noreturn__))
#define STATIC_NORETURN(ret, args) static ret args __attribute__((__noreturn__))
#endif

#ifdef _MSC_VER
#define EXTERN_NORETURN(ret, args) __declspec(noreturn) extern ret args
#define STATIC_NORETURN(ret, args) __declspec(noreturn) static ret args
#endif

#ifdef __WATCOMC__
#pragma aux noreturn aborts;
#define EXTERN_NORETURN(ret, args) extern ret __pragma("noreturn") args
#define STATIC_NORETURN(ret, args) static ret __pragma("noreturn") args
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
#define BIT31 0x80000000

#define LOG2_10 3.3219280948873626
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
