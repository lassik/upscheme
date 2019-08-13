#include <mach-o/dyld.h>

#include "os.h"

char *get_exename(char *buf, size_t size)
{
    uint32_t bufsize = (uint32_t)size;
    if (_NSGetExecutablePath(buf, &bufsize))
        return NULL;
    return buf;
}
