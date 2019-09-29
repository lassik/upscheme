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

#include "scheme.h"

value_t builtin_ascii_codepoint_p(value_t *args, uint32_t nargs)
{
    int32_t cc;

    argcount("ascii-codepoint?", nargs, 1);
    if (isfixnum(args[0])) {
        cc = numval(args[0]);
        if ((cc >= 0) && (cc < 128)) {
            return FL_T;
        }
    }
    return FL_F;
}

value_t builtin_ascii_char_p(value_t *args, uint32_t nargs)
{
    struct cprim *cp;
    int32_t cc;

    argcount("ascii-char?", nargs, 1);
    if (iscprim(args[0])) {
        cp = (struct cprim *)ptr(args[0]);
        if (cp_class(cp) == wchartype) {
            cc = *(int32_t *)cp_data(cp);  // TODO: Is this right?
            if (cc < 128) {
                return FL_T;
            }
        }
    }
    return FL_F;
}

static int32_t must_get_char_as_int(const char *procname, value_t *args,
                                    uint32_t nargs)
{
    struct cprim *cp;
    int32_t cc;

    argcount(procname, nargs, 1);
    if (isfixnum(args[0])) {
        return numval(args[0]);  // TODO: range?
    }
    if (iscprim(args[0])) {
        cp = (struct cprim *)ptr(args[0]);
        if (cp_class(cp) == wchartype) {
            cc = *(int32_t *)cp_data(cp);
            return *(int32_t *)cp_data(cp);  // TODO: Is this right?
        }
    }
    type_error(procname, "wchar or fixnum", args[0]);
}

static int32_t map_char_int(const char *procname, int32_t (*mapfun)(int32_t),
                            value_t *args, uint32_t nargs)
{
    struct cprim *cp;
    int32_t cc;

    argcount(procname, nargs, 1);
    if (isfixnum(args[0])) {
        return fixnum(mapfun(numval(args[0])));
    }
    if (iscprim(args[0])) {
        cp = (struct cprim *)ptr(args[0]);
        if (cp_class(cp) == wchartype) {
            cc = *(int32_t *)cp_data(cp);
            return mk_wchar(mapfun(cc));  // TODO: Is this right?
        }
    }
    type_error(procname, "wchar or byte", args[0]);
}

static int32_t cc_base_offset_limit(int32_t cc, int32_t base, int32_t offset,
                                    int32_t limit, int32_t maxlimit)

{
    if (limit > maxlimit) {
        limit = maxlimit;
    }
    if ((cc >= base) && (cc < base + limit)) {
        return fixnum(offset + (base - cc));
    }
    return FL_F;
}

//

static int32_t ascii_upcase_int(int32_t cc)
{
    if ((cc >= 0x61) && (cc <= 0x7a)) {
        return cc - 0x20;
    }
    return cc;
}

static int32_t ascii_downcase_int(int32_t cc)
{
    if ((cc >= 0x41) && (cc <= 0x5a)) {
        return cc + 0x20;
    }
    return cc;
}

static int32_t ascii_open_bracket_int(int32_t cc)
{
    if ((cc == '(') || (cc == '[') || (cc == '{') || (cc == '<')) {
        return cc;
    }
    return -1;
}

static int32_t ascii_close_bracket_int(int32_t cc)
{
    if ((cc == ')') || (cc == ']') || (cc == '}') || (cc == '>')) {
        return cc;
    }
    return -1;
}

static int32_t ascii_mirror_bracket_int(int32_t cc)
{
    switch (cc) {
    case '(':
        return ')';
    case ')':
        return '(';
    case '[':
        return ']';
    case ']':
        return '[';
    case '{':
        return '}';
    case '}':
        return '{';
    case '<':
        return '>';
    case '>':
        return '<';
    }
    return -1;
}

static int32_t ascii_control_to_display_int(int32_t cc)
{
    if ((cc >= 0x00) && (cc <= 0x1f)) {
        return cc + 0x40;
    }
    if (cc == 0x7f) {
        return 0x3f;
    }
    return -1;
}

static int32_t ascii_display_to_control_int(int32_t cc)
{
    if ((cc >= 0x40) && (cc <= 0x5f)) {
        return cc - 0x40;
    }
    if (cc == 0x3f) {
        return 0x7f;
    }
    return -1;
}

//

value_t builtin_ascii_control_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-control?", args, nargs);
    return (((cc >= 0x0) && (cc <= 0x1f)) || (cc == 0x7f)) ? FL_T : FL_F;
}

value_t builtin_ascii_display_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-display?", args, nargs);
    return ((cc >= 0x20) && (cc <= 0x7e)) ? FL_T : FL_F;
}

value_t builtin_ascii_whitespace_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-whitespace?", args, nargs);
    if (cc < 0x09) {
        return FL_F;
    }
    if (cc < 0x0e) {
        return FL_T;
    }
    return (cc == 0x20) ? FL_T : FL_F;
}

value_t builtin_ascii_space_or_tab_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-space-or-tab?", args, nargs);
    return ((cc == 0x09) || (cc == 0x20)) ? FL_T : FL_F;
}

value_t builtin_ascii_punctuation_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-punctuation?", args, nargs);
    if ((cc >= 0x21) && (cc <= 0x2f)) {
        return FL_T;
    }
    if ((cc >= 0x3a) && (cc <= 0x40)) {
        return FL_T;
    }
    if ((cc >= 0x5b) && (cc <= 0x60)) {
        return FL_T;
    }
    if ((cc >= 0x7b) && (cc <= 0x7e)) {
        return FL_T;
    }
    return FL_F;
}

value_t builtin_ascii_upper_case_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-upper-case?", args, nargs);
    return ((cc >= 0x41) && (cc <= 0x5a)) ? FL_T : FL_F;
}

value_t builtin_ascii_lower_case_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-lower-case?", args, nargs);
    return ((cc >= 0x61) && (cc <= 0x7a)) ? FL_T : FL_F;
}

value_t builtin_ascii_alphanumeric_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-alphanumeric?", args, nargs);
    if ((cc >= 0x30) && (cc <= 0x39)) {
        return FL_T;
    }
    if ((cc >= 0x41) && (cc <= 0x5a)) {
        return FL_T;
    }
    if ((cc >= 0x61) && (cc <= 0x7a)) {
        return FL_T;
    }
    return FL_F;
}

value_t builtin_ascii_numeric_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-numeric?", args, nargs);
    return ((cc >= 0x30) && (cc <= 0x39)) ? FL_T : FL_F;
}

value_t builtin_ascii_alphabetic_p(value_t *args, uint32_t nargs)
{
    uint32_t cc;

    cc = must_get_char_as_int("ascii-alphabetic?", args, nargs);
    return ((cc >= 0x41) && (cc <= 0x5a)) || ((cc >= 0x61) && (cc <= 0x7a));
}

value_t builtin_ascii_upcase(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-upcase", ascii_upcase_int, args, nargs);
}

value_t builtin_ascii_downcase(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-downcase", ascii_downcase_int, args, nargs);
}

value_t builtin_ascii_open_bracket(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-open-bracket", ascii_open_bracket_int, args,
                        nargs);
}

value_t builtin_ascii_close_bracket(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-close-bracket", ascii_close_bracket_int, args,
                        nargs);
}

value_t builtin_ascii_mirror_bracket(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-mirror-bracket", ascii_mirror_bracket_int,
                        args, nargs);
}

value_t builtin_ascii_control_to_display(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-control->display",
                        ascii_control_to_display_int, args, nargs);
}

value_t builtin_ascii_display_to_control(value_t *args, uint32_t nargs)
{
    return map_char_int("ascii-display->control",
                        ascii_display_to_control_int, args, nargs);
}

value_t builtin_ascii_nth_digit(value_t *args, uint32_t nargs)
{
    int32_t i;

    i = must_get_char_as_int("ascii-nth-digit", args, nargs);
    return ((i >= 0) && (i < 10)) ? mk_wchar(0x30 + i) : FL_F;
}

value_t builtin_ascii_nth_upper_case(value_t *args, uint32_t nargs)
{
    int32_t i;

    i = must_get_char_as_int("ascii-nth-upper-case", args, nargs);
    return ((i >= 0) && (i < 26)) ? mk_wchar(0x41 + i) : FL_F;
}

value_t builtin_ascii_nth_lower_case(value_t *args, uint32_t nargs)
{
    int32_t i;

    i = must_get_char_as_int("ascii-nth-lower-case", args, nargs);
    return ((i >= 0) && (i < 26)) ? mk_wchar(0x61 + i) : FL_F;
}

value_t builtin_ascii_digit_value(value_t *args, uint32_t nargs)
{
    int32_t cc, limit;

    cc = must_get_char_as_int("ascii-digit-value", args, nargs);
    limit = 10;  // TODO: really an arg
    return cc_base_offset_limit(cc, 0x30, 0, limit, 10);
}

value_t builtin_ascii_upper_case_value(value_t *args, uint32_t nargs)
{
    int32_t cc, offset, limit;

    cc = must_get_char_as_int("ascii-upper-case-value", args, nargs);
    offset = 0;  // TODO: really an arg
    limit = 26;  // TODO: really an arg
    return cc_base_offset_limit(cc, 0x41, offset, limit, 26);
}

value_t builtin_ascii_lower_case_value(value_t *args, uint32_t nargs)
{
    int32_t cc, offset, limit;

    cc = must_get_char_as_int("ascii-lower-case-value", args, nargs);
    offset = 0;  // TODO: really an arg
    limit = 26;  // TODO: really an arg
    return cc_base_offset_limit(cc, 0x61, offset, limit, 26);
}
