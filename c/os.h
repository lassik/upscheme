void path_to_dirname(char *path);
void get_cwd(char *buf, size_t size);
int set_cwd(char *buf);
char *get_exename(char *buf, size_t size);
int os_path_exists(const char *path);
void os_setenv(const char *name, const char *value);
