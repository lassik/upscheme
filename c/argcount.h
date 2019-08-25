static void argcount(const char *fname, uint32_t nargs, uint32_t c)
{
    if (__unlikely(nargs != c))
        lerrorf(ArgError, "%s: too %s arguments", fname,
                nargs < c ? "few" : "many");
}
