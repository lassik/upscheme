#include <sys/types.h>

#include <sys/sysctl.h>

#include "fs.h"

char *get_exename(char *buf, size_t size)
{
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    sysctl(mib, 4, buf, &size, NULL, 0);

    return buf;
}
