// Copyright 2008 Jeff Bezanson
// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: BSD-3-Clause

// This Scheme only runs on machines with the following characteristics:
//
// - supports integer word sizes of 8, 16, 32, and 64 bits
// - uses unsigned and signed 2's complement representations
// - all pointer types are the same size
// - there is an integer type with the same size as a pointer
//
// Some features require:
//
// - IEEE 754 single- and double-precision floating point
//
// We assume the LP64 convention for 64-bit platforms.

#ifdef __DJGPP__
#include "scheme_compiler_djgpp.h"
#endif

#ifdef __DMC__
#include "scheme_compiler_dmc.h"
#endif

#ifdef __GNUC__
#include "scheme_compiler_gnuc.h"
#endif

#ifdef _MSC_VER
#include "scheme_compiler_msc.h"
#endif

#ifdef __WATCOMC__
#include "scheme_compiler_watcomc.h"
#endif

#define ALIGN(x, sz) (((x) + (sz - 1)) & (-sz))

extern double D_PNAN;
extern double D_NNAN;
extern double D_PINF;
extern double D_NINF;
extern float F_PNAN;
extern float F_NNAN;
extern float F_PINF;
extern float F_NINF;

//// #include "utils.h"

char *uint2str(char *dest, size_t len, uint64_t num, uint32_t base);
int str2int(char *str, size_t len, int64_t *res, uint32_t base);
int isdigit_base(char c, int base);

//// #include "utf8.h"

extern int locale_is_utf8;

#ifdef _WIN32
extern int wcwidth(uint32_t);
#endif

// is c the start of a utf8 sequence?
#define isutf(c) (((c)&0xC0) != 0x80)

#define UEOF ((uint32_t)-1)

// convert UTF-8 data to wide character
size_t u8_toucs(uint32_t *dest, size_t sz, const char *src, size_t srcsz);

// the opposite conversion
size_t u8_toutf8(char *dest, size_t sz, const uint32_t *src, size_t srcsz);

// single character to UTF-8, returns # bytes written
size_t u8_wc_toutf8(char *dest, uint32_t ch);

// character number to byte offset
size_t u8_offset(const char *str, size_t charnum);

// byte offset to character number
size_t u8_charnum(const char *s, size_t offset);

// return next character, updating an index variable
uint32_t u8_nextchar(const char *s, size_t *i);

// next character without NUL character terminator
uint32_t u8_nextmemchar(const char *s, size_t *i);

// move to next character
void u8_inc(const char *s, size_t *i);

// move to previous character
void u8_dec(const char *s, size_t *i);

// returns length of next utf-8 sequence
size_t u8_seqlen(const char *s);

// returns the # of bytes needed to encode a certain character
size_t u8_charlen(uint32_t ch);

// computes the # of bytes needed to encode a WC string as UTF-8
size_t u8_codingsize(uint32_t *wcstr, size_t n);

char read_escape_control_char(char c);

// assuming src points to the character after a backslash, read an
// escape sequence, storing the result in dest and returning the number of
// input characters processed
size_t u8_read_escape_sequence(const char *src, size_t ssz, uint32_t *dest);

// given a wide character, convert it to an ASCII escape sequence stored in
// buf, where buf is "sz" bytes. returns the number of characters output.
// sz must be at least 3.
int u8_escape_wchar(char *buf, size_t sz, uint32_t ch);

// convert a string "src" containing escape sequences to UTF-8
size_t u8_unescape(char *buf, size_t sz, const char *src);

// convert UTF-8 "src" to escape sequences.
//
//   sz is buf size in bytes. must be at least 12.
//
//   if escape_quotes is nonzero, quote characters will be escaped.
//
//   if ascii is nonzero, the output is 7-bit ASCII, no UTF-8 survives.
//
//   starts at src[*pi], updates *pi to point to the first unprocessed
//   byte of the input.
//
//   end is one more than the last allowable value of *pi.
//
//   returns number of bytes placed in buf, including a NUL terminator.
//
size_t u8_escape(char *buf, size_t sz, const char *src, size_t *pi,
                 size_t end, int escape_quotes, int ascii);

// utility predicates used by the above
int octal_digit(char c);
int hex_digit(char c);

// return a pointer to the first occurrence of ch in s, or NULL if not
// found. character index of found character returned in *charn.
char *u8_strchr(const char *s, uint32_t ch, size_t *charn);

// same as the above, but searches a buffer of a given size instead of
// a NUL-terminated string.
char *u8_memchr(const char *s, uint32_t ch, size_t sz, size_t *charn);

char *u8_memrchr(const char *s, uint32_t ch, size_t sz);

// count the number of characters in a UTF-8 string
size_t u8_strlen(const char *s);

// number of columns occupied by a string
size_t u8_strwidth(const char *s);

int u8_is_locale_utf8(const char *locale);

// printf where the format string and arguments may be in UTF-8.
// you can avoid this function and just use ordinary printf() if the current
// locale is UTF-8.
size_t u8_vprintf(const char *fmt, va_list ap);
size_t u8_printf(const char *fmt, ...);

// determine whether a sequence of bytes is valid UTF-8. length is in bytes
int u8_isvalid(const char *str, int length);

// reverse a UTF-8 string. len is length in bytes. dest and src must both
// be allocated to at least len+1 bytes. returns 1 for error, 0 otherwise
int u8_reverse(char *dest, char *src, size_t len);

//// #include "ios.h"

// this flag controls when data actually moves out to the underlying I/O
// channel. memory streams are a special case of this where the data
// never moves out.
typedef enum { bm_none, bm_line, bm_block, bm_mem } bufmode_t;

typedef enum { bst_none, bst_rd, bst_wr } bufstate_t;

#define IOS_INLSIZE 54
#define IOS_BUFSIZE 131072

struct ios {
    bufmode_t bm;

    // the state only indicates where the underlying file position is relative
    // to the buffer. reading: at the end. writing: at the beginning.
    // in general, you can do any operation in any state.
    bufstate_t state;

    int errcode;

    char *buf;       // start of buffer
    size_t maxsize;  // space allocated to buffer
    size_t size;     // length of valid data in buf, >=ndirty
    size_t bpos;     // current position in buffer
    size_t ndirty;   // # bytes at &buf[0] that need to be written

    off_t fpos;     // cached file pos
    size_t lineno;  // current line number

    // pointer-size integer to support platforms where it might have
    // to be a pointer
    long fd;

    unsigned char readonly : 1;
    unsigned char ownbuf : 1;
    unsigned char ownfd : 1;
    unsigned char _eof : 1;

    // this means you can read, seek back, then read the same data
    // again any number of times. usually only true for files and strings.
    unsigned char rereadable : 1;

    char local[IOS_INLSIZE];
};

// low-level interface functions
size_t ios_read(struct ios *s, char *dest, size_t n);
size_t ios_readall(struct ios *s, char *dest, size_t n);
size_t ios_write(struct ios *s, char *data, size_t n);
off_t ios_seek(struct ios *s, off_t pos);  // absolute seek
off_t ios_seek_end(struct ios *s);
off_t ios_skip(struct ios *s, off_t offs);  // relative seek
off_t ios_pos(struct ios *s);               // get current position
size_t ios_trunc(struct ios *s, size_t size);
int ios_eof(struct ios *s);
int ios_flush(struct ios *s);
void ios_close(struct ios *s);
char *ios_takebuf(struct ios *s,
                  size_t *psize);  // release buffer to caller
// set buffer space to use
int ios_setbuf(struct ios *s, char *buf, size_t size, int own);
int ios_bufmode(struct ios *s, bufmode_t mode);
void ios_set_readonly(struct ios *s);
size_t ios_copy(struct ios *to, struct ios *from, size_t nbytes);
size_t ios_copyall(struct ios *to, struct ios *from);
size_t ios_copyuntil(struct ios *to, struct ios *from, char delim);
// ensure at least n bytes are buffered if possible. returns # available.
size_t ios_readprep(struct ios *from, size_t n);
// void ios_lock(struct ios *s);
// int struct iosrylock(struct ios *s);
// int ios_unlock(struct ios *s);

// stream creation
struct ios *ios_file(struct ios *s, char *fname, int rd, int wr, int create,
                     int trunc);
struct ios *ios_mem(struct ios *s, size_t initsize);
struct ios *ios_str(struct ios *s, char *str);
struct ios *ios_static_buffer(struct ios *s, char *buf, size_t sz);
struct ios *ios_fd(struct ios *s, long fd, int isfile, int own);
// todo: ios_socket
extern struct ios *ios_stdin;
extern struct ios *ios_stdout;
extern struct ios *ios_stderr;
void ios_init_stdstreams();

// high-level functions - output
int ios_putnum(struct ios *s, char *data, uint32_t type);
int ios_putint(struct ios *s, int n);
int ios_pututf8(struct ios *s, uint32_t wc);
int ios_putstringz(struct ios *s, char *str, int do_write_nulterm);
int ios_printf(struct ios *s, const char *format, ...);
int ios_vprintf(struct ios *s, const char *format, va_list args);

void hexdump(struct ios *dest, const char *buffer, size_t len,
             size_t startoffs);

// high-level stream functions - input
int ios_getnum(struct ios *s, char *data, uint32_t type);
int ios_getutf8(struct ios *s, uint32_t *pwc);
int ios_peekutf8(struct ios *s, uint32_t *pwc);
int ios_ungetutf8(struct ios *s, uint32_t wc);
int ios_getstringz(struct ios *dest, struct ios *src);
int ios_getstringn(struct ios *dest, struct ios *src, size_t nchars);
int ios_getline(struct ios *s, char **pbuf, size_t *psz);
char *ios_readline(struct ios *s);

// discard data buffered for reading
void ios_purge(struct ios *s);

// seek by utf8 sequence increments
int ios_nextutf8(struct ios *s);
int ios_prevutf8(struct ios *s);

// stdio-style functions
#define IOS_EOF (-1)
int ios_putc(int c, struct ios *s);
// wint_t ios_putwc(struct ios *s, wchar_t wc);
int ios_getc(struct ios *s);
int ios_peekc(struct ios *s);
// wint_t ios_getwc(struct ios *s);
int ios_ungetc(int c, struct ios *s);
// wint_t ios_ungetwc(struct ios *s, wint_t wc);
#define ios_puts(str, s) ios_write(s, str, strlen(str))

//  With memory streams, mixed reads and writes are equivalent to performing
//  sequences of *p++, as either an lvalue or rvalue. File streams behave
//  similarly, but other streams might not support this. Using unbuffered
//  mode makes this more predictable.
//
//  Note on "unget" functions:
//  There are two kinds of functions here: those that operate on sized
//  blocks of bytes and those that operate on logical units like "character"
//  or "integer". The "unget" functions only work on logical units. There
//  is no "unget n bytes". You can only do an unget after a matching get.
//  However, data pushed back by an unget is available to all read operations.
//  The reason for this is that unget is defined in terms of its effect on
//  the underlying buffer (namely, it rebuffers data as if it had been
//  buffered but not read yet). IOS reserves the right to perform large block
//  operations directly, bypassing the buffer. In such a case data was
//  never buffered, so "rebuffering" has no meaning (i.e. there is no
//  correspondence between the buffer and the physical stream).
//
//  Single-bit I/O is able to write partial bytes ONLY IF the stream supports
//  seeking. Also, line buffering is not well-defined in the context of
//  single-bit I/O, so it might not do what you expect.
//
//  implementation notes:
//  in order to know where we are in a file, we must ensure the buffer
//  is only populated from the underlying stream starting with p==buf.
//
//  to switch from writing to reading: flush, set p=buf, cnt=0
//  to switch from reading to writing: seek backwards cnt bytes, p=buf, cnt=0
//
//  when writing: buf starts at curr. physical stream pos, p - buf is how
//  many bytes we've written logically. cnt==0
//
//  dirty == (bitpos>0 && state==iost_wr), EXCEPT right after switching from
//  reading to writing, where we might be in the middle of a byte without
//  having changed it.
//
//  to write a bit: if !dirty, read up to maxsize-(p-buf) into buffer, then
//  seek back by the same amount (undo it). write onto those bits. now set
//  the dirty bit. in this state, we can bit-read up to the end of the byte,
//  then formally switch to the read state using flush.
//
//  design points:
//  - data-source independence, including memory streams
//  - expose buffer to user, allow user-owned buffers
//  - allow direct I/O, don't always go through buffer
//  - buffer-internal seeking. makes seeking back 1-2 bytes very fast,
//    and makes it possible for sockets where it otherwise wouldn't be
//  - tries to allow switching between reading and writing
//  - support 64-bit and large files
//  - efficient, low-latency buffering
//  - special support for utf8
//  - type-aware functions with byte-order swapping service
//  - position counter for meaningful data offsets with sockets
//
//  theory of operation:
//
//  the buffer is a view of part of a file/stream. you can seek, read, and
//  write around in it as much as you like, as if it were just a string.
//
//  we keep track of the part of the buffer that's invalid (written to).
//  we remember whether the position of the underlying stream is aligned
//  with the end of the buffer (reading mode) or the beginning (writing mode).
//
//  based on this info, we might have to seek back before doing a flush.
//
//  as optimizations, we do no writing if the buffer isn't "dirty", and we
//  do no reading if the data will only be overwritten.

//// #include "socket.h"

#ifndef _WIN32
void closesocket(int fd);
#endif

int open_tcp_port(short portno);
int open_any_tcp_port(short *portno);
int open_any_udp_port(short *portno);
int connect_to_host(char *hostname, short portno);
int sendall(int sockfd, char *buffer, int bufLen, int flags);
int readall(int sockfd, char *buffer, int bufLen, int flags);
int socket_ready(int sock);

//// #include "timefuncs.h"

uint64_t i64time();
void sleep_ms(int ms);
void timeparts(int32_t *buf, double t);

//// #include "hashing.h"

uintptr_t nextipow2(uintptr_t i);
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

//// #include "htable.h"

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

//// #include "bitvector.h"

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
                      int scrap);
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

//// #include "os.h"

void path_to_dirname(char *path);
void get_cwd(char *buf, size_t size);
int set_cwd(char *buf);
char *get_exename(char *buf, size_t size);
int os_path_exists(const char *path);
void os_setenv(const char *name, const char *value);

value_t builtin_os_open_directory(value_t *args, uint32_t nargs);
value_t builtin_os_read_directory(value_t *args, uint32_t nargs);
value_t builtin_os_close_directory(value_t *args, uint32_t nargs);
void os_init(void);

//// #include "random.h"

#define random() genrand_int32()
#define srandom(n) init_genrand(n)
double rand_double();
float rand_float();
double randn();
void randomize();
uint32_t genrand_int32();
void init_genrand(uint32_t s);
uint64_t i64time();

//// #include "llt.h"

void llt_init();

//// #include "ieee754.h"

union ieee754_float {
    float f;

    struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        unsigned int negative : 1;
        unsigned int exponent : 8;
        unsigned int mantissa : 23;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        unsigned int mantissa : 23;
        unsigned int exponent : 8;
        unsigned int negative : 1;
#endif
    } ieee;
};

#define IEEE754_FLOAT_BIAS 0x7f

union ieee754_double {
    double d;

    struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        unsigned int negative : 1;
        unsigned int exponent : 11;
        unsigned int mantissa0 : 20;
        unsigned int mantissa1 : 32;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        unsigned int mantissa1 : 32;
        unsigned int mantissa0 : 20;
        unsigned int exponent : 11;
        unsigned int negative : 1;
#endif
    } ieee;
};

#define IEEE754_DOUBLE_BIAS 0x3ff

union ieee854_long_double {
    long double d;

    struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        unsigned int negative : 1;
        unsigned int exponent : 15;
        unsigned int empty : 16;
        unsigned int mantissa0 : 32;
        unsigned int mantissa1 : 32;
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        unsigned int mantissa1 : 32;
        unsigned int mantissa0 : 32;
        unsigned int exponent : 15;
        unsigned int negative : 1;
        unsigned int empty : 16;
#endif
    } ieee;
};

#define IEEE854_LONG_DOUBLE_BIAS 0x3fff

//// #include "flisp.h"

struct cons {
    value_t car;
    value_t cdr;
};

struct symbol {
    uintptr_t flags;
    value_t binding;  // global value binding
    struct fltype *type;
    uint32_t hash;
    void *dlcache;  // dlsym address
    // below fields are private
    struct symbol *left;
    struct symbol *right;
    union {
        char name[1];
        void *_pad;  // ensure field aligned to pointer size
    };
};

struct gensym {
    value_t isconst;
    value_t binding;  // global value binding
    struct fltype *type;
    uint32_t id;
};

#define TAG_NUM 0x0
#define TAG_CPRIM 0x1
#define TAG_FUNCTION 0x2
#define TAG_VECTOR 0x3
#define TAG_NUM1 0x4
#define TAG_CVALUE 0x5
#define TAG_SYM 0x6
#define TAG_CONS 0x7
#define UNBOUND ((value_t)0x1)  // an invalid value
#define TAG_FWD UNBOUND
#define tag(x) ((x)&0x7)
#define ptr(x) ((void *)((x) & (~(value_t)0x7)))
#define tagptr(p, t) (((value_t)(p)) | (t))
#define fixnum(x) ((value_t)(((ufixnum_t)(fixnum_t)(x)) << 2))
#define numval(x) (((fixnum_t)(x)) >> 2)
#ifdef BITS64
#define fits_fixnum(x) (((x) >> 61) == 0 || (~((x) >> 61)) == 0)
#else
#define fits_fixnum(x) (((x) >> 29) == 0 || (~((x) >> 29)) == 0)
#endif
#define fits_bits(x, b) (((x) >> (b - 1)) == 0 || (~((x) >> (b - 1))) == 0)
#define uintval(x) (((unsigned int)(x)) >> 3)
#define builtin(n) tagptr((((int)n) << 3), TAG_FUNCTION)
#define iscons(x) (tag(x) == TAG_CONS)
#define issymbol(x) (tag(x) == TAG_SYM)
#define isfixnum(x) (((x)&3) == TAG_NUM)
#define bothfixnums(x, y) ((((x) | (y)) & 3) == TAG_NUM)
#define isbuiltin(x) ((tag(x) == TAG_FUNCTION) && uintval(x) <= OP_ASET)
#define isvector(x) (tag(x) == TAG_VECTOR)
#define iscvalue(x) (tag(x) == TAG_CVALUE)
#define iscprim(x) (tag(x) == TAG_CPRIM)
#define selfevaluating(x) (tag(x) < 6)
// comparable with ==
#define eq_comparable(a, b) (!(((a) | (b)) & 1))
#define eq_comparablep(a) (!((a)&1))
// doesn't lead to other values
#define leafp(a) (((a)&3) != 3)

#define isforwarded(v) (((value_t *)ptr(v))[0] == TAG_FWD)
#define forwardloc(v) (((value_t *)ptr(v))[1])
#define forward(v, to)                      \
    do {                                    \
        (((value_t *)ptr(v))[0] = TAG_FWD); \
        (((value_t *)ptr(v))[1] = to);      \
    } while (0)

#define vector_size(v) (((size_t *)ptr(v))[0] >> 2)
#define vector_setsize(v, n) (((size_t *)ptr(v))[0] = ((n) << 2))
#define vector_elt(v, i) (((value_t *)ptr(v))[1 + (i)])
#define vector_grow_amt(x) ((x) < 8 ? 5 : 6 * ((x) >> 3))
// functions ending in _ are unsafe, faster versions
#define car_(v) (((struct cons *)ptr(v))->car)
#define cdr_(v) (((struct cons *)ptr(v))->cdr)
#define car(v) (tocons((v), "car")->car)
#define cdr(v) (tocons((v), "cdr")->cdr)
#define fn_bcode(f) (((value_t *)ptr(f))[0])
#define fn_vals(f) (((value_t *)ptr(f))[1])
#define fn_env(f) (((value_t *)ptr(f))[2])
#define fn_name(f) (((value_t *)ptr(f))[3])

#define set(s, v) (((struct symbol *)ptr(s))->binding = (v))
#define setc(s, v)                                \
    do {                                          \
        ((struct symbol *)ptr(s))->flags |= 1;    \
        ((struct symbol *)ptr(s))->binding = (v); \
    } while (0)
#define isconstant(s) ((s)->flags & 0x1)
#define iskeyword(s) ((s)->flags & 0x2)
#define symbol_value(s) (((struct symbol *)ptr(s))->binding)
#define ismanaged(v)                             \
    ((((unsigned char *)ptr(v)) >= fromspace) && \
     (((unsigned char *)ptr(v)) < fromspace + heapsize))
#define isgensym(x) (issymbol(x) && ismanaged(x))

#define isfunction(x) (tag(x) == TAG_FUNCTION && (x) > (N_BUILTINS << 3))
#define isclosure(x) isfunction(x)
#define iscbuiltin(x) \
    (iscvalue(x) && (cv_class((struct cvalue *)ptr(x)) == builtintype))

void fl_gc_handle(value_t *pv);
void fl_free_gc_handles(uint32_t n);

// utility for iterating over all arguments in a builtin
// i=index, i0=start index, arg = var for each arg, args = arg array
// assumes "nargs" is the argument count
#define FOR_ARGS(i, i0, arg, args) \
    for (i = i0; ((size_t)i) < nargs && ((arg = args[i]) || 1); i++)

#define N_BUILTINS ((int)N_OPCODES)

extern value_t FL_NIL, FL_T, FL_F, FL_EOF;

#define FL_UNSPECIFIED FL_T

// read, eval, print main entry points
value_t fl_read_sexpr(value_t f);
void fl_print(struct ios *f, value_t v);
value_t fl_toplevel_eval(value_t expr);
value_t fl_apply(value_t f, value_t l);
value_t fl_applyn(uint32_t n, value_t f, ...);

extern value_t printprettysym, printreadablysym, printwidthsym;

// object model manipulation
value_t fl_cons(value_t a, value_t b);
value_t fl_list2(value_t a, value_t b);
value_t fl_listn(size_t n, ...);
value_t symbol(const char *str);
char *symbol_name(value_t v);
int fl_is_keyword_name(const char *str, size_t len);
value_t alloc_vector(size_t n, int init);
size_t llength(value_t v);
value_t fl_compare(value_t a, value_t b);  // -1, 0, or 1
value_t fl_equal(value_t a, value_t b);    // T or nil
int equal_lispvalue(value_t a, value_t b);
uintptr_t hash_lispvalue(value_t a);
int isnumtok_base(char *tok, value_t *pval, int base);

// safe casts
struct cons *tocons(value_t v, char *fname);
struct symbol *tosymbol(value_t v, char *fname);
fixnum_t tofixnum(value_t v, char *fname);
char *tostring(value_t v, char *fname);

// error handling
struct fl_readstate {
    struct htable backrefs;
    struct htable gensyms;
    value_t source;
    struct fl_readstate *prev;
};

struct fl_exception_context {
    jmp_buf buf;
    uint32_t sp;
    uint32_t frame;
    uint32_t ngchnd;
    struct fl_readstate *rdst;
    struct fl_exception_context *prev;
};

extern struct fl_exception_context *fl_ctx;
extern uint32_t fl_throwing_frame;
extern value_t fl_lasterror;

#define FL_TRY_EXTERN                 \
    struct fl_exception_context _ctx; \
    int l__tr, l__ca;                 \
    fl_savestate(&_ctx);              \
    fl_ctx = &_ctx;                   \
    if (!setjmp(_ctx.buf))            \
        for (l__tr = 1; l__tr; l__tr = 0, (void)(fl_ctx = fl_ctx->prev))

#define FL_CATCH_EXTERN \
    else for (l__ca = 1; l__ca; l__ca = 0, fl_restorestate(&_ctx))

void fl_savestate(struct fl_exception_context *_ctx);
void fl_restorestate(struct fl_exception_context *_ctx);
extern value_t ArgError, IOError, KeyError, MemoryError, EnumerationError;
extern value_t UnboundError;

struct cvtable {
    void (*print)(value_t self, struct ios *f);
    void (*relocate)(value_t oldv, value_t newv);
    void (*finalize)(value_t self);
    void (*print_traverse)(value_t self);
};

// functions needed to implement the value interface (struct cvtable)
typedef enum {
    T_INT8,
    T_UINT8,
    T_INT16,
    T_UINT16,
    T_INT32,
    T_UINT32,
    T_INT64,
    T_UINT64,
    T_FLOAT,
    T_DOUBLE
} numerictype_t;

#define N_NUMTYPES ((int)T_DOUBLE + 1)

#ifdef BITS64
#define T_FIXNUM T_INT64
#else
#define T_FIXNUM T_INT32
#endif

value_t relocate_lispvalue(value_t v);
void print_traverse(value_t v);
void fl_print_chr(char c, struct ios *f);
void fl_print_str(char *s, struct ios *f);
void fl_print_child(struct ios *f, value_t v);

typedef int (*cvinitfunc_t)(struct fltype *, value_t, void *);

struct fltype {
    value_t type;
    numerictype_t numtype;
    size_t size;
    size_t elsz;
    struct cvtable *vtable;
    struct fltype *eltype;  // for arrays
    struct fltype *artype;  // (array this)
    int marked;
    cvinitfunc_t init;
};

struct cvalue {
    struct fltype *type;
    void *data;
    size_t len;  // length of *data in bytes
    union {
        value_t parent;  // optional
        char _space[1];  // variable size
    };
};

#define CVALUE_NWORDS 4

struct cprim {
    struct fltype *type;
    char _space[1];
};

struct function {
    value_t bcode;
    value_t vals;
    value_t env;
    value_t name;
};

#define CPRIM_NWORDS 2
#define MAX_INL_SIZE 384

#define CV_OWNED_BIT 0x1
#define CV_PARENT_BIT 0x2
#define owned(cv) ((uintptr_t)(cv)->type & CV_OWNED_BIT)
#define hasparent(cv) ((uintptr_t)(cv)->type & CV_PARENT_BIT)
#define isinlined(cv) ((cv)->data == &(cv)->_space[0])
#define cv_class(cv) ((struct fltype *)(((uintptr_t)(cv)->type) & ~3))
#define cv_len(cv) ((cv)->len)
#define cv_type(cv) (cv_class(cv)->type)
#define cv_data(cv) ((cv)->data)
#define cv_isstr(cv) (cv_class(cv)->eltype == bytetype)
#define cv_isPOD(cv) (cv_class(cv)->init != NULL)

#define cvalue_data(v) cv_data((struct cvalue *)ptr(v))
#define cvalue_len(v) cv_len((struct cvalue *)ptr(v))
#define value2c(type, v) ((type)cv_data((struct cvalue *)ptr(v)))

#define valid_numtype(v) ((v) < N_NUMTYPES)
#define cp_class(cp) ((cp)->type)
#define cp_type(cp) (cp_class(cp)->type)
#define cp_numtype(cp) (cp_class(cp)->numtype)
#define cp_data(cp) (&(cp)->_space[0])

// WARNING: multiple evaluation!
#define cptr(v)                                   \
    (iscprim(v) ? cp_data((struct cprim *)ptr(v)) \
                : cv_data((struct cvalue *)ptr(v)))

typedef value_t (*builtin_t)(value_t *, uint32_t);

extern value_t QUOTE;
extern value_t int8sym, uint8sym, int16sym, uint16sym, int32sym, uint32sym;
extern value_t int64sym, uint64sym;
extern value_t longsym, ulongsym, bytesym, wcharsym;
extern value_t structsym, arraysym, enumsym, cfunctionsym, voidsym,
pointersym;
extern value_t stringtypesym, wcstringtypesym, emptystringsym;
extern value_t unionsym, floatsym, doublesym;
extern struct fltype *bytetype, *wchartype;
extern struct fltype *stringtype, *wcstringtype;
extern struct fltype *builtintype;

value_t cvalue(struct fltype *type, size_t sz);
void add_finalizer(struct cvalue *cv);
void cv_autorelease(struct cvalue *cv);
void cv_pin(struct cvalue *cv);
size_t ctype_sizeof(value_t type, int *palign);
value_t cvalue_copy(value_t v);
value_t cvalue_from_data(struct fltype *type, void *data, size_t sz);
value_t cvalue_from_ref(struct fltype *type, void *ptr, size_t sz,
                        value_t parent);
value_t cbuiltin(char *name, builtin_t f);
size_t cvalue_arraylen(value_t v);
value_t size_wrap(size_t sz);
size_t toulong(value_t n, char *fname);
value_t cvalue_string(size_t sz);
value_t cvalue_static_cstring(const char *str);
value_t string_from_cstr(const char *str);
value_t string_from_cstrn(const char *str, size_t n);
int fl_isstring(value_t v);
int fl_isnumber(value_t v);
int fl_isgensym(value_t v);
int fl_isiostream(value_t v);
struct ios *fl_toiostream(value_t v, const char *fname);
value_t cvalue_compare(value_t a, value_t b);
int numeric_compare(value_t a, value_t b, int eq, int eqnans, char *fname);

void to_sized_ptr(value_t v, char *fname, char **pdata, size_t *psz);

struct fltype *get_type(value_t t);
struct fltype *get_array_type(value_t eltype);
struct fltype *define_opaque_type(value_t sym, size_t sz,
                                  struct cvtable *vtab, cvinitfunc_t init);

value_t mk_double(double_t n);
value_t mk_float(float_t n);
value_t mk_uint32(uint32_t n);
value_t mk_uint64(uint64_t n);
value_t mk_wchar(int32_t n);
value_t return_from_uint64(uint64_t Uaccum);
value_t return_from_int64(int64_t Saccum);

numerictype_t effective_numerictype(double r);
double conv_to_double(void *data, numerictype_t tag);
void conv_from_double(void *data, double d, numerictype_t tag);
int64_t conv_to_int64(void *data, numerictype_t tag);
uint64_t conv_to_uint64(void *data, numerictype_t tag);
int32_t conv_to_int32(void *data, numerictype_t tag);
uint32_t conv_to_uint32(void *data, numerictype_t tag);
#ifdef BITS64
#define conv_to_long conv_to_int64
#define conv_to_ulong conv_to_uint64
#else
#define conv_to_long conv_to_int32
#define conv_to_ulong conv_to_uint32
#endif

struct builtinspec {
    char *name;
    builtin_t fptr;
};

void assign_global_builtins(struct builtinspec *b);

// builtins
value_t fl_hash(value_t *args, uint32_t nargs);
value_t cvalue_byte(value_t *args, uint32_t nargs);
value_t cvalue_wchar(value_t *args, uint32_t nargs);

void fl_init(size_t initial_heapsize);
int fl_load_boot_image(void);

//// #include "buf.h"

struct buf {
    size_t cap;
    size_t fill;
    size_t scan;
    size_t mark;
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

int buf_scan_end(struct buf *buf);
int buf_scan_byte(struct buf *buf, int byte);
int buf_scan_bag(struct buf *buf, const char *bag);
int buf_scan_bag_not(struct buf *buf, const char *bag);
int buf_scan_while(struct buf *buf, const char *bag);
int buf_scan_while_not(struct buf *buf, const char *bag);
void buf_scan_mark(struct buf *buf);
int buf_scan_equals(struct buf *buf, const char *s);

//// #include "argcount.h"

void argcount(const char *fname, uint32_t nargs, uint32_t c);

//// #include "env.h"

const char *env_get_os_name(void);

value_t builtin_environment_stack(value_t *args, uint32_t nargs);

//// #include "libraries.h"

value_t builtin_import(value_t *args, uint32_t nargs);

//// #include "builtins.h"

value_t builtin_pid(value_t *args, uint32_t nargs);
value_t builtin_parent_pid(value_t *args, uint32_t nargs);
value_t builtin_process_group(value_t *args, uint32_t nargs);

value_t builtin_user_effective_gid(value_t *args, uint32_t nargs);
value_t builtin_user_effective_uid(value_t *args, uint32_t nargs);
value_t builtin_user_real_gid(value_t *args, uint32_t nargs);
value_t builtin_user_real_uid(value_t *args, uint32_t nargs);

value_t builtin_term_init(value_t *args, uint32_t nargs);
value_t builtin_term_exit(value_t *args, uint32_t nargs);

value_t builtin_spawn(value_t *args, uint32_t nargs);

value_t builtin_read_ini_file(value_t *args, uint32_t nargs);

value_t builtin_color_name_to_rgb24(value_t *args, uint32_t nargs);

//// #include "stringfuncs.h"

value_t fl_stringp(value_t *args, uint32_t nargs);
value_t fl_string_reverse(value_t *args, uint32_t nargs);
value_t fl_string_sub(value_t *args, uint32_t nargs);

// util.c

struct accum {
    value_t list;
    value_t tail;
};

#define ACCUM_EMPTY                    \
    {                                  \
        .list = FL_NIL, .tail = FL_NIL \
    }

void accum_elt(struct accum *accum, value_t elt);
void accum_pair(struct accum *accum, value_t a, value_t d);
void accum_name_value(struct accum *accum, const char *name, value_t value);

// boot_image.c

extern char boot_image[];
extern const size_t boot_image_size;

#include "htableh_inc.h"
