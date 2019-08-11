// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"

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

void buf_putc(struct buf *buf, int c) { buf_resb(buf, 1)[0] = c; }

void buf_putb(struct buf *buf, const void *bytes, size_t nbyte)
{
    memcpy(buf_resb(buf, nbyte), bytes, nbyte);
}

void buf_puts(struct buf *buf, const char *s) { buf_putb(buf, s, strlen(s)); }

void buf_putu(struct buf *buf, uint64_t u)
{
    char tmp[24];

    snprintf(tmp, sizeof(tmp), "%llu", u);
    buf_puts(buf, tmp);
}

void buf_free(struct buf *buf)
{
    free(buf->bytes);
    free(buf);
}
