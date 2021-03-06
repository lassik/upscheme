#include <sys/types.h>

#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "scheme.h"

double D_PNAN;
double D_NNAN;
double D_PINF;
double D_NINF;
float F_PNAN;
float F_NNAN;
float F_PINF;
float F_NINF;

int locale_is_utf8;

#ifdef __WATCOMC__
static float purkka(const char *s, char **dummy) { return strtod(s, dummy); }
#define strtof purkka
#endif

void llt_init()
{
    locale_is_utf8 = u8_is_locale_utf8(setlocale(LC_ALL, ""));

    randomize();

    ios_init_stdstreams();

    D_PNAN = strtod("+NaN", NULL);
    D_NNAN = -strtod("+NaN", NULL);
    D_PINF = strtod("+Inf", NULL);
    D_NINF = strtod("-Inf", NULL);

    F_PNAN = strtof("+NaN", NULL);
    F_NNAN = -strtof("+NaN", NULL);
    F_PINF = strtof("+Inf", NULL);
    F_NINF = strtof("-Inf", NULL);
}
