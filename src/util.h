
#ifndef __UTIL_H
#define __UTIL_H

#include <string.h>
#include <stdlib.h>

#include "tty.h"

static inline boolean streq(const char *a, const char *b) {
    return strcmp(a,b) == 0;
} 

// Call calloc(), failing on error.
static inline void *ck_calloc(size_t count, size_t size) {
    void *result = calloc(count, size);
    if (!result) { sysError("Memory error: calloc() failed.",""); }
    return result;
}

// This is wrong (assumes 16-bit int) but I'm keeping it for now until
// I get a better set of tests.
static inline int longCanBeInt(long l) {
    return l >= OBJINT_MIN && l <= OBJINT_MAX;
}

#endif
