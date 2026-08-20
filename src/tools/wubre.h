/*
 * wubre.h - WuBu Regex Engine. "occupational supremacy of C11 code".
 * ---------------------------------------------------------------------------
 * Our OWN regex engine (no vectorscan, no PCRE, no RE2, no libstdc++).
 * Thompson NFA simulation - guaranteed linear-ish time, no catastrophic
 * backtracking. Supports ERE and BRE (GNU grep -E / -G semantics), case
 * insensitive matching, and byte-exact search over a buffer.
 *
 * Limitations (documented, same as ripgrep's engine):
 *   - No backreferences (\1 ..) - NFA cannot express them.
 *   - No counted repetition {m,n} - rare; document rather than fake.
 *
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#ifndef WUBRE_H
#define WUBRE_H

#include <stddef.h>
#include <stdbool.h>

typedef struct WURegex WURegex;

#define WUBRE_ERE   0x0   /* extended regex (grep -E) */
#define WUBRE_BRE   0x1   /* basic regex   (grep -G, default) */
#define WUBRE_ICASE 0x2   /* case insensitive */
#define WUBRE_DOTNL 0x4   /* dot matches newline (default: it does not) */

/* Compile a pattern. On error returns NULL and writes a message to err. */
WURegex *wubre_compile(const char *pat, int flags, char *err, size_t errsz);

/* Search buf[0..n) for a match. Returns true iff the regex matches
 * anywhere in the buffer (unanchored, per-line semantics: ^ = pos 0,
 * $ = pos n). Does NOT require NUL termination. */
bool wubre_search(const WURegex *re, const unsigned char *buf, size_t n);

void wubre_free(WURegex *re);

#endif /* WUBRE_H */
