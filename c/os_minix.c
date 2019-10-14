#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "scheme.h"

pid_t getpgid(pid_t pid)
{
    (void)pid;
    errno = ENOSYS;
    return (pid_t)-1;
}

char *get_exename(char *buf, size_t size)
{
    (void)buf;
    (void)size;
    return NULL;
}
