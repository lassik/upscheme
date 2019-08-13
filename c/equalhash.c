#include <assert.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtypes.h"
#include "utils.h"
#include "utf8.h"
#include "ios.h"
#include "socket.h"
#include "timefuncs.h"
#include "hashing.h"
#include "htable.h"
#include "htableh_inc.h"
#include "bitvector.h"
#include "fs.h"
#include "random.h"
#include "llt.h"

#include "flisp.h"
#include "equalhash.h"

#include "htable_inc.h"

#define _equal_lispvalue_(x, y) equal_lispvalue((value_t)(x), (value_t)(y))

HTIMPL(equalhash, hash_lispvalue, _equal_lispvalue_)
