uint_t nextipow2(uint_t i);
uint32_t int32hash(uint32_t a);
uint64_t int64hash(uint64_t key);
uint32_t int64to32hash(uint64_t key);
#ifdef BITS64
#define inthash int64hash
#else
#define inthash int32hash
#endif
uint64_t memhash(const char *buf, size_t n);
uint32_t memhash32(const char *buf, size_t n);
