#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "scheme.h"

static void warn(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void warnsys(const char *msg)
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

static void diesys(const char *msg)
{
    warnsys(msg);
    exit(1);
}

static void set_close_on_exec(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFD)) == -1) {
        diesys("cannot get file descriptor flags");
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        diesys("cannot set close-on-exec");
    }
}

value_t builtin_spawn(value_t *args, uint32_t nargs)
{
    pid_t child;
    int status, exec_errno;
    int fds[2];
    struct pollfd pollfd;
    ssize_t nbyte;
    value_t unix_argv_obj;
    struct sv_accum unix_argv;

    // TODO: memory leak: unix_argv not freed on type error
    argcount("spawn", nargs, 1);
    unix_argv_obj = args[0];
    if (unix_argv_obj == FL_NIL) {
        lerror(ArgError, "executable argument list is empty");
    }
    sv_accum_init(&unix_argv);
    do {
        if (!iscons(unix_argv_obj)) {
            lerror(ArgError, "executable arguments not a proper list");
        }
        sv_accum_strdup(&unix_argv,
                        tostring(car_(unix_argv_obj), "executable argument"));
        unix_argv_obj = cdr_(unix_argv_obj);
    } while (unix_argv_obj != FL_NIL);
    // TODO: Signal mask for the child process.
    if (pipe(fds) == -1) {
        diesys("cannot create pipe");
    }
    set_close_on_exec(fds[0]);
    set_close_on_exec(fds[1]);
    if ((child = fork()) == -1) {
        diesys("cannot fork");
    }
    if (!child) {
        // We are in the new child process.
        close(fds[0]);
        execvp(unix_argv.vec[0], unix_argv.vec);
        exec_errno = errno;
        nbyte = write(fds[1], &exec_errno, sizeof(exec_errno));
        if (nbyte == (ssize_t)-1) {
            warnsys("completely borked (child)");
        } else if (nbyte != (ssize_t)sizeof(exec_errno)) {
            warn("completely borked (child)");
        }
        _exit(126);
    }
    // We are in the old parent process.
    close(fds[1]);
    memset(&pollfd, 0, sizeof(pollfd));
    pollfd.fd = fds[0];
    pollfd.events = POLLIN;
    if (poll(&pollfd, 1, -1) == -1) {
        diesys("cannot poll");
    }
    exec_errno = 0;
    if (pollfd.revents & POLLIN) {
        nbyte = read(pollfd.fd, &exec_errno, sizeof(exec_errno));
        if (nbyte == 0) {
            // We don't get any data, means the pipe was closed.
        } else if (nbyte == (ssize_t)-1) {
            warnsys("completely borked (parent)");
        } else if (nbyte != (ssize_t)sizeof(exec_errno)) {
            warn("completely borked (parent)");
        }
    }
    if (waitpid(child, &status, 0) == -1) {
        warnsys("cannot wait for child process");
        return FL_NIL;
    }
    if (!WIFEXITED(status)) {
        warn("child process did not exit normally");
        return FL_NIL;
    }
    if (exec_errno != 0) {
        errno = exec_errno;
        warnsys("cannot execute command");
        return FL_NIL;
    }
    return fixnum(WEXITSTATUS(status));
}
