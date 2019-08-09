// a mask with n set lo or hi bits
#define lomask(n) (uint32_t)((((uint32_t)1) << (n)) - 1)
#define himask(n) (~lomask(32 - n))
#define ONES32 ((uint32_t)0xffffffff)

uint32_t bitreverse(uint32_t x);

uint32_t *bitvector_new(uint64_t n, int initzero);
uint32_t *bitvector_resize(uint32_t *b, uint64_t oldsz, uint64_t newsz,
                           int initzero);
size_t bitvector_nwords(uint64_t nbits);
void bitvector_set(uint32_t *b, uint64_t n, uint32_t c);
uint32_t bitvector_get(uint32_t *b, uint64_t n);

uint32_t bitvector_next(uint32_t *b, uint64_t n0, uint64_t n);

void bitvector_shr(uint32_t *b, size_t n, uint32_t s);
void bitvector_shr_to(uint32_t *dest, uint32_t *b, size_t n, uint32_t s);
void bitvector_shl(uint32_t *b, size_t n, uint32_t s);
void bitvector_shl_to(uint32_t *dest, uint32_t *b, size_t n, uint32_t s,
                      bool_t scrap);
void bitvector_fill(uint32_t *b, uint32_t offs, uint32_t c, uint32_t nbits);
void bitvector_copy(uint32_t *dest, uint32_t doffs, uint32_t *a,
                    uint32_t aoffs, uint32_t nbits);
void bitvector_not(uint32_t *b, uint32_t offs, uint32_t nbits);
void bitvector_not_to(uint32_t *dest, uint32_t doffs, uint32_t *a,
                      uint32_t aoffs, uint32_t nbits);
void bitvector_reverse(uint32_t *b, uint32_t offs, uint32_t nbits);
void bitvector_reverse_to(uint32_t *dest, uint32_t *src, uint32_t soffs,
                          uint32_t nbits);
void bitvector_and_to(uint32_t *dest, uint32_t doffs, uint32_t *a,
                      uint32_t aoffs, uint32_t *b, uint32_t boffs,
                      uint32_t nbits);
void bitvector_or_to(uint32_t *dest, uint32_t doffs, uint32_t *a,
                     uint32_t aoffs, uint32_t *b, uint32_t boffs,
                     uint32_t nbits);
void bitvector_xor_to(uint32_t *dest, uint32_t doffs, uint32_t *a,
                      uint32_t aoffs, uint32_t *b, uint32_t boffs,
                      uint32_t nbits);
uint64_t bitvector_count(uint32_t *b, uint32_t offs, uint64_t nbits);
uint32_t bitvector_any0(uint32_t *b, uint32_t offs, uint32_t nbits);
uint32_t bitvector_any1(uint32_t *b, uint32_t offs, uint32_t nbits);
