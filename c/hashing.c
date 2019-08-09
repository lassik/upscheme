/*
  Hashing
*/

#ifndef WIN32
#include <sys/param.h> /* attempt to define endianness */
#else
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
#endif

#ifdef LINUX
#include <endian.h> /* attempt to define endianness */
#endif

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dtypes.h"
#include "utils.h"
#include "hashing.h"
#include "timefuncs.h"
#include "ios.h"
#include "random.h"

uint_t nextipow2(uint_t i)
{
    if (i == 0)
        return 1;
    if ((i & (i - 1)) == 0)
        return i;
    if (i & TOP_BIT)
        return TOP_BIT;

    // repeatedly clear bottom bit
    while (i & (i - 1))
        i = i & (i - 1);

    return i << 1;
}

uint32_t int32hash(uint32_t a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

uint64_t int64hash(uint64_t key)
{
    key = (~key) + (key << 21);  // key = (key << 21) - key - 1;
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);  // key * 265
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);  // key * 21
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

uint32_t int64to32hash(uint64_t key)
{
    key = (~key) + (key << 18);  // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21;  // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t)key;
}

#include "lookup3.h"

uint64_t memhash(const char *buf, size_t n)
{
    uint32_t c = 0xcafe8881, b = 0x4d6a087c;

    hashlittle2(buf, n, &c, &b);
    return (uint64_t)c | (((uint64_t)b) << 32);
}

uint32_t memhash32(const char *buf, size_t n)
{
    uint32_t c = 0xcafe8881, b = 0x4d6a087c;

    hashlittle2(buf, n, &c, &b);
    return c;
}
