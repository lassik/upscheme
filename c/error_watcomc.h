#pragma aux DivideByZeroError aborts;
extern void DivideByZeroError(void);

#pragma aux lerrorf aborts;
extern void lerrorf(value_t e, char *format, ...);

#pragma aux lerror aborts;
extern void lerror(value_t e, const char *msg);

#pragma aux fl_raise aborts;
extern void fl_raise(value_t e);

#pragma aux type_error aborts;
extern void type_error(char *fname, char *expected, value_t got);

#pragma aux bounds_error aborts;
extern void bounds_error(char *fname, value_t arr, value_t ind);
