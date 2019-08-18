#include <windows.h>

#include "os.h"

void path_to_dirname(char *path)
{
    char *p;

    p = strchr(path, 0);
    while (p > path) {
        p--;
        if ((*p == '/') || (*p == '\\')) {
            break;
        }
    }
    *p = '\0';
}

void get_cwd(char *buf, size_t size) { GetCurrentDirectory(size, buf); }

int set_cwd(char *buf)
{
    if (SetCurrentDirectory(buf) == 0)
        return 1;
    return 0;
}

char *get_exename(char *buf, size_t size)
{
    if (GetModuleFileName(NULL, buf, size) == 0)
        return NULL;
    return buf;
}

int os_path_exists(const char *path)
{
    struct stat st;

    if (_stat(path, &st) == -1) {
        return FL_F;
    }
    return FL_T;
}
