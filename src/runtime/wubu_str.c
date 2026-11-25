/*
 * wubu_str.c -- single implementation of wubu_strlcpy (see wubu_str.h).
 *
 * Deliberately libc-free: it is linked into the freestanding kernel as well as
 * the hosted product, so it cannot call memcpy from either side.
 */
#include "wubu_str.h"

size_t wubu_strlcpy(char *dst, const char *src, size_t size)
{
    size_t n = 0;
    while (src[n]) n++;
    if (size) {
        size_t c = (n >= size) ? size - 1 : n;
        for (size_t i = 0; i < c; i++) dst[i] = src[i];
        dst[c] = '\0';
    }
    return n;
}
