/*
  include this file and call HTIMPL to generate an implementation
*/

#define hash_size(h) ((h)->size / 2)

// compute empirical max-probe for a given size
#define max_probe(size) \
    ((size) <= (HT_N_INLINE * 2) ? (HT_N_INLINE / 2) : (size) >> 3)

#define HTIMPL(HTNAME, HFUNC, EQFUNC)                                        \
    static void **HTNAME##_lookup_bp(struct htable *h, void *key)            \
    {                                                                        \
        uintptr_t hv;                                                        \
        size_t i, orig, index, iter;                                         \
        size_t newsz, sz = hash_size(h);                                     \
        size_t maxprobe = max_probe(sz);                                     \
        void **tab = h->table;                                               \
        void **ol;                                                           \
                                                                             \
        hv = HFUNC((uintptr_t)key);                                          \
    retry_bp:                                                                \
        iter = 0;                                                            \
        index = (uintptr_t)(hv & (sz - 1)) * 2;                              \
        sz *= 2;                                                             \
        orig = index;                                                        \
                                                                             \
        do {                                                                 \
            if (tab[index + 1] == HT_NOTFOUND) {                             \
                tab[index] = key;                                            \
                return &tab[index + 1];                                      \
            }                                                                \
                                                                             \
            if (EQFUNC(key, tab[index]))                                     \
                return &tab[index + 1];                                      \
                                                                             \
            index = (index + 2) & (sz - 1);                                  \
            iter++;                                                          \
            if (iter > maxprobe)                                             \
                break;                                                       \
        } while (index != orig);                                             \
                                                                             \
        /* table full */                                                     \
        /* quadruple size, rehash, retry the insert */                       \
        /* it's important to grow the table really fast; otherwise we waste  \
         */                                                                  \
        /* lots of time rehashing all the keys over and over. */             \
        sz = h->size;                                                        \
        ol = h->table;                                                       \
        if (sz >= (1 << 19) || (sz <= (1 << 8)))                             \
            newsz = sz << 1;                                                 \
        else if (sz <= HT_N_INLINE)                                          \
            newsz = HT_N_INLINE;                                             \
        else                                                                 \
            newsz = sz << 2;                                                 \
        /*printf("trying to allocate %d words.\n", newsz); fflush(stdout);*/ \
        tab = (void **)malloc(newsz * sizeof(void *));                       \
        if (tab == NULL)                                                     \
            return NULL;                                                     \
        for (i = 0; i < newsz; i++)                                          \
            tab[i] = HT_NOTFOUND;                                            \
        h->table = tab;                                                      \
        h->size = newsz;                                                     \
        for (i = 0; i < sz; i += 2) {                                        \
            if (ol[i + 1] != HT_NOTFOUND) {                                  \
                (*HTNAME##_lookup_bp(h, ol[i])) = ol[i + 1];                 \
            }                                                                \
        }                                                                    \
        if (ol != &h->_space[0])                                             \
            free(ol);                                                        \
                                                                             \
        sz = hash_size(h);                                                   \
        maxprobe = max_probe(sz);                                            \
        tab = h->table;                                                      \
                                                                             \
        goto retry_bp;                                                       \
                                                                             \
        return NULL;                                                         \
    }                                                                        \
                                                                             \
    void HTNAME##_put(struct htable *h, void *key, void *val)                \
    {                                                                        \
        void **bp = HTNAME##_lookup_bp(h, key);                              \
                                                                             \
        *bp = val;                                                           \
    }                                                                        \
                                                                             \
    void **HTNAME##_bp(struct htable *h, void *key)                          \
    {                                                                        \
        return HTNAME##_lookup_bp(h, key);                                   \
    }                                                                        \
                                                                             \
    /* returns bp if key is in hash, otherwise NULL */                       \
    /* if return is non-NULL and *bp == HT_NOTFOUND then key was deleted */  \
    static void **HTNAME##_peek_bp(struct htable *h, void *key)              \
    {                                                                        \
        size_t sz = hash_size(h);                                            \
        size_t maxprobe = max_probe(sz);                                     \
        void **tab = h->table;                                               \
        size_t index = (uintptr_t)(HFUNC((uintptr_t)key) & (sz - 1)) * 2;    \
        size_t orig = index;                                                 \
        size_t iter = 0;                                                     \
                                                                             \
        sz *= 2;                                                             \
        do {                                                                 \
            if (tab[index] == HT_NOTFOUND)                                   \
                return NULL;                                                 \
            if (EQFUNC(key, tab[index]))                                     \
                return &tab[index + 1];                                      \
                                                                             \
            index = (index + 2) & (sz - 1);                                  \
            iter++;                                                          \
            if (iter > maxprobe)                                             \
                break;                                                       \
        } while (index != orig);                                             \
                                                                             \
        return NULL;                                                         \
    }                                                                        \
                                                                             \
    void *HTNAME##_get(struct htable *h, void *key)                          \
    {                                                                        \
        void **bp = HTNAME##_peek_bp(h, key);                                \
        if (bp == NULL)                                                      \
            return HT_NOTFOUND;                                              \
        return *bp;                                                          \
    }                                                                        \
                                                                             \
    int HTNAME##_has(struct htable *h, void *key)                            \
    {                                                                        \
        return (HTNAME##_get(h, key) != HT_NOTFOUND);                        \
    }                                                                        \
                                                                             \
    int HTNAME##_remove(struct htable *h, void *key)                         \
    {                                                                        \
        void **bp = HTNAME##_peek_bp(h, key);                                \
        if (bp != NULL) {                                                    \
            *bp = HT_NOTFOUND;                                               \
            return 1;                                                        \
        }                                                                    \
        return 0;                                                            \
    }                                                                        \
                                                                             \
    void HTNAME##_adjoin(struct htable *h, void *key, void *val)             \
    {                                                                        \
        void **bp = HTNAME##_lookup_bp(h, key);                              \
        if (*bp == HT_NOTFOUND)                                              \
            *bp = val;                                                       \
    }
