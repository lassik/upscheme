// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: BSD-3-Clause

struct buf {
    size_t cap;
    size_t fill;
    char *bytes;
};

struct buf *buf_new(void);
char *buf_resb(struct buf *buf, size_t nbyte);
void buf_put_ios(struct buf *buf, struct ios *ios);
void buf_putc(struct buf *buf, int c);
void buf_putb(struct buf *buf, const void *bytes, size_t nbyte);
void buf_puts(struct buf *buf, const char *s);
void buf_putu(struct buf *buf, uint64_t u);
void buf_free(struct buf *buf);
