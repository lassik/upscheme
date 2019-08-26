/*
  string functions
*/

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "dtypes.h"
#include "utils.h"
#include "utf8.h"
#include "ios.h"
#include "socket.h"
#include "hashing.h"
#include "htable.h"
#include "htableh_inc.h"
#include "bitvector.h"
#include "os.h"
#include "random.h"
#include "llt.h"

#include "flisp.h"

#include "error.h"

#include "argcount.h"

extern void display_defaults(struct ios *f, value_t v);

value_t fl_stringp(value_t *args, uint32_t nargs)
{
    argcount("string?", nargs, 1);
    return fl_isstring(args[0]) ? FL_T : FL_F;
}

value_t fl_string_count(value_t *args, uint32_t nargs)
{
    char *str;
    size_t start, len, stop;

    start = 0;
    if (nargs < 1 || nargs > 3)
        argcount("string.count", nargs, 1);
    if (!fl_isstring(args[0]))
        type_error("string.count", "string", args[0]);
    len = cv_len((struct cvalue *)ptr(args[0]));
    stop = len;
    if (nargs > 1) {
        start = toulong(args[1], "string.count");
        if (start > len)
            bounds_error("string.count", args[0], args[1]);
        if (nargs > 2) {
            stop = toulong(args[2], "string.count");
            if (stop > len)
                bounds_error("string.count", args[0], args[2]);
            if (stop <= start)
                return fixnum(0);
        }
    }
    str = cvalue_data(args[0]);
    return size_wrap(u8_charnum(str + start, stop - start));
}

value_t fl_string_width(value_t *args, uint32_t nargs)
{
    char *s;

    argcount("string.width", nargs, 1);
    if (iscprim(args[0])) {
        struct cprim *cp = (struct cprim *)ptr(args[0]);
        if (cp_class(cp) == wchartype) {
            int w = wcwidth(*(uint32_t *)cp_data(cp));
            if (w < 0)
                return FL_F;
            return fixnum(w);
        }
    }
    s = tostring(args[0], "string.width");
    return size_wrap(u8_strwidth(s));
}

value_t fl_string_reverse(value_t *args, uint32_t nargs)
{
    size_t len;
    value_t ns;

    argcount("string.reverse", nargs, 1);
    if (!fl_isstring(args[0]))
        type_error("string.reverse", "string", args[0]);
    len = cv_len((struct cvalue *)ptr(args[0]));
    ns = cvalue_string(len);
    u8_reverse(cvalue_data(ns), cvalue_data(args[0]), len);
    return ns;
}

value_t fl_string_encode(value_t *args, uint32_t nargs)
{
    argcount("string.encode", nargs, 1);
    if (iscvalue(args[0])) {
        struct cvalue *cv = (struct cvalue *)ptr(args[0]);
        struct fltype *t = cv_class(cv);
        if (t->eltype == wchartype) {
            size_t nc = cv_len(cv) / sizeof(uint32_t);
            uint32_t *ptr = (uint32_t *)cv_data(cv);
            size_t nbytes = u8_codingsize(ptr, nc);
            value_t str = cvalue_string(nbytes);
            ptr =
            cv_data((struct cvalue *)ptr(args[0]));  // relocatable pointer
            u8_toutf8(cvalue_data(str), nbytes, ptr, nc);
            return str;
        }
    }
    type_error("string.encode", "wchar array", args[0]);
    return FL_NIL;  // TODO: remove
}

value_t fl_string_decode(value_t *args, uint32_t nargs)
{
    int term;
    struct cvalue *cv;
    char *ptr;
    size_t nb, nc, newsz;
    value_t wcstr;
    uint32_t *pwc;

    term = 0;
    if (nargs == 2) {
        term = (args[1] != FL_F);
    } else {
        argcount("string.decode", nargs, 1);
    }
    if (!fl_isstring(args[0]))
        type_error("string.decode", "string", args[0]);
    cv = (struct cvalue *)ptr(args[0]);
    ptr = (char *)cv_data(cv);
    nb = cv_len(cv);
    nc = u8_charnum(ptr, nb);
    newsz = nc * sizeof(uint32_t);
    if (term)
        newsz += sizeof(uint32_t);
    wcstr = cvalue(wcstringtype, newsz);
    ptr = cv_data((struct cvalue *)ptr(args[0]));  // relocatable pointer
    pwc = cvalue_data(wcstr);
    u8_toucs(pwc, nc, ptr, nb);
    if (term)
        pwc[nc] = 0;
    return wcstr;
}

extern value_t fl_buffer(value_t *args, uint32_t nargs);
extern value_t stream_to_string(value_t *ps);

value_t fl_string(value_t *args, uint32_t nargs)
{
    value_t arg, buf;
    struct ios *s;
    uint32_t i;
    value_t outp;

    if (nargs == 1 && fl_isstring(args[0]))
        return args[0];
    buf = fl_buffer(NULL, 0);
    fl_gc_handle(&buf);
    s = value2c(struct ios *, buf);
    FOR_ARGS(i, 0, arg, args) { display_defaults(s, args[i]); }
    outp = stream_to_string(&buf);
    fl_free_gc_handles(1);
    return outp;
}

value_t fl_string_split(value_t *args, uint32_t nargs)
{
    char *s;
    char *delim;
    size_t len, dlen, ssz, tokend, tokstart, i, junk;
    value_t first, c, last;

    argcount("string.split", nargs, 2);
    s = tostring(args[0], "string.split");
    delim = tostring(args[1], "string.split");
    len = cv_len((struct cvalue *)ptr(args[0]));
    dlen = cv_len((struct cvalue *)ptr(args[1]));
    tokend = tokstart = i = 0;
    first = c = FL_NIL;
    fl_gc_handle(&first);
    fl_gc_handle(&last);
    do {
        // find and allocate next token
        tokstart = tokend = i;
        while (i < len &&
               !u8_memchr(delim, u8_nextmemchar(s, &i), dlen, &junk))
            tokend = i;
        ssz = tokend - tokstart;
        last = c;  // save previous cons cell
        c = fl_cons(cvalue_string(ssz), FL_NIL);

        // we've done allocation; reload movable pointers
        s = cv_data((struct cvalue *)ptr(args[0]));
        delim = cv_data((struct cvalue *)ptr(args[1]));

        if (ssz)
            memcpy(cv_data((struct cvalue *)ptr(car_(c))), &s[tokstart], ssz);

        // link new cell
        if (last == FL_NIL)
            first = c;  // first time, save first cons
        else
            ((struct cons *)ptr(last))->cdr = c;

        // note this tricky condition: if the string ends with a
        // delimiter, we need to go around one more time to add an
        // empty string. this happens when (i==len && tokend<i)
    } while (i < len || (i == len && (tokend != i)));
    fl_free_gc_handles(2);
    return first;
}

value_t fl_string_sub(value_t *args, uint32_t nargs)
{
    char *s;
    size_t len, i1, i2;
    value_t ns;

    if (nargs != 2)
        argcount("string.sub", nargs, 3);
    s = tostring(args[0], "string.sub");
    len = cv_len((struct cvalue *)ptr(args[0]));
    i1 = toulong(args[1], "string.sub");
    if (i1 > len)
        bounds_error("string.sub", args[0], args[1]);
    if (nargs == 3) {
        i2 = toulong(args[2], "string.sub");
        if (i2 > len)
            bounds_error("string.sub", args[0], args[2]);
    } else {
        i2 = len;
    }
    if (i2 <= i1)
        return cvalue_string(0);
    ns = cvalue_string(i2 - i1);
    memcpy(cv_data((struct cvalue *)ptr(ns)), &s[i1], i2 - i1);
    return ns;
}

value_t fl_string_char(value_t *args, uint32_t nargs)
{
    char *s;
    size_t len, i, sl;

    argcount("string.char", nargs, 2);
    s = tostring(args[0], "string.char");
    len = cv_len((struct cvalue *)ptr(args[0]));
    i = toulong(args[1], "string.char");
    if (i >= len)
        bounds_error("string.char", args[0], args[1]);
    sl = u8_seqlen(&s[i]);
    if (sl > len || i > len - sl)
        bounds_error("string.char", args[0], args[1]);
    return mk_wchar(u8_nextchar(s, &i));
}

value_t builtin_char_upcase(value_t *args, uint32_t nargs)
{
    struct cprim *cp;

    argcount("char.upcase", nargs, 1);
    cp = (struct cprim *)ptr(args[0]);
    if (!iscprim(args[0]) || cp_class(cp) != wchartype)
        type_error("char.upcase", "wchar", args[0]);
    return mk_wchar(towupper(*(int32_t *)cp_data(cp)));
}

value_t builtin_char_downcase(value_t *args, uint32_t nargs)
{
    struct cprim *cp;

    argcount("char.downcase", nargs, 1);
    cp = (struct cprim *)ptr(args[0]);
    if (!iscprim(args[0]) || cp_class(cp) != wchartype)
        type_error("char.downcase", "wchar", args[0]);
    return mk_wchar(towlower(*(int32_t *)cp_data(cp)));
}

value_t builtin_char_alphabetic(value_t *args, uint32_t nargs)
{
    struct cprim *cp;

    argcount("char-alphabetic?", nargs, 1);
    cp = (struct cprim *)ptr(args[0]);
    if (!iscprim(args[0]) || cp_class(cp) != wchartype)
        type_error("char-alphabetic?", "wchar", args[0]);
    return iswalpha(*(int32_t *)cp_data(cp)) ? FL_T : FL_F;
}

static value_t mem_find_byte(char *s, char c, size_t start, size_t len)
{
    char *p;

    p = memchr(s + start, c, len - start);
    if (p == NULL)
        return FL_F;
    return size_wrap((size_t)(p - s));
}

value_t fl_string_find(value_t *args, uint32_t nargs)
{
    char cbuf[8];
    char *s;
    char *needle;
    struct cprim *cp;
    value_t v;
    size_t start, len, needlesz, i;

    if (nargs == 3)
        start = toulong(args[2], "string.find");
    else {
        argcount("string.find", nargs, 2);
        start = 0;
    }
    s = tostring(args[0], "string.find");
    len = cv_len((struct cvalue *)ptr(args[0]));
    if (start > len)
        bounds_error("string.find", args[0], args[2]);

    v = args[1];
    cp = (struct cprim *)ptr(v);
    if (iscprim(v) && cp_class(cp) == wchartype) {
        uint32_t c = *(uint32_t *)cp_data(cp);
        if (c <= 0x7f)
            return mem_find_byte(s, (char)c, start, len);
        needlesz = u8_toutf8(cbuf, sizeof(cbuf), &c, 1);
        needle = cbuf;
    } else if (iscprim(v) && cp_class(cp) == bytetype) {
        return mem_find_byte(s, *(char *)cp_data(cp), start, len);
    } else if (fl_isstring(v)) {
        struct cvalue *cv = (struct cvalue *)ptr(v);
        needlesz = cv_len(cv);
        needle = (char *)cv_data(cv);
    } else {
        type_error("string.find", "string", args[1]);
    }
    if (needlesz > len - start)
        return FL_F;
    else if (needlesz == 1)
        return mem_find_byte(s, needle[0], start, len);
    else if (needlesz == 0)
        return size_wrap(start);
    for (i = start; i < len - needlesz + 1; i++) {
        if (s[i] == needle[0]) {
            if (!memcmp(&s[i + 1], needle + 1, needlesz - 1))
                return size_wrap(i);
        }
    }
    return FL_F;
}

value_t fl_string_inc(value_t *args, uint32_t nargs)
{
    char *s;
    size_t len, cnt, i;

    if (nargs < 2 || nargs > 3)
        argcount("string.inc", nargs, 2);
    s = tostring(args[0], "string.inc");
    len = cv_len((struct cvalue *)ptr(args[0]));
    i = toulong(args[1], "string.inc");
    cnt = 1;
    if (nargs == 3)
        cnt = toulong(args[2], "string.inc");
    while (cnt--) {
        if (i >= len)
            bounds_error("string.inc", args[0], args[1]);
        (void)(isutf(s[++i]) || isutf(s[++i]) || isutf(s[++i]) || ++i);
    }
    return size_wrap(i);
}

value_t fl_string_dec(value_t *args, uint32_t nargs)
{
    char *s;
    size_t len, cnt, i;

    if (nargs < 2 || nargs > 3)
        argcount("string.dec", nargs, 2);
    s = tostring(args[0], "string.dec");
    len = cv_len((struct cvalue *)ptr(args[0]));
    i = toulong(args[1], "string.dec");
    cnt = 1;
    if (nargs == 3)
        cnt = toulong(args[2], "string.dec");
    // note: i is allowed to start at index len
    if (i > len)
        bounds_error("string.dec", args[0], args[1]);
    while (cnt--) {
        if (i == 0)
            bounds_error("string.dec", args[0], args[1]);
        (void)(isutf(s[--i]) || isutf(s[--i]) || isutf(s[--i]) || --i);
    }
    return size_wrap(i);
}

static unsigned long get_radix_arg(value_t arg, char *fname)
{
    unsigned long radix;

    radix = toulong(arg, fname);
    if (radix < 2 || radix > 36)
        lerrorf(ArgError, "%s: invalid radix", fname);
    return radix;
}

value_t fl_numbertostring(value_t *args, uint32_t nargs)
{
    char buf[128];
    uint64_t num;
    unsigned long radix;
    value_t n;
    char *str;
    int neg;

    if (nargs < 1 || nargs > 2)
        argcount("number->string", nargs, 2);
    n = args[0];
    neg = 0;
    if (isfixnum(n))
        num = numval(n);
    else if (!iscprim(n))
        type_error("number->string", "integer", n);
    else
        num = conv_to_uint64(cp_data((struct cprim *)ptr(n)),
                             cp_numtype((struct cprim *)ptr(n)));
    if (numval(fl_compare(args[0], fixnum(0))) < 0) {
        num = -num;
        neg = 1;
    }
    radix = 10;
    if (nargs == 2)
        radix = get_radix_arg(args[1], "number->string");
    str = uint2str(buf, sizeof(buf), num, radix);
    if (neg && str > &buf[0])
        *(--str) = '-';
    return string_from_cstr(str);
}

value_t fl_stringtonumber(value_t *args, uint32_t nargs)
{
    char *str;
    value_t n;
    unsigned long radix;

    if (nargs < 1 || nargs > 2)
        argcount("string->number", nargs, 2);
    str = tostring(args[0], "string->number");
    radix = 0;
    if (nargs == 2)
        radix = get_radix_arg(args[1], "string->number");
    if (!isnumtok_base(str, &n, (int)radix))
        return FL_F;
    return n;
}

value_t fl_string_isutf8(value_t *args, uint32_t nargs)
{
    char *s;
    size_t len;

    argcount("string.isutf8", nargs, 1);
    s = tostring(args[0], "string.isutf8");
    len = cv_len((struct cvalue *)ptr(args[0]));
    return u8_isvalid(s, len) ? FL_T : FL_F;
}

static struct builtinspec stringfunc_info[] = {
    { "string", fl_string },
    { "string?", fl_stringp },
    { "string.count", fl_string_count },
    { "string.width", fl_string_width },
    { "string.split", fl_string_split },
    { "string.sub", fl_string_sub },
    { "string.find", fl_string_find },
    { "string.char", fl_string_char },
    { "string.inc", fl_string_inc },
    { "string.dec", fl_string_dec },
    { "string.reverse", fl_string_reverse },
    { "string.encode", fl_string_encode },
    { "string.decode", fl_string_decode },
    { "string.isutf8", fl_string_isutf8 },

    { "char-upcase", builtin_char_upcase },
    { "char-downcase", builtin_char_downcase },
    { "char-alphabetic?", builtin_char_alphabetic },

    { "char.upcase", builtin_char_upcase },
    { "char.downcase", builtin_char_downcase },
    { "char.alphabetic?", builtin_char_alphabetic },

    { "number->string", fl_numbertostring },
    { "string->number", fl_stringtonumber },

    { NULL, NULL }
};

void stringfuncs_init(void) { assign_global_builtins(stringfunc_info); }
