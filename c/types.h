struct fltype *get_type(value_t t)
{
    struct fltype *ft;
    void **bp;
    size_t sz;
    int align, isarray;

    if (issymbol(t)) {
        ft = ((struct symbol *)ptr(t))->type;
        if (ft != NULL)
            return ft;
    }
    bp = equalhash_bp(&TypeTable, (void *)t);
    if (*bp != HT_NOTFOUND)
        return *bp;
    isarray = (iscons(t) && car_(t) == arraysym && iscons(cdr_(t)));
    if (isarray && !iscons(cdr_(cdr_(t)))) {
        // special case: incomplete array type
        sz = 0;
    } else {
        sz = ctype_sizeof(t, &align);
    }
    ft = (struct fltype *)malloc(sizeof(struct fltype));
    ft->type = t;
    if (issymbol(t)) {
        ft->numtype = sym_to_numtype(t);
        ((struct symbol *)ptr(t))->type = ft;
    } else {
        ft->numtype = N_NUMTYPES;
    }
    ft->size = sz;
    ft->vtable = NULL;
    ft->artype = NULL;
    ft->marked = 1;
    ft->elsz = 0;
    ft->eltype = NULL;
    ft->init = NULL;
    if (iscons(t)) {
        if (isarray) {
            struct fltype *eltype = get_type(car_(cdr_(t)));
            if (eltype->size == 0) {
                free(ft);
                lerror(ArgError, "invalid array element type");
            }
            ft->elsz = eltype->size;
            ft->eltype = eltype;
            ft->init = &cvalue_array_init;
            // eltype->artype = ft; -- this is a bad idea since some types
            // carry array sizes
        } else if (car_(t) == enumsym) {
            ft->numtype = T_INT32;
            ft->init = &cvalue_enum_init;
        }
    }
    *bp = ft;
    return ft;
}

struct fltype *get_array_type(value_t eltype)
{
    struct fltype *et;

    et = get_type(eltype);
    if (et->artype == NULL)
        et->artype = get_type(fl_list2(arraysym, eltype));
    return et->artype;
}

struct fltype *define_opaque_type(value_t sym, size_t sz,
                                  struct cvtable *vtab, cvinitfunc_t init)
{
    struct fltype *ft;

    ft = (struct fltype *)malloc(sizeof(struct fltype));
    ft->type = sym;
    ft->size = sz;
    ft->numtype = N_NUMTYPES;
    ft->vtable = vtab;
    ft->artype = NULL;
    ft->eltype = NULL;
    ft->elsz = 0;
    ft->marked = 1;
    ft->init = init;
    return ft;
}

void relocate_typetable(void)
{
    struct htable *h;
    size_t i;
    void *nv;

    h = &TypeTable;
    for (i = 0; i < h->size; i += 2) {
        if (h->table[i] != HT_NOTFOUND) {
            nv = (void *)relocate((value_t)h->table[i]);
            h->table[i] = nv;
            if (h->table[i + 1] != HT_NOTFOUND)
                ((struct fltype *)h->table[i + 1])->type = (value_t)nv;
        }
    }
}
