__declspec(noreturn) extern void DivideByZeroError(void);

__declspec(noreturn) extern void lerrorf(value_t e, char *format, ...);

__declspec(noreturn) extern void lerror(value_t e, const char *msg);

__declspec(noreturn) extern void fl_raise(value_t e);

__declspec(noreturn) extern void type_error(char *fname, char *expected,
                                            value_t got);

__declspec(noreturn) extern void bounds_error(char *fname, value_t arr,
                                              value_t ind);
