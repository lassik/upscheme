void DivideByZeroError(void) __attribute__((__noreturn__));

void lerrorf(value_t e, const char *format, ...)
__attribute__((__noreturn__));

void lerror(value_t e, const char *msg) __attribute__((__noreturn__));

void fl_raise(value_t e) __attribute__((__noreturn__));

void type_error(const char *fname, const char *expected, value_t got)
__attribute__((__noreturn__));

void bounds_error(const char *fname, value_t arr, value_t ind)
__attribute__((__noreturn__));
