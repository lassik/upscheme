typedef uintptr_t value_t;
typedef uintptr_t ufixnum_t;
typedef intptr_t fixnum_t;

#define SCHEME_C_COMPILER_NAME "GCC"  // TODO: wrong
#define SCHEME_C_COMPILER_VERSION __VERSION__

#define DBL_MAXINT 9007199254740992LL
#define FLT_MAXINT 16777216
#define U64_MAX 18446744073709551615ULL
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1LL)
#define BIT63 0x8000000000000000LL
#define BIT31 0x80000000

#define UINT32_TOP_BIT 0x80000000
#define UINT64_TOP_BIT 0x8000000000000000ULL

#if UINTPTR_MAX == 0xffffffffffffffffULL
#define BITS64
#else
#undef BITS64
#endif

#ifdef BITS64
#define TOP_BIT UINT64_TOP_BIT
#else
#define TOP_BIT UINT32_TOP_BIT
#endif

#define LOG2_10 3.3219280948873626
#define sign_bit(r) ((*(int64_t *)&(r)) & BIT63)
#define NBABS(n, nb) (((n) ^ ((n) >> ((nb)-1))) - ((n) >> ((nb)-1)))
#define DFINITE(d) \
    (((*(int64_t *)&(d)) & 0x7ff0000000000000LL) != 0x7ff0000000000000LL)
#define DNAN(d) ((d) != (d))

#define __unlikely(x) __builtin_expect(!!(x), 0)
#define __likely(x) __builtin_expect(!!(x), 1)

void DivideByZeroError(void) __attribute__((__noreturn__));

void lerrorf(value_t e, const char *format, ...)
__attribute__((__noreturn__));

void lerror(value_t e, const char *msg) __attribute__((__noreturn__));

void fl_raise(value_t e) __attribute__((__noreturn__));

void type_error(const char *fname, const char *expected, value_t got)
__attribute__((__noreturn__));

void bounds_error(const char *fname, value_t arr, value_t ind)
__attribute__((__noreturn__));
