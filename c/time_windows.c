#include <windows.h>

#include <stdint.h>
#include <time.h>

#include "scheme.h"

#if 0
double tvals2float(struct tm *t, struct timeb *tstruct)
{
    return (double)t->tm_hour * 3600 + (double)t->tm_min * 60 +
        (double)t->tm_sec + (double)tstruct->millitm/1.0e3;
}
#endif

uint64_t i64time(void) { return 0; }

void sleep_ms(int ms)
{
    if (ms < 1) {
        return;
    }
    Sleep(ms);
}

void timeparts(int32_t *buf, double t)
{
    time_t tme = (time_t)t;
    struct tm *tm;

    tm = localtime(&tme);
    tm->tm_year += 1900;
    memcpy(buf, (char *)tm, sizeof(struct tm));
}
