#define __unlikely(x) (x)
#define __likely(x) (x)

#define __ORDER_BIG_ENDIAN__ 4321
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__

#pragma aux DivideByZeroError aborts;
extern void DivideByZeroError(void);

#pragma aux lerrorf aborts;
extern void lerrorf(value_t e, const char *format, ...);

#pragma aux lerror aborts;
extern void lerror(value_t e, const char *msg);

#pragma aux fl_raise aborts;
extern void fl_raise(value_t e);

#pragma aux type_error aborts;
extern void type_error(const char *fname, const char *expected, value_t got);

#pragma aux bounds_error aborts;
extern void bounds_error(const char *fname, value_t arr, value_t ind);
