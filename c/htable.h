#ifndef __HTABLE_H_
#define __HTABLE_H_

#define HT_N_INLINE 32

struct htable {
    size_t size;
    void **table;
    void *_space[HT_N_INLINE];
};

// define this to be an invalid key/value
#define HT_NOTFOUND ((void *)1)

// initialize and free
struct htable *htable_new(struct htable *h, size_t size);
void htable_free(struct htable *h);

// clear and (possibly) change size
void htable_reset(struct htable *h, size_t sz);

#endif
