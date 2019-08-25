extern void DivideByZeroError(void);
#pragma noreturn(DivideByZeroError)

extern void lerrorf(value_t e, const char *format, ...);
#pragma noreturn(lerrorf)

extern void lerror(value_t e, const char *msg);
#pragma noreturn(lerror)

extern void fl_raise(value_t e);
#pragma noreturn(fl_raise)

extern void type_error(const char *fname, const char *expected, value_t got);
#pragma noreturn(type_error)

extern void bounds_error(const char *fname, value_t arr, value_t ind);
#pragma noreturn(bounds_error)
