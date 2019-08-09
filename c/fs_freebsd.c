#include <sys/types.h>

#include <sys/sysctl.h>

#include "fs.h"

char *get_exename(char *buf, size_t size)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };

    sysctl(mib, 4, buf, &size, 0, 0);
    return buf;
}
