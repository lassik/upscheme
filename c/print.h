extern void *memrchr(const void *s, int c, size_t n);
extern value_t instrsym;
extern value_t outstrsym;

struct printer_options {
    int display;      // Use `display` repr instead of `write` repr
    int newline;      // Write a newline at the end.
    int shared;       // 0=no cycle detection, 1=minimal cycles, 2=max cycles
    int indent;       // Write indented lines instead of one long line.
    int width;        // maximum line length when indenting, ignored when not
    fixnum_t length;  // truncate lists after N items and write "..."
    fixnum_t level;   // print only the outermost N levels of nested structure
};

struct printer {
    int line;
    int column;
    unsigned int level;
    unsigned int cycle_labels;
    struct htable cycle_traversed;
    struct printer_options opts;
};

// Printer state during one printer run
static struct printer pr;

static void outc(char c, struct ios *f)
{
    ios_putc(c, f);
    if (c == '\n')
        pr.column = 0;
    else
        pr.column++;
}

static void outs(char *s, struct ios *f)
{
    ios_puts(s, f);
    pr.column += u8_strwidth(s);
}

static void outsn(char *s, struct ios *f, size_t n)
{
    ios_write(f, s, n);
    pr.column += u8_strwidth(s);
}

static int outindent(int n, struct ios *f)
{
    int n0;

    // move back to left margin if we get too indented
    if (n > pr.opts.width - 12)
        n = 2;
    n0 = n;
    ios_putc('\n', f);
    pr.line++;
    pr.column = n;
    while (n) {
        ios_putc(' ', f);
        n--;
    }
    return n0;
}

void fl_print_chr(char c, struct ios *f) { outc(c, f); }

void fl_print_str(char *s, struct ios *f) { outs(s, f); }

void print_traverse(value_t v)
{
    value_t *bp;

    while (iscons(v)) {
        if (ismarked(v)) {
            bp = (value_t *)ptrhash_bp(&pr.cycle_traversed, (void *)v);
            if (*bp == (value_t)HT_NOTFOUND)
                *bp = fixnum(pr.cycle_labels++);
            return;
        }
        mark_cons(v);
        print_traverse(car_(v));
        v = cdr_(v);
    }
    if (!ismanaged(v) || issymbol(v))
        return;
    if (ismarked(v)) {
        bp = (value_t *)ptrhash_bp(&pr.cycle_traversed, (void *)v);
        if (*bp == (value_t)HT_NOTFOUND)
            *bp = fixnum(pr.cycle_labels++);
        return;
    }
    if (isvector(v)) {
        unsigned int i;

        if (vector_size(v) > 0)
            mark_cons(v);
        for (i = 0; i < vector_size(v); i++)
            print_traverse(vector_elt(v, i));
    } else if (iscprim(v)) {
        // don't consider shared references to e.g. chars
    } else if (isclosure(v)) {
        struct function *f;

        mark_cons(v);
        f = (struct function *)ptr(v);
        print_traverse(f->bcode);
        print_traverse(f->vals);
        print_traverse(f->env);
    } else {
        struct cvalue *cv;
        struct fltype *t;

        assert(iscvalue(v));
        cv = (struct cvalue *)ptr(v);
        // don't consider shared references to ""
        if (!cv_isstr(cv) || cv_len(cv) != 0)
            mark_cons(v);
        t = cv_class(cv);
        if (t->vtable != NULL && t->vtable->print_traverse != NULL)
            t->vtable->print_traverse(v);
    }
}

static void print_symbol_name(struct ios *f, char *name)
{
    int i, escape, charescape;

    escape = charescape = 0;
    if ((name[0] == '\0') || (name[0] == '.' && name[1] == '\0') ||
        (name[0] == '#') || isnumtok(name, NULL))
        escape = 1;
    i = 0;
    while (name[i]) {
        if (!symchar(name[i])) {
            escape = 1;
            if (name[i] == '|' || name[i] == '\\') {
                charescape = 1;
                break;
            }
        }
        i++;
    }
    if (escape) {
        if (charescape) {
            outc('|', f);
            i = 0;
            while (name[i]) {
                if (name[i] == '|' || name[i] == '\\')
                    outc('\\', f);
                outc(name[i], f);
                i++;
            }
            outc('|', f);
        } else {
            outc('|', f);
            outs(name, f);
            outc('|', f);
        }
    } else {
        outs(name, f);
    }
}

/*
  The following implements a simple pretty-printing algorithm. This is
  an unlimited-width approach that doesn't require an extra pass.
  It uses some heuristics to guess whether an expression is "small",
  and avoids wrapping symbols across lines. The result is high
  performance and nice output for typical code. Quality is poor for
  pathological or deeply-nested expressions, but those are difficult
  to print anyway.
*/
#define SMALL_STR_LEN 20
static int tinyp(value_t v)
{
    if (issymbol(v))
        return (u8_strwidth(symbol_name(v)) < SMALL_STR_LEN);
    if (fl_isstring(v))
        return (cv_len((struct cvalue *)ptr(v)) < SMALL_STR_LEN);
    return (isfixnum(v) || isbuiltin(v) || v == FL_F || v == FL_T ||
            v == FL_NIL || v == FL_EOF || iscprim(v));
}

static int smallp(value_t v)
{
    if (tinyp(v))
        return 1;
    if (fl_isnumber(v))
        return 1;
    if (iscons(v)) {
        if (tinyp(car_(v)) &&
            (tinyp(cdr_(v)) || (iscons(cdr_(v)) && tinyp(car_(cdr_(v))) &&
                                cdr_(cdr_(v)) == NIL)))
            return 1;
        return 0;
    }
    if (isvector(v)) {
        size_t s = vector_size(v);
        return (s == 0 || (tinyp(vector_elt(v, 0)) &&
                           (s == 1 || (s == 2 && tinyp(vector_elt(v, 1))))));
    }
    return 0;
}

static int specialindent(value_t head)
{
    // indent these forms 2 spaces, not lined up with the first argument
    if (head == LAMBDA || head == TRYCATCH || head == definesym ||
        head == defmacrosym || head == forsym)
        return 2;
    return -1;
}

static int lengthestimate(value_t v)
{
    // get the width of an expression if we can do so cheaply
    if (issymbol(v))
        return u8_strwidth(symbol_name(v));
    if (iscprim(v) && cp_class((struct cprim *)ptr(v)) == wchartype)
        return 4;
    return -1;
}

static int allsmallp(value_t v)
{
    int n;

    n = 1;
    while (iscons(v)) {
        if (!smallp(car_(v)))
            return 0;
        v = cdr_(v);
        n++;
        if (n > 25)
            return n;
    }
    return n;
}

static int indentafter3(value_t head, value_t v)
{
    // for certain X always indent (X a b c) after b
    return ((head == forsym) && !allsmallp(cdr_(v)));
}

static int indentafter2(value_t head, value_t v)
{
    // for certain X always indent (X a b) after a
    return ((head == definesym || head == defmacrosym) &&
            !allsmallp(cdr_(v)));
}

static int indentevery(value_t v)
{
    value_t c;

    // indent before every subform of a special form, unless every
    // subform is "small"
    c = car_(v);
    if (c == LAMBDA || c == setqsym)
        return 0;
    if (c == IF)  // TODO: others
        return !allsmallp(cdr_(v));
    return 0;
}

static int blockindent(value_t v)
{
    // in this case we switch to block indent mode, where the head
    // is no longer considered special:
    // (a b c d e
    //  f g h i j)
    return (allsmallp(v) > 9);
}

static void print_pair(struct ios *f, value_t v)
{
    value_t cd, head;
    char *op;
    fixnum_t last_line;
    int startpos, newindent, blk, n_unindented, n, si, ind, est, always,
    nextsmall, thistiny, after2, after3;

    op = NULL;
    if (iscons(cdr_(v)) && cdr_(cdr_(v)) == NIL &&
        !ptrhash_has(&pr.cycle_traversed, (void *)cdr_(v)) &&
        (((car_(v) == QUOTE) && (op = "'")) ||
         ((car_(v) == BACKQUOTE) && (op = "`")) ||
         ((car_(v) == COMMA) && (op = ",")) ||
         ((car_(v) == COMMAAT) && (op = ",@")) ||
         ((car_(v) == COMMADOT) && (op = ",.")))) {
        // special prefix syntax
        unmark_cons(v);
        unmark_cons(cdr_(v));
        outs(op, f);
        fl_print_child(f, car_(cdr_(v)));
        return;
    }
    startpos = pr.column;
    outc('(', f);
    newindent = pr.column;
    blk = blockindent(v);
    n = ind = always = 0;
    if (!blk)
        always = indentevery(v);
    head = car_(v);
    after3 = indentafter3(head, v);
    after2 = indentafter2(head, v);
    n_unindented = 1;
    while (1) {
        cd = cdr_(v);
        if (pr.opts.length >= 0 && n >= pr.opts.length && cd != NIL) {
            outsn("...)", f, 4);
            break;
        }
        last_line = pr.line;
        unmark_cons(v);
        fl_print_child(f, car_(v));
        if (!iscons(cd) || ptrhash_has(&pr.cycle_traversed, (void *)cd)) {
            if (cd != NIL) {
                outsn(" . ", f, 3);
                fl_print_child(f, cd);
            }
            outc(')', f);
            break;
        }

        if (!pr.opts.indent || ((head == LAMBDA) && n == 0)) {
            // never break line before lambda-list
            ind = 0;
        } else {
            est = lengthestimate(car_(cd));
            nextsmall = smallp(car_(cd));
            thistiny = tinyp(car_(v));
            ind =
            (((pr.line > last_line) || (pr.column > pr.opts.width / 2 &&
                                        !nextsmall && !thistiny && n > 0)) ||

             (pr.column > pr.opts.width - 4) ||

             (est != -1 && (pr.column + est > pr.opts.width - 2)) ||

             ((head == LAMBDA) && !nextsmall) ||

             (n > 0 && always) ||

             (n == 2 && after3) || (n == 1 && after2) ||

             (n_unindented >= 3 && !nextsmall) ||

             (n == 0 && !smallp(head)));
        }

        if (ind) {
            newindent = outindent(newindent, f);
            n_unindented = 1;
        } else {
            n_unindented++;
            outc(' ', f);
            if (n == 0) {
                // set indent level after printing head
                si = specialindent(head);
                if (si != -1)
                    newindent = startpos + si;
                else if (!blk)
                    newindent = pr.column;
            }
        }
        n++;
        v = cd;
    }
}

static void cvalue_print(struct ios *f, value_t v);

static int write_cycle_prefix(struct ios *f, value_t v)
{
    value_t label;

    if ((label = (value_t)ptrhash_get(&pr.cycle_traversed, (void *)v)) !=
        (value_t)HT_NOTFOUND) {
        if (!ismarked(v)) {
            pr.column += ios_printf(f, "#%ld#", numval(label));
            return 1;
        }
        pr.column += ios_printf(f, "#%ld=", numval(label));
    }
    if (ismanaged(v))
        unmark_cons(v);
    return 0;
}

void fl_print_child(struct ios *f, value_t v)
{
    char *name;

    // fprintf(stderr, "fl_print_child\n");
    if (pr.opts.level >= 0 && pr.level >= pr.opts.level &&
        (iscons(v) || isvector(v) || isclosure(v))) {
        outc('#', f);
        return;
    }
    pr.level++;
    switch (tag(v)) {
    case TAG_NUM:
    case TAG_NUM1:
        pr.column += ios_printf(f, "%ld", numval(v));
        break;
    case TAG_SYM:
        name = symbol_name(v);
        if (pr.opts.display)
            outs(name, f);
        else if (ismanaged(v)) {
            outsn("#:", f, 2);
            outs(name, f);
        } else
            print_symbol_name(f, name);
        break;
    case TAG_FUNCTION:
        if (v == FL_T) {
            outsn("#t", f, 2);
        } else if (v == FL_F) {
            outsn("#f", f, 2);
        } else if (v == FL_NIL) {
            outsn("()", f, 2);
        } else if (v == FL_EOF) {
            outsn("#<eof>", f, 6);
        } else if (isbuiltin(v)) {
            if (!pr.opts.display)
                outsn("#.", f, 2);
            outs(builtin_names[uintval(v)], f);
        } else {
            assert(isclosure(v));
            if (!pr.opts.display) {
                struct function *fn;
                char *data;
                size_t i, sz;

                if (write_cycle_prefix(f, v))
                    break;
                fn = (struct function *)ptr(v);
                outs("#fn(", f);
                data = cvalue_data(fn->bcode);
                sz = cvalue_len(fn->bcode);
                for (i = 0; i < sz; i++)
                    data[i] += 48;
                fl_print_child(f, fn->bcode);
                for (i = 0; i < sz; i++)
                    data[i] -= 48;
                outc(' ', f);
                fl_print_child(f, fn->vals);
                if (fn->env != NIL) {
                    outc(' ', f);
                    fl_print_child(f, fn->env);
                }
                if (fn->name != LAMBDA) {
                    outc(' ', f);
                    fl_print_child(f, fn->name);
                }
                outc(')', f);
            } else {
                outs("#<function>", f);
            }
        }
        break;
    case TAG_CPRIM:
        if (v == UNBOUND)
            outs("#<undefined>", f);
        else
            cvalue_print(f, v);
        break;
    case TAG_CVALUE:
    case TAG_VECTOR:
    case TAG_CONS:
        if (!pr.opts.display && write_cycle_prefix(f, v))
            break;
        if (isvector(v)) {
            int newindent, est, sz, i;

            outc('[', f);
            newindent = pr.column;
            sz = vector_size(v);
            for (i = 0; i < sz; i++) {
                if (pr.opts.length >= 0 && i >= pr.opts.length &&
                    i < sz - 1) {
                    outsn("...", f, 3);
                    break;
                }
                fl_print_child(f, vector_elt(v, i));
                if (i < sz - 1) {
                    if (!pr.opts.indent) {
                        outc(' ', f);
                    } else {
                        est = lengthestimate(vector_elt(v, i + 1));
                        if (pr.column > pr.opts.width - 4 ||
                            (est != -1 &&
                             (pr.column + est > pr.opts.width - 2)) ||
                            (pr.column > pr.opts.width / 2 &&
                             !smallp(vector_elt(v, i + 1)) &&
                             !tinyp(vector_elt(v, i))))
                            newindent = outindent(newindent, f);
                        else
                            outc(' ', f);
                    }
                }
            }
            outc(']', f);
            break;
        }
        if (iscvalue(v))
            cvalue_print(f, v);
        else
            print_pair(f, v);
        break;
    }
    pr.level--;
}

static void print_string(struct ios *f, char *str, size_t sz)
{
    char buf[512];
    size_t i = 0;
    uint8_t c;
    static char hexdig[] = "0123456789abcdef";

    outc('"', f);
    if (!u8_isvalid(str, sz)) {
        // alternate print algorithm that preserves data if it's not UTF-8
        for (i = 0; i < sz; i++) {
            c = str[i];
            if (c == '\\')
                outsn("\\\\", f, 2);
            else if (c == '"')
                outsn("\\\"", f, 2);
            else if (c >= 32 && c < 0x7f)
                outc(c, f);
            else {
                outsn("\\x", f, 2);
                outc(hexdig[c >> 4], f);
                outc(hexdig[c & 0xf], f);
            }
        }
    } else {
        while (i < sz) {
            size_t n = u8_escape(buf, sizeof(buf), str, &i, sz, 1, 0);
            outsn(buf, f, n - 1);
        }
    }
    outc('"', f);
}

int double_exponent(double d)
{
    union ieee754_double dl;

    dl.d = d;
    return dl.ieee.exponent - IEEE754_DOUBLE_BIAS;
}

void snprint_real(char *s, size_t cnt, double r,
                  int width,  // printf field width, or 0
                  int dec,    // # decimal digits desired, recommend 16
                  // # of zeros in .00...0x before using scientific notation
                  // recommend 3-4 or so
                  int max_digs_rt,
                  // # of digits left of decimal before scientific notation
                  // recommend 10
                  int max_digs_lf)
{
    int mag;
    double fpart, temp;
    char format[8];
    char num_format[3];
    int sz, keepz = 0;

    s[0] = '\0';
    if (width == -1) {
        width = 0;
        keepz = 1;
    }
    if (isnan(r)) {
        if (sign_bit(r))
            strncpy(s, "-nan", cnt);
        else
            strncpy(s, "nan", cnt);
        return;
    }
    if (r == 0) {
        strncpy(s, "0", cnt);
        return;
    }

    num_format[0] = 'l';
    num_format[2] = '\0';

    mag = double_exponent(r);

    mag = (int)(((double)mag) / LOG2_10 + 0.5);
    if (r == 0)
        mag = 0;
    if ((mag > max_digs_lf - 1) || (mag < -max_digs_rt)) {
        num_format[1] = 'e';
        temp = r / pow(10, mag);    /* see if number will have a decimal */
        fpart = temp - floor(temp); /* when written in scientific notation */
    } else {
        num_format[1] = 'f';
        fpart = r - floor(r);
    }
    if (fpart == 0)
        dec = 0;
    if (width == 0) {
        snprintf(format, 8, "%%.%d%s", dec, num_format);
    } else {
        snprintf(format, 8, "%%%d.%d%s", width, dec, num_format);
    }
    sz = snprintf(s, cnt, format, r);
    /* trim trailing zeros from fractions. not when using scientific
       notation, since we might have e.g. 1.2000e+100. also not when we
       need a specific output width */
    if (width == 0 && !keepz) {
        if (sz > 2 && fpart && num_format[1] != 'e') {
            while (s[sz - 1] == '0') {
                s[sz - 1] = '\0';
                sz--;
            }
            // don't need trailing .
            if (s[sz - 1] == '.') {
                s[sz - 1] = '\0';
                sz--;
            }
        }
    }
    // TODO. currently 1.1e20 prints as 1.1000000000000000e+20; be able to
    // get rid of all those zeros.
}

static numerictype_t sym_to_numtype(value_t type);

// 'weak' means we don't need to accurately reproduce the type, so
// for example #int32(0) can be printed as just 0. this is used
// printing in a context where a type is already implied, e.g. inside
// an array.
static void cvalue_printdata(struct ios *f, void *data, size_t len,
                             value_t type, int weak)
{
    if (type == bytesym) {
        unsigned char ch = *(unsigned char *)data;
        if (pr.opts.display)
            outc(ch, f);
        else if (weak)
            pr.column += ios_printf(f, "#x%hhx", ch);
        else
            pr.column += ios_printf(f, "#byte(#x%hhx)", ch);
    } else if (type == wcharsym) {
        char seq[8];
        uint32_t wc = *(uint32_t *)data;
        size_t nb = u8_toutf8(seq, sizeof(seq), &wc, 1);

        seq[nb] = '\0';
        if (pr.opts.display) {
            // TODO: better multibyte handling
            if (wc == 0)
                ios_putc(0, f);
            else
                outs(seq, f);
        } else {
            outsn("#\\", f, 2);
            if (wc == 0x00)
                outsn("nul", f, 3);
            else if (wc == 0x07)
                outsn("alarm", f, 5);
            else if (wc == 0x08)
                outsn("backspace", f, 9);
            else if (wc == 0x09)
                outsn("tab", f, 3);
            // else if (wc == 0x0A) outsn("linefeed", f, 8);
            else if (wc == 0x0A)
                outsn("newline", f, 7);
            else if (wc == 0x0B)
                outsn("vtab", f, 4);
            else if (wc == 0x0C)
                outsn("page", f, 4);
            else if (wc == 0x0D)
                outsn("return", f, 6);
            else if (wc == 0x1B)
                outsn("esc", f, 3);
            // else if (wc == 0x20) outsn("space", f, 5);
            else if (wc == 0x7F)
                outsn("delete", f, 6);
            else if (iswprint(wc))
                outs(seq, f);
            else
                pr.column += ios_printf(f, "x%04x", (int)wc);
        }
    } else if (type == floatsym || type == doublesym) {
        char buf[64];
        double d;
        int ndec;

        if (type == floatsym) {
            d = (double)*(float *)data;
            ndec = 8;
        } else {
            d = *(double *)data;
            ndec = 16;
        }
        if (!DFINITE(d)) {
            char *rep;

            if (isnan(d))
                rep = sign_bit(d) ? "-nan.0" : "+nan.0";
            else
                rep = sign_bit(d) ? "-inf.0" : "+inf.0";
            if (type == floatsym && !pr.opts.display && !weak)
                pr.column += ios_printf(f, "#%s(%s)", symbol_name(type), rep);
            else
                outs(rep, f);
        } else if (d == 0) {
            if (1 / d < 0)
                outsn("-0.0", f, 4);
            else
                outsn("0.0", f, 3);
            if (type == floatsym && !pr.opts.display && !weak)
                outc('f', f);
        } else {
            int hasdec;

            snprint_real(buf, sizeof(buf), d, 0, ndec, 3, 10);
            hasdec = (strpbrk(buf, ".eE") != NULL);
            outs(buf, f);
            if (!hasdec)
                outsn(".0", f, 2);
            if (type == floatsym && !pr.opts.display && !weak)
                outc('f', f);
        }
    } else if (type == uint64sym
#ifdef BITS64
               || type == ulongsym
#endif
    ) {
        uint64_t ui64 = *(uint64_t *)data;
        if (weak || pr.opts.display)
            pr.column += ios_printf(f, "%llu", ui64);
        else
            pr.column += ios_printf(f, "#%s(%llu)", symbol_name(type), ui64);
    } else if (issymbol(type)) {
        // handle other integer prims. we know it's smaller than uint64
        // at this point, so int64 is big enough to capture everything.
        numerictype_t nt = sym_to_numtype(type);
        if (nt == N_NUMTYPES) {
            pr.column += ios_printf(f, "#<%s>", symbol_name(type));
        } else {
            int64_t i64 = conv_to_int64(data, nt);
            if (weak || pr.opts.display)
                pr.column += ios_printf(f, "%lld", i64);
            else
                pr.column +=
                ios_printf(f, "#%s(%lld)", symbol_name(type), i64);
        }
    } else if (iscons(type)) {
        if (car_(type) == arraysym) {
            value_t eltype = car(cdr_(type));
            size_t cnt, elsize, i;

            if (iscons(cdr_(cdr_(type)))) {
                cnt = toulong(car_(cdr_(cdr_(type))), "length");
                elsize = cnt ? len / cnt : 0;
            } else {
                // incomplete array type
                int junk;
                elsize = ctype_sizeof(eltype, &junk);
                cnt = elsize ? len / elsize : 0;
            }
            if (eltype == bytesym) {
                if (pr.opts.display) {
                    ios_write(f, data, len);
                    /*
                    char *nl = memrchr(data, '\n', len);
                    if (nl)
                        pr.column = u8_strwidth(nl+1);
                    else
                        pr.column += u8_strwidth(data);
                    */
                } else {
                    print_string(f, (char *)data, len);
                }
                return;
            } else if (eltype == wcharsym) {
                // TODO wchar
            } else {
            }
            if (!weak) {
                if (eltype == uint8sym) {
                    outsn("#vu8(", f, 5);
                } else {
                    outsn("#array(", f, 7);
                    fl_print_child(f, eltype);
                    if (cnt > 0)
                        outc(' ', f);
                }
            } else {
                outc('[', f);
            }
            for (i = 0; i < cnt; i++) {
                if (i > 0)
                    outc(' ', f);
                cvalue_printdata(f, data, elsize, eltype, 1);
                data = (char *)data + elsize;
            }
            if (!weak)
                outc(')', f);
            else
                outc(']', f);
        } else if (car_(type) == enumsym) {
            int n = *(int *)data;
            value_t syms = car(cdr_(type));
            assert(isvector(syms));
            if (!weak) {
                outsn("#enum(", f, 6);
                fl_print_child(f, syms);
                outc(' ', f);
            }
            if (n >= (int)vector_size(syms)) {
                cvalue_printdata(f, data, len, int32sym, 1);
            } else {
                fl_print_child(f, vector_elt(syms, n));
            }
            if (!weak)
                outc(')', f);
        }
    }
}

static void cvalue_print(struct ios *f, value_t v)
{
    struct cvalue *cv = (struct cvalue *)ptr(v);
    void *data = cptr(v);
    value_t label;

    if (cv_class(cv) == builtintype) {
        void *fptr = *(void **)data;
        label = (value_t)ptrhash_get(&reverse_dlsym_lookup_table, cv);
        if (label == (value_t)HT_NOTFOUND) {
            pr.column +=
            ios_printf(f, "#<builtin @#x%08zx>", (size_t)(builtin_t)fptr);
        } else {
            if (pr.opts.display) {
                outs(symbol_name(label), f);
            } else {
                outsn("#fn(", f, 4);
                outs(symbol_name(label), f);
                outc(')', f);
            }
        }
    } else if (cv_class(cv)->vtable != NULL &&
               cv_class(cv)->vtable->print != NULL) {
        cv_class(cv)->vtable->print(v, f);
    } else {
        value_t type = cv_type(cv);
        size_t len = iscprim(v) ? cv_class(cv)->size : cv_len(cv);
        cvalue_printdata(f, data, len, type, 0);
    }
}

void print_with_options(struct ios *f, value_t v,
                        struct printer_options *opts)
{
    memcpy(&pr.opts, opts, sizeof(pr.opts));

    // TODO
    if (pr.opts.width < 80)
        pr.opts.width = 80;

    // TODO
    pr.opts.level = -1;
    pr.opts.length = -1;

    pr.level = 0;
    pr.cycle_labels = 0;
    if (pr.opts.shared)
        print_traverse(v);
    pr.line = pr.column = 0;

    fl_print_child(f, v);

    if (pr.opts.newline) {
        ios_putc('\n', f);
        pr.line++;
        pr.column = 0;
    }

    if (pr.opts.level >= 0 || pr.opts.length >= 0) {
        memset(consflags, 0,
               4 * bitvector_nwords(heapsize / sizeof(struct cons)));
    }

    if ((iscons(v) || isvector(v) || isfunction(v) || iscvalue(v)) &&
        !fl_isstring(v) && v != FL_T && v != FL_F && v != FL_NIL) {
        htable_reset(&pr.cycle_traversed, 32);
    }
}

void display_defaults(struct ios *f, value_t v)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.display = 1;
    opts.shared = 1;
    opts.length = -1;
    opts.level = -1;
    print_with_options(f, v, &opts);
}

void write_defaults_indent(struct ios *f, value_t v)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.shared = 1;
    opts.indent = 1;
    opts.length = -1;
    opts.level = -1;
    print_with_options(f, v, &opts);
}

void fl_print(struct ios *f, value_t v)
{
    struct printer_options opts;
    value_t pl;

    memset(&opts, 0, sizeof(opts));

    // *print-readably*
    opts.display = (symbol_value(printreadablysym) == FL_F);

    // *print-pretty*
    opts.indent = (symbol_value(printprettysym) != FL_F);

    // *print-width*
    pl = symbol_value(printwidthsym);
    if (isfixnum(pl))
        opts.width = numval(pl);
    else
        opts.width = -1;

    // *print-length*
    pl = symbol_value(printlengthsym);
    if (isfixnum(pl))
        opts.length = numval(pl);
    else
        opts.length = -1;

    // *print-level*
    pl = symbol_value(printlevelsym);
    if (isfixnum(pl))
        opts.level = numval(pl);
    else
        opts.level = -1;

    print_with_options(f, v, &opts);
}

static value_t writelike(struct printer_options *opts, const char *proc_name,
                         value_t *args, uint32_t nargs)
{
    value_t val;
    struct ios *ios;

    if (nargs < 1 || nargs > 2)
        argcount(proc_name, nargs, 1);
    val = args[0];
    if (nargs == 2)
        ios = fl_toiostream(args[1], proc_name);
    else
        ios = fl_toiostream(symbol_value(outstrsym), proc_name);
    print_with_options(ios, val, opts);
    return val;
}

static value_t builtin_display(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.display = 1;
    opts.shared = 1;
    return writelike(&opts, "display", args, nargs);
}

static value_t builtin_displayln(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.display = 1;
    opts.shared = 1;
    opts.newline = 1;
    return writelike(&opts, "displayln", args, nargs);
}

static value_t builtin_write(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.shared = 1;
    opts.display = (symbol_value(printreadablysym) == FL_F);
    return writelike(&opts, "write", args, nargs);
}

static value_t builtin_writeln(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.shared = 1;
    opts.newline = 1;
    return writelike(&opts, "writeln", args, nargs);
}

static value_t builtin_write_shared(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    opts.shared = 2;
    return writelike(&opts, "write-shared", args, nargs);
}

static value_t builtin_write_simple(value_t *args, uint32_t nargs)
{
    struct printer_options opts;

    memset(&opts, 0, sizeof(opts));
    return writelike(&opts, "write-simple", args, nargs);
}

static value_t builtin_newline(value_t *args, uint32_t nargs)
{
    struct ios *ios;

    if (nargs > 1)
        argcount("newline", nargs, 1);
    if (nargs == 1)
        ios = fl_toiostream(args[0], "newline");
    else
        ios = fl_toiostream(symbol_value(outstrsym), "newline");
    ios_putc('\n', ios);
    return FL_T;
}

static struct builtinspec printfunc_info[] = {
    { "display", builtin_display },
    { "displayln", builtin_displayln },
    { "write", builtin_write },
    { "writeln", builtin_writeln },
    { "write-shared", builtin_write_shared },
    { "write-simple", builtin_write_simple },
    { "newline", builtin_newline },

    { "xdisplay", builtin_display },
    { "xdisplayln", builtin_displayln },
    { "xwrite", builtin_write },
    { "xwriteln", builtin_writeln },
    { "xwrite-shared", builtin_write_shared },
    { "xwrite-simple", builtin_write_simple },
    { "xnewline", builtin_newline },
    { NULL, NULL }
};

void print_init(void)
{
    htable_new(&pr.cycle_traversed, 32);
    assign_global_builtins(printfunc_info);
}
