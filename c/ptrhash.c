/*
  pointer hash table
  optimized for storing info about particular values
*/

#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme.h"

#include "equalhash.h"

#include "htable_inc.h"

#define OP_EQ(x, y) ((x) == (y))

#ifdef BITS64
static uint64_t _pinthash(uint64_t a)
{
    a = (~a) + (a << 21);  // a = (a << 21) - a - 1;
    a = a ^ (a >> 24);
    a = (a + (a << 3)) + (a << 8);  // a * 265
    a = a ^ (a >> 14);
    a = (a + (a << 2)) + (a << 4);  // a * 21
    a = a ^ (a >> 28);
    a = a + (a << 31);
    return a;
}
#else
static uint32_t _pinthash(uint32_t a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}
#endif

HTIMPL(ptrhash, _pinthash, OP_EQ)
