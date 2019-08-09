#include <sys/poll.h>
#include <sys/time.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "timefuncs.h"

double tv2float(struct timeval *tv)
{
    return (double)tv->tv_sec + (double)tv->tv_usec / 1.0e6;
}

double diff_time(struct timeval *tv1, struct timeval *tv2)
{
    return tv2float(tv1) - tv2float(tv2);
}

uint64_t i64time(void)
{
    uint64_t a;
    struct timeval now;

    gettimeofday(&now, NULL);
    a = (((uint64_t)now.tv_sec) << 32) + (uint64_t)now.tv_usec;
    return a;
}

double clock_now(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return tv2float(&now);
}

void sleep_ms(int ms)
{
    struct timeval timeout;

    if (ms == 0) {
        return;
    }
    timeout.tv_sec = ms / 1000;
    timeout.tv_usec = (ms % 1000) * 1000;
    select(0, NULL, NULL, NULL, &timeout);
}

void timeparts(int32_t *buf, double t)
{
    struct tm tm;
    time_t tme = (time_t)t;

    localtime_r(&tme, &tm);
    tm.tm_year += 1900;
    memcpy(buf, (char *)&tm, sizeof(struct tm));
}

double parsetime(const char *str)
{
    char *fmt = "%c"; /* needed to suppress GCC warning */
    char *res;
    time_t t;
    struct tm tm;

    res = strptime(str, fmt, &tm);
    if (res != NULL) {
        tm.tm_isdst =
        -1; /* Not set by strptime(); tells mktime() to determine
              whether daylight saving time is in effect */
        t = mktime(&tm);
        if (t == ((time_t)-1))
            return -1;
        return (double)t;
    }
    return -1;
}
