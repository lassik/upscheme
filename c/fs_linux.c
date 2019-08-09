#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fs.h"

char *get_exename(char *buf, size_t size)
{
    char linkname[64]; /* /proc/<pid>/exe */
    pid_t pid;
    ssize_t ret;

    /* Get our PID and build the name of the link in /proc */
    pid = getpid();

    if (snprintf(linkname, sizeof(linkname), "/proc/%i/exe", pid) < 0)
        return NULL;

    /* Now read the symbolic link */
    ret = readlink(linkname, buf, size);

    /* In case of an error, leave the handling up to the caller */
    if (ret == -1)
        return NULL;

    /* Report insufficient buffer size */
    if ((size_t)ret >= size)
        return NULL;

    /* Ensure proper NUL termination */
    buf[ret] = 0;

    return buf;
}
