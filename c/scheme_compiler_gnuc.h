typedef uintptr_t value_t;
typedef uintptr_t ufixnum_t;
typedef intptr_t fixnum_t;

#ifdef BITS64
#define T_FIXNUM T_INT64
#else
#define T_FIXNUM T_INT32
#endif

void DivideByZeroError(void) __attribute__((__noreturn__));

void lerrorf(value_t e, const char *format, ...)
__attribute__((__noreturn__));

void lerror(value_t e, const char *msg) __attribute__((__noreturn__));

void fl_raise(value_t e) __attribute__((__noreturn__));

void type_error(const char *fname, const char *expected, value_t got)
__attribute__((__noreturn__));

void bounds_error(const char *fname, value_t arr, value_t ind)
__attribute__((__noreturn__));

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
