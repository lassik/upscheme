#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>  // for printf
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "dtypes.h"

#ifdef _WIN32
#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#endif

#include "utils.h"
#include "utf8.h"
#include "ios.h"
#include "timefuncs.h"

#define MOST_OF(x) ((x) - ((x) >> 4))

static void *our_memrchr(const void *s, int c, size_t n)
{
    const unsigned char *src;
    unsigned char uc;

    src = (unsigned char *)s + n;
    uc = c;
    while (--src >= (unsigned char *)s)
        if (*src == uc)
            return (void *)src;
    return NULL;
}

#if 0
// poll for read, unless forwrite!=0
static void _fd_poll(long fd, int forwrite)
{
#ifndef _WIN32
    fd_set set;

    FD_ZERO(&set);
    FD_SET(fd, &set);
    if (forwrite)
        select(fd+1, NULL, &set, NULL, NULL);
    else
        select(fd+1, &set, NULL, NULL, NULL);
#else
#endif
}
#endif

#ifdef _WIN32
static int _enonfatal(int err)
{
    (void)err;
    return 0;
}
#endif

#ifndef _WIN32
static int _enonfatal(int err)
{
    return (err == EAGAIN || err == EINPROGRESS || err == EINTR ||
            err == EWOULDBLOCK);
}
#endif

#define SLEEP_TIME 5  // ms

// return error code, #bytes read in *nread
// these wrappers retry operations until success or a fatal error
static int _os_read(long fd, void *buf, size_t n, size_t *nread)
{
    ssize_t r;

    while (1) {
        r = read((int)fd, buf, n);
        if (r > -1) {
            *nread = (size_t)r;
            return 0;
        }
        if (!_enonfatal(errno)) {
            *nread = 0;
            return errno;
        }
        sleep_ms(SLEEP_TIME);
    }
    return 0;
}

static int _os_read_all(long fd, void *buf, size_t n, size_t *nread)
{
    unsigned char *ubuf;
    size_t got;
    int err;

    ubuf = buf;
    *nread = 0;
    while (n > 0) {
        err = _os_read(fd, ubuf, n, &got);
        n -= got;
        *nread += got;
        ubuf += got;
        if (err || got == 0)
            return err;
    }
    return 0;
}

static int _os_write(long fd, void *buf, size_t n, size_t *nwritten)
{
    ssize_t r;

    while (1) {
        r = write((int)fd, buf, n);
        if (r > -1) {
            *nwritten = (size_t)r;
            return 0;
        }
        if (!_enonfatal(errno)) {
            *nwritten = 0;
            return errno;
        }
        sleep_ms(SLEEP_TIME);
    }
    return 0;
}

static int _os_write_all(long fd, void *buf, size_t n, size_t *nwritten)
{
    unsigned char *ubuf;
    size_t wrote;
    int err;

    ubuf = buf;
    *nwritten = 0;
    while (n > 0) {
        err = _os_write(fd, ubuf, n, &wrote);
        n -= wrote;
        *nwritten += wrote;
        ubuf += wrote;
        if (err)
            return err;
    }
    return 0;
}

/* internal utility functions */

static char *_buf_realloc(struct ios *s, size_t sz)
{
    char *temp;

    if ((s->buf == NULL || s->buf == &s->local[0]) && (sz <= IOS_INLSIZE)) {
        /* TODO: if we want to allow shrinking, see if the buffer shrank
           down to this size, in which case we need to copy. */
        s->buf = &s->local[0];
        s->maxsize = IOS_INLSIZE;
        s->ownbuf = 1;
        return s->buf;
    }

    if (sz <= s->maxsize)
        return s->buf;

    if (s->ownbuf && s->buf != &s->local[0]) {
        // if we own the buffer we're free to resize it
        // always allocate 1 bigger in case user wants to add a NUL
        // terminator after taking over the buffer
        temp = LLT_REALLOC(s->buf, sz + 1);
        if (temp == NULL)
            return NULL;
    } else {
        temp = LLT_ALLOC(sz + 1);
        if (temp == NULL)
            return NULL;
        s->ownbuf = 1;
        if (s->size > 0)
            memcpy(temp, s->buf, s->size);
    }

    s->buf = temp;
    s->maxsize = sz;
    return s->buf;
}

// write a block of data into the buffer at the current position, resizing
// if necessary. returns # written.
static size_t _write_grow(struct ios *s, char *data, size_t n)
{
    size_t amt;
    size_t newsize;

    if (n == 0)
        return 0;

    if (s->bpos + n > s->size) {
        if (s->bpos + n > s->maxsize) {
            /* TODO: here you might want to add a mechanism for limiting
               the growth of the stream. */
            newsize = s->maxsize ? s->maxsize * 2 : 8;
            while (s->bpos + n > newsize)
                newsize *= 2;
            if (_buf_realloc(s, newsize) == NULL) {
                /* no more space; write as much as we can */
                amt = s->maxsize - s->bpos;
                if (amt > 0) {
                    memcpy(&s->buf[s->bpos], data, amt);
                }
                s->bpos += amt;
                s->size = s->maxsize;
                return amt;
            }
        }
        s->size = s->bpos + n;
    }
    memcpy(s->buf + s->bpos, data, n);
    s->bpos += n;

    return n;
}

/* interface functions, low level */

static size_t _ios_read(struct ios *s, char *dest, size_t n, int all)
{
    size_t tot = 0;
    size_t got, avail;

    while (n > 0) {
        avail = s->size - s->bpos;

        if (avail > 0) {
            size_t ncopy = (avail >= n) ? n : avail;
            memcpy(dest, s->buf + s->bpos, ncopy);
            s->bpos += ncopy;
            if (ncopy >= n) {
                s->state = bst_rd;
                return tot + ncopy;
            }
        }
        if (s->bm == bm_mem || s->fd == -1) {
            // can't get any more data
            s->state = bst_rd;
            if (avail == 0)
                s->_eof = 1;
            return avail;
        }

        dest += avail;
        n -= avail;
        tot += avail;

        ios_flush(s);
        s->bpos = s->size = 0;
        s->state = bst_rd;

        s->fpos = -1;
        if (n > MOST_OF(s->maxsize)) {
            // doesn't fit comfortably in buffer; go direct
            if (all)
                _os_read_all(s->fd, dest, n, &got);
            else
                _os_read(s->fd, dest, n, &got);
            tot += got;
            if (got == 0)
                s->_eof = 1;
            return tot;
        } else {
            // refill buffer
            if (_os_read(s->fd, s->buf, s->maxsize, &got)) {
                s->_eof = 1;
                return tot;
            }
            if (got == 0) {
                s->_eof = 1;
                return tot;
            }
            s->size = got;
        }
    }

    return tot;
}

size_t ios_read(struct ios *s, char *dest, size_t n)
{
    return _ios_read(s, dest, n, 0);
}

size_t ios_readall(struct ios *s, char *dest, size_t n)
{
    return _ios_read(s, dest, n, 1);
}

size_t ios_readprep(struct ios *s, size_t n)
{
    size_t got, space;
    int result;

    if (s->state == bst_wr && s->bm != bm_mem) {
        ios_flush(s);
        s->bpos = s->size = 0;
    }
    space = s->size - s->bpos;
    s->state = bst_rd;
    if (space >= n || s->bm == bm_mem || s->fd == -1)
        return space;
    if (s->maxsize < s->bpos + n) {
        // it won't fit. grow buffer or move data back.
        if (n <= s->maxsize && space <= ((s->maxsize) >> 2)) {
            if (space)
                memmove(s->buf, s->buf + s->bpos, space);
            s->size -= s->bpos;
            s->bpos = 0;
        } else {
            if (_buf_realloc(s, s->bpos + n) == NULL)
                return space;
        }
    }
    result = _os_read(s->fd, s->buf + s->size, s->maxsize - s->size, &got);
    if (result)
        return space;
    s->size += got;
    return s->size - s->bpos;
}

static void _write_update_pos(struct ios *s)
{
    if (s->bpos > s->ndirty)
        s->ndirty = s->bpos;
    if (s->bpos > s->size)
        s->size = s->bpos;
}

size_t ios_write(struct ios *s, char *data, size_t n)
{
    size_t space, wrote;

    if (s->readonly)
        return 0;
    if (n == 0)
        return 0;

    wrote = 0;
    if (s->state == bst_none)
        s->state = bst_wr;
    if (s->state == bst_rd) {
        if (!s->rereadable) {
            s->size = 0;
            s->bpos = 0;
        }
        space = s->size - s->bpos;
    } else {
        space = s->maxsize - s->bpos;
    }

    if (s->bm == bm_mem) {
        wrote = _write_grow(s, data, n);
    } else if (s->bm == bm_none) {
        s->fpos = -1;
        _os_write_all(s->fd, data, n, &wrote);
        return wrote;
    } else if (n <= space) {
        if (s->bm == bm_line) {
            char *nl;
            if ((nl = (char *)our_memrchr(data, '\n', n)) != NULL) {
                size_t linesz = nl - data + 1;
                s->bm = bm_block;
                wrote += ios_write(s, data, linesz);
                ios_flush(s);
                s->bm = bm_line;
                n -= linesz;
                data += linesz;
            }
        }
        memcpy(s->buf + s->bpos, data, n);
        s->bpos += n;
        wrote += n;
    } else {
        s->state = bst_wr;
        ios_flush(s);
        if (n > MOST_OF(s->maxsize)) {
            _os_write_all(s->fd, data, n, &wrote);
            return wrote;
        }
        return ios_write(s, data, n);
    }
    _write_update_pos(s);
    return wrote;
}

off_t ios_seek(struct ios *s, off_t pos)
{
    off_t fdpos;

    s->_eof = 0;
    if (s->bm == bm_mem) {
        if ((size_t)pos > s->size)
            return -1;
        s->bpos = pos;
    } else {
        ios_flush(s);
        fdpos = lseek(s->fd, pos, SEEK_SET);
        if (fdpos == (off_t)-1)
            return fdpos;
        s->bpos = s->size = 0;
    }
    return 0;
}

off_t ios_seek_end(struct ios *s)
{
    off_t fdpos;

    s->_eof = 1;
    if (s->bm == bm_mem) {
        s->bpos = s->size;
    } else {
        ios_flush(s);
        fdpos = lseek(s->fd, 0, SEEK_END);
        if (fdpos == (off_t)-1)
            return fdpos;
        s->bpos = s->size = 0;
    }
    return 0;
}

off_t ios_skip(struct ios *s, off_t offs)
{
    off_t fdpos;

    if (offs != 0) {
        if (offs > 0) {
            if (offs <= (off_t)(s->size - s->bpos)) {
                s->bpos += offs;
                return 0;
            } else if (s->bm == bm_mem) {
                // TODO: maybe grow buffer
                return -1;
            }
        } else if (offs < 0) {
            if (-offs <= (off_t)s->bpos) {
                s->bpos += offs;
                s->_eof = 0;
                return 0;
            } else if (s->bm == bm_mem) {
                return -1;
            }
        }
        ios_flush(s);
        if (s->state == bst_wr)
            offs += s->bpos;
        else if (s->state == bst_rd)
            offs -= (s->size - s->bpos);
        fdpos = lseek(s->fd, offs, SEEK_CUR);
        if (fdpos == (off_t)-1)
            return fdpos;
        s->bpos = s->size = 0;
        s->_eof = 0;
    }
    return 0;
}

off_t ios_pos(struct ios *s)
{
    off_t fdpos;

    if (s->bm == bm_mem)
        return (off_t)s->bpos;

    fdpos = s->fpos;
    if (fdpos == (off_t)-1) {
        fdpos = lseek(s->fd, 0, SEEK_CUR);
        if (fdpos == (off_t)-1)
            return fdpos;
        s->fpos = fdpos;
    }

    if (s->state == bst_wr)
        fdpos += s->bpos;
    else if (s->state == bst_rd)
        fdpos -= (s->size - s->bpos);
    return fdpos;
}

size_t ios_trunc(struct ios *s, size_t size)
{
    if (s->bm == bm_mem) {
        if (size == s->size)
            return s->size;
        if (size < s->size) {
            if (s->bpos > size)
                s->bpos = size;
        } else {
            if (_buf_realloc(s, size) == NULL)
                return s->size;
        }
        s->size = size;
        return size;
    }
    // todo
    return 0;
}

int ios_eof(struct ios *s)
{
    if (s->bm == bm_mem)
        return (s->_eof ? 1 : 0);
    if (s->fd == -1)
        return 1;
    if (s->_eof)
        return 1;
    return 0;
}

int ios_flush(struct ios *s)
{
    size_t nw, ntowrite;
    int err;

    if (s->ndirty == 0 || s->bm == bm_mem || s->buf == NULL)
        return 0;
    if (s->fd == -1)
        return -1;

    if (s->state == bst_rd) {
        if (lseek(s->fd, -(off_t)s->size, SEEK_CUR) == (off_t)-1) {
        }
    }

    ntowrite = s->ndirty;
    s->fpos = -1;
    err = _os_write_all(s->fd, s->buf, ntowrite, &nw);
    // todo: try recovering from some kinds of errors (e.g. retry)

    if (s->state == bst_rd) {
        if (lseek(s->fd, s->size - nw, SEEK_CUR) == (off_t)-1) {
        }
    } else if (s->state == bst_wr) {
        if (s->bpos != nw &&
            lseek(s->fd, (off_t)s->bpos - (off_t)nw, SEEK_CUR) == (off_t)-1) {
        }
        // now preserve the invariant that data to write
        // begins at the beginning of the buffer, and s->size refers
        // to how much valid file data is stored in the buffer.
        if (s->size > s->ndirty) {
            size_t delta = s->size - s->ndirty;
            memmove(s->buf, s->buf + s->ndirty, delta);
        }
        s->size -= s->ndirty;
        s->bpos = 0;
    }

    s->ndirty = 0;

    if (err)
        return err;
    if (nw < ntowrite)
        return -1;
    return 0;
}

void ios_close(struct ios *s)
{
    ios_flush(s);
    if (s->fd != -1 && s->ownfd)
        close(s->fd);
    s->fd = -1;
    if (s->buf != NULL && s->ownbuf && s->buf != &s->local[0])
        LLT_FREE(s->buf);
    s->buf = NULL;
    s->size = s->maxsize = s->bpos = 0;
}

static void _buf_init(struct ios *s, bufmode_t bm)
{
    s->bm = bm;
    if (s->bm == bm_mem || s->bm == bm_none) {
        s->buf = &s->local[0];
        s->maxsize = IOS_INLSIZE;
    } else {
        s->buf = NULL;
        _buf_realloc(s, IOS_BUFSIZE);
    }
    s->size = s->bpos = 0;
}

char *ios_takebuf(struct ios *s, size_t *psize)
{
    char *buf;

    ios_flush(s);

    if (s->buf == &s->local[0]) {
        buf = LLT_ALLOC(s->size + 1);
        if (buf == NULL)
            return NULL;
        if (s->size)
            memcpy(buf, s->buf, s->size);
    } else {
        buf = s->buf;
    }
    buf[s->size] = '\0';

    *psize = s->size + 1;  // buffer is actually 1 bigger for terminating NUL

    /* empty stream and reinitialize */
    _buf_init(s, s->bm);

    return buf;
}

int ios_setbuf(struct ios *s, char *buf, size_t size, int own)
{
    size_t nvalid = 0;
    ios_flush(s);

    nvalid = (size < s->size) ? size : s->size;
    if (nvalid > 0)
        memcpy(buf, s->buf, nvalid);
    if (s->bpos > nvalid) {
        // truncated
        s->bpos = nvalid;
    }
    s->size = nvalid;

    if (s->buf != NULL && s->ownbuf && s->buf != &s->local[0])
        LLT_FREE(s->buf);
    s->buf = buf;
    s->maxsize = size;
    s->ownbuf = own;
    return 0;
}

int ios_bufmode(struct ios *s, bufmode_t mode)
{
    // no fd; can only do mem-only buffering
    if (s->fd == -1 && mode != bm_mem)
        return -1;
    s->bm = mode;
    return 0;
}

void ios_set_readonly(struct ios *s)
{
    if (s->readonly)
        return;
    ios_flush(s);
    s->state = bst_none;
    s->readonly = 1;
}

static size_t ios_copy_(struct ios *to, struct ios *from, size_t nbytes,
                        bool_t all)
{
    size_t total, avail, written, ntowrite;

    total = 0;
    if (!ios_eof(from)) {
        do {
            avail = ios_readprep(from, IOS_BUFSIZE / 2);
            if (avail == 0) {
                from->_eof = 1;
                break;
            }
            ntowrite = (avail <= nbytes || all) ? avail : nbytes;
            written = ios_write(to, from->buf + from->bpos, ntowrite);
            // TODO: should this be +=written instead?
            from->bpos += ntowrite;
            total += written;
            if (!all) {
                nbytes -= written;
                if (nbytes == 0)
                    break;
            }
            if (written < ntowrite)
                break;
        } while (!ios_eof(from));
    }
    return total;
}

size_t ios_copy(struct ios *to, struct ios *from, size_t nbytes)
{
    return ios_copy_(to, from, nbytes, 0);
}

size_t ios_copyall(struct ios *to, struct ios *from)
{
    return ios_copy_(to, from, 0, 1);
}

#define LINE_CHUNK_SIZE 160

size_t ios_copyuntil(struct ios *to, struct ios *from, char delim)
{
    size_t total, avail, ntowrite, written;
    char *pd;
    int first;

    total = 0;
    avail = from->size - from->bpos;
    first = 1;
    if (!ios_eof(from)) {
        do {
            if (avail == 0) {
                first = 0;
                avail = ios_readprep(from, LINE_CHUNK_SIZE);
            }
            pd = (char *)memchr(from->buf + from->bpos, delim, avail);
            if (pd == NULL) {
                written = ios_write(to, from->buf + from->bpos, avail);
                from->bpos += avail;
                total += written;
                avail = 0;
            } else {
                ntowrite = pd - (from->buf + from->bpos) + 1;
                written = ios_write(to, from->buf + from->bpos, ntowrite);
                from->bpos += ntowrite;
                total += written;
                return total;
            }
        } while (!ios_eof(from) && (first || avail >= LINE_CHUNK_SIZE));
    }
    from->_eof = 1;
    return total;
}

static void _ios_init(struct ios *s)
{
    // put all fields in a sane initial state
    s->bm = bm_block;
    s->state = bst_none;
    s->errcode = 0;
    s->buf = NULL;
    s->maxsize = 0;
    s->size = 0;
    s->bpos = 0;
    s->ndirty = 0;
    s->fpos = -1;
    s->lineno = 1;
    s->fd = -1;
    s->ownbuf = 1;
    s->ownfd = 0;
    s->_eof = 0;
    s->rereadable = 0;
    s->readonly = 0;
}

/* stream object initializers. we do no allocation. */

struct ios *ios_file(struct ios *s, char *fname, int rd, int wr, int create,
                     int trunc)
{
    int fd, flags;

    if (!(rd || wr))
        // must specify read and/or write
        goto open_file_err;
    flags = wr ? (rd ? O_RDWR : O_WRONLY) : O_RDONLY;
    if (create)
        flags |= O_CREAT;
    if (trunc)
        flags |= O_TRUNC;
    fd = open(fname, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH /*644*/);
    s = ios_fd(s, fd, 1, 1);
    if (fd == -1)
        goto open_file_err;
    if (!wr)
        s->readonly = 1;
    return s;
open_file_err:
    s->fd = -1;
    return NULL;
}

struct ios *ios_mem(struct ios *s, size_t initsize)
{
    _ios_init(s);
    s->bm = bm_mem;
    _buf_realloc(s, initsize);
    return s;
}

struct ios *ios_str(struct ios *s, char *str)
{
    size_t n;

    n = strlen(str);
    if (ios_mem(s, n + 1) == NULL)
        return NULL;
    ios_write(s, str, n + 1);
    ios_seek(s, 0);
    return s;
}

struct ios *ios_static_buffer(struct ios *s, char *buf, size_t sz)
{
    ios_mem(s, 0);
    ios_setbuf(s, buf, sz, 0);
    s->size = sz;
    ios_set_readonly(s);
    return s;
}

struct ios *ios_fd(struct ios *s, long fd, int isfile, int own)
{
    _ios_init(s);
    s->fd = fd;
    if (isfile)
        s->rereadable = 1;
    _buf_init(s, bm_block);
    s->ownfd = own;
    if (fd == STDERR_FILENO)
        s->bm = bm_none;
    return s;
}

struct ios *ios_stdin = NULL;
struct ios *ios_stdout = NULL;
struct ios *ios_stderr = NULL;

void ios_init_stdstreams()
{
    ios_stdin = LLT_ALLOC(sizeof(struct ios));
    ios_fd(ios_stdin, STDIN_FILENO, 0, 0);

    ios_stdout = LLT_ALLOC(sizeof(struct ios));
    ios_fd(ios_stdout, STDOUT_FILENO, 0, 0);
    ios_stdout->bm = bm_line;

    ios_stderr = LLT_ALLOC(sizeof(struct ios));
    ios_fd(ios_stderr, STDERR_FILENO, 0, 0);
    ios_stderr->bm = bm_none;
}

/* higher level interface */

int ios_putc(int c, struct ios *s)
{
    char ch = (char)c;

    if (s->state == bst_wr && s->bpos < s->maxsize && s->bm != bm_none) {
        s->buf[s->bpos++] = ch;
        _write_update_pos(s);
        if (s->bm == bm_line && ch == '\n')
            ios_flush(s);
        return 1;
    }
    return (int)ios_write(s, &ch, 1);
}

int ios_getc(struct ios *s)
{
    char ch;

    if (s->state == bst_rd && s->bpos < s->size) {
        ch = s->buf[s->bpos++];
    } else {
        if (s->_eof)
            return IOS_EOF;
        if (ios_read(s, &ch, 1) < 1)
            return IOS_EOF;
    }
    if (ch == '\n')
        s->lineno++;
    return (unsigned char)ch;
}

int ios_peekc(struct ios *s)
{
    size_t n;

    if (s->bpos < s->size)
        return (unsigned char)s->buf[s->bpos];
    if (s->_eof)
        return IOS_EOF;
    n = ios_readprep(s, 1);
    if (n == 0)
        return IOS_EOF;
    return (unsigned char)s->buf[s->bpos];
}

int ios_ungetc(int c, struct ios *s)
{
    if (s->state == bst_wr)
        return IOS_EOF;
    if (s->bpos > 0) {
        s->bpos--;
        s->buf[s->bpos] = (char)c;
        s->_eof = 0;
        return c;
    }
    if (s->size == s->maxsize) {
        if (_buf_realloc(s, s->maxsize * 2) == NULL)
            return IOS_EOF;
    }
    memmove(s->buf + 1, s->buf, s->size);
    s->buf[0] = (char)c;
    s->size++;
    s->_eof = 0;
    return c;
}

int ios_getutf8(struct ios *s, uint32_t *pwc)
{
    int c;
    size_t sz, i;
    char c0;
    char buf[8];

    c = ios_getc(s);
    if (c == IOS_EOF)
        return IOS_EOF;
    c0 = (char)c;
    if ((unsigned char)c0 < 0x80) {
        *pwc = (uint32_t)(unsigned char)c0;
        return 1;
    }
    sz = u8_seqlen(&c0) - 1;
    if (ios_ungetc(c, s) == IOS_EOF)
        return IOS_EOF;
    if (ios_readprep(s, sz) < sz)
        // NOTE: this can return EOF even if some bytes are available
        return IOS_EOF;
    i = s->bpos;
    *pwc = u8_nextchar(s->buf, &i);
    ios_read(s, buf, sz + 1);
    return 1;
}

int ios_peekutf8(struct ios *s, uint32_t *pwc)
{
    int c;
    size_t sz, i;
    char c0;

    c = ios_peekc(s);
    if (c == IOS_EOF)
        return IOS_EOF;
    c0 = (char)c;
    if ((unsigned char)c0 < 0x80) {
        *pwc = (uint32_t)(unsigned char)c0;
        return 1;
    }
    sz = u8_seqlen(&c0) - 1;
    if (ios_readprep(s, sz) < sz)
        return IOS_EOF;
    i = s->bpos;
    *pwc = u8_nextchar(s->buf, &i);
    return 1;
}

int ios_pututf8(struct ios *s, uint32_t wc)
{
    char buf[8];
    size_t n;

    if (wc < 0x80)
        return ios_putc((int)wc, s);
    n = u8_toutf8(buf, 8, &wc, 1);
    return ios_write(s, buf, n);
}

void ios_purge(struct ios *s)
{
    if (s->state == bst_rd) {
        s->bpos = s->size;
    }
}

char *ios_readline(struct ios *s)
{
    struct ios dest;
    size_t n;

    ios_mem(&dest, 0);
    ios_copyuntil(&dest, s, '\n');
    return ios_takebuf(&dest, &n);
}

int ios_vprintf(struct ios *s, const char *format, va_list args)
{
    char *str;
    va_list al;
    size_t avail;
    int len;
    char *start;

    va_copy(al, args);
    if (s->state == bst_wr && s->bpos < s->maxsize && s->bm != bm_none) {
        avail = s->maxsize - s->bpos;
        start = s->buf + s->bpos;
        len = vsnprintf(start, avail, format, args);
        if (len < 0) {
            goto done;
        }
        if (avail > (size_t)len) {
            s->bpos += (size_t)len;
            _write_update_pos(s);
            // TODO: only works right if newline is at end
            if (s->bm == bm_line && our_memrchr(start, '\n', (size_t)len)) {
                ios_flush(s);
            }
            goto done;
        }
    }
    len = vsnprintf(NULL, 0, format, al);
    if (len <= 0) {
        len = 0;
        goto done;
    }
    if (!(str = calloc(1, len + 1))) {
        goto done;
    }
    len = vsnprintf(str, len, format, al);
    ios_write(s, str, len);
    LLT_FREE(str);
done:
    va_end(al);
    return len;
}

int ios_printf(struct ios *s, const char *format, ...)
{
    va_list args;
    int c;

    va_start(args, format);
    c = ios_vprintf(s, format, args);
    va_end(args);
    return c;
}
