#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <setjmp.h>

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
#include "dirpath.h"
#include "random.h"
#include "llt.h"

#include "flisp.h"
#include "equalhash.h"

#include "htable_inc.h"

#define _equal_lispvalue_(x, y) equal_lispvalue((value_t)(x), (value_t)(y))

HTIMPL(equalhash, hash_lispvalue, _equal_lispvalue_)
