#include <sys/utsname.h>

#include <string.h>

static const struct utsname *get_global_uname(void)
{
    static struct utsname buf;

    if (!buf.sysname[0]) {
        if (uname(&buf) == -1) {
            memset(&buf, 0, sizeof(buf));
        }
    }
    return &buf;
}

const char *env_get_os_name(void) { return get_global_uname()->sysname; }
