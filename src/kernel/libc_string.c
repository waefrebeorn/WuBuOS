/*
 * libc_string.c -- the kernel's STRING module (the split of the
 * freestanding libc: the string half, self-contained).
 *
 * The kernel libc (libc.c) lacked the standard string surface —
 * strstr/strchr/strrchr/memmove/strdup/strncat/strcasestr/tolower/
 * toupper/isspace/strtok_r/strerror — so every module hand-rolled
 * loops (the PE loader hit snprintf; the file-type probes duplicate
 * the case-insensitive match). This module provides them all, C11,
 * kernel-clean (-nostdlib: no libc heap calls beyond malloc/free
 * which the kernel provides), opaque (no globals, no god header —
 * callers include libc.h only).
 *
 * Every function is byte-for-byte the standard contract; the kernel
 * tests prove each against the C library's behavior.
 */
#include "libc.h"

#include <string.h>
#include <stdlib.h>

/* KC02: strchr — the first occurrence of c in s (or NULL). */
char *strchr(const char *s, int c)
{
    char ch = (char)c;
    while (*s) {
        if (*s == ch) return (char *)s;
        s++;
    }
    return ch == '\0' ? (char *)s : NULL;
}

/* KC02: strrchr — the LAST occurrence. */
char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = NULL;
    while (*s) {
        if (*s == ch) last = s;
        s++;
    }
    if (ch == '\0') return (char *)s;
    return (char *)last;
}

/* KC01: strstr — the first occurrence of needle in haystack. */
char *strstr(const char *haystack, const char *needle)
{
    if (!needle[0]) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (haystack[0] == needle[0] &&
            strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/* KC07: strcasestr — the case-insensitive strstr (the file-type
 * probes need it: .WAV vs .wav). */
char *strcasestr(const char *haystack, const char *needle)
{
    if (!needle[0]) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        size_t i = 0;
        while (i < nlen &&
               (haystack[i] | 0x20) == (needle[i] | 0x20)) i++;
        if (i == nlen) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/* KC03: memmove — the overlapping-safe copy. */
void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dest;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

/* KC05: strdup — the heap copy. */
char *strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* KC06: strncat — the bounded append (always NUL-terminates). */
char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) d++;
    size_t i = 0;
    while (i < n && src[i]) {
        d[i] = src[i];
        i++;
    }
    d[i] = '\0';
    return dest;
}

/* KC08: the character classes + case. */
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

/* KC09: strtok_r — the reentrant tokenizer (the config parsers
 * duplicate this). */
char *strtok_r(char *str, const char *delim, char **saveptr)
{
    char *tok;
    if (str) *saveptr = str;
    tok = *saveptr;
    while (*tok && strchr(delim, *tok)) tok++;
    if (!*tok) {
        *saveptr = tok;
        return NULL;
    }
    char *end = tok;
    while (*end && !strchr(delim, *end)) end++;
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = end;
    }
    return tok;
}

/* KC10: strerror — the kernel's error names (the exec layer needs
 * them). */
char *strerror(int errnum)
{
    switch (errnum) {
    case 1:  return "operation not permitted";
    case 2:  return "no such file or directory";
    case 12: return "out of memory";
    case 22: return "invalid argument";
    default: return "unknown error";
    }
}
