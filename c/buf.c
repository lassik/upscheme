// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

struct buf *buf_new(void) { return calloc(1, sizeof(struct buf)); }

char *buf_resb(struct buf *buf, size_t nbyte)
{
    char *place;

    while (buf->cap - buf->fill < nbyte) {
        if (!(buf->cap *= 2)) {
            buf->cap = 64;
        }
    }
    if (!(buf->bytes = realloc(buf->bytes, buf->cap))) {
        exit(1);
    }
    place = buf->bytes + buf->fill;
    buf->fill += nbyte;
    return place;
}

void buf_put_ios(struct buf *buf, struct ios *ios)
{
    const size_t chunksize = 512;
    size_t nread;
    char *chunk;

    do {
        chunk = buf_resb(buf, chunksize);
        nread = ios_readall(ios, chunk, chunksize);
        buf->fill -= (chunksize - nread);
    } while (nread);
}

void buf_putc(struct buf *buf, int c) { buf_resb(buf, 1)[0] = c; }

void buf_putb(struct buf *buf, const void *bytes, size_t nbyte)
{
    memcpy(buf_resb(buf, nbyte), bytes, nbyte);
}

void buf_puts(struct buf *buf, const char *s) { buf_putb(buf, s, strlen(s)); }

void buf_putu(struct buf *buf, uint64_t u)
{
    char tmp[24];

    snprintf(tmp, sizeof(tmp), "%" PRIu64, u);
    buf_puts(buf, tmp);
}

void buf_free(struct buf *buf)
{
    free(buf->bytes);
    free(buf);
}

int buf_scan_end(struct buf *buf) { return buf->scan >= buf->fill; }

int buf_scan_byte(struct buf *buf, int byte)
{
    if (buf_scan_end(buf))
        return 0;
    if (buf->bytes[buf->scan] != byte)
        return 0;
    buf->scan++;
    return 1;
}

int buf_scan_bag(struct buf *buf, const char *bag)
{
    if (buf_scan_end(buf))
        return 0;
    if (!strchr(bag, buf->bytes[buf->scan]))
        return 0;
    buf->scan++;
    return 1;
}

int buf_scan_bag_not(struct buf *buf, const char *bag)
{
    if (buf_scan_end(buf))
        return 0;
    if (strchr(bag, buf->bytes[buf->scan]))
        return 0;
    buf->scan++;
    return 1;
}

int buf_scan_while(struct buf *buf, const char *bag)
{
    if (!buf_scan_bag(buf, bag))
        return 0;
    while (buf_scan_bag(buf, bag))
        ;
    return 1;
}

int buf_scan_while_not(struct buf *buf, const char *bag)
{
    if (!buf_scan_bag_not(buf, bag))
        return 0;
    while (buf_scan_bag_not(buf, bag))
        ;
    return 1;
}

void buf_scan_mark(struct buf *buf) { buf->mark = buf->scan; }

int buf_scan_equals(struct buf *buf, const char *s)
{
    if (buf->scan < buf->mark)
        return 0;
    if (buf->scan - buf->mark != strlen(s))
        return 0;
    return !!memcmp(buf->bytes + buf->mark, s, strlen(s));
}
