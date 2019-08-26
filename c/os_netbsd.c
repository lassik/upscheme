#include <sys/types.h>

#include <sys/sysctl.h>

#include "scheme.h"

char *get_exename(char *buf, size_t size)
{
    int mib[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };

    sysctl(mib, 4, buf, &size, 0, 0);
    return buf;
}
