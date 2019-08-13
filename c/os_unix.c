#include <string.h>
#include <unistd.h>

#include "os.h"

void path_to_dirname(char *path)
{
    char *p;

    if (!(p = strrchr(path, '/'))) {
        p = path;
    }
    *p = '\0';
}

void get_cwd(char *buf, size_t size) { getcwd(buf, size); }

int set_cwd(char *buf)
{
    if (chdir(buf) == -1)
        return 1;
    return 0;
}
