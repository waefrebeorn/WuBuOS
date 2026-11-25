/*
 * wubu_str.h -- bounded string helpers, one owner.
 *
 * Both the freestanding kernel and the hosted product link wubu_str.o (see
 * mk/objects.mk, the same arrangement as wubu_crc32). It uses no libc: the
 * kernel has its own strlen/memcpy and the host has glibc, and neither is
 * needed to copy a bounded string.
 */
#ifndef WUBU_STR_H
#define WUBU_STR_H

#include <stddef.h>

/* BSD strlcpy semantics: copy at most size-1 bytes of src, always
 * NUL-terminate, never write past dst[size-1]. Returns strlen(src), so
 * (ret >= size) means the copy was truncated.
 *
 * This exists because strncpy() is wrong in both directions:
 *   - it zero-fills the entire remainder, so strncpy(buf, "ok", 65536) writes
 *     65,533 NULs to copy two bytes;
 *   - it does NOT terminate when the source is longer than n.
 * 914 of 1100 call sites in this tree wrote `sizeof(x) - 1` and depended on the
 * destination having been zeroed elsewhere. */
size_t wubu_strlcpy(char *dst, const char *src, size_t size);

#endif
