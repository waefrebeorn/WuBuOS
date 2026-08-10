/*
 * libc_string_test.c -- the kernel string module test.
 *
 * Proves the new libc surface (KC01-KC10 + KC04) against the
 * standard contracts:
 *   1. strstr / strcasestr find the needle
 *   2. strchr / strrchr find first/last
 *   3. memmove overlaps safely
 *   4. strdup + strncat
 *   5. tolower/toupper/isspace
 *   6. strtok_r tokenizes
 *   7. snprintf truncates + terminates
 *   8. strerror names errors
 */
#include "libc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== libc_string_test (the kernel string module) ===\n");

    /* 1. strstr + strcasestr */
    if (strstr("hello world", "world") == NULL)
        FAIL("strstr miss");
    if (strstr("hello", "xyz") != NULL) FAIL("strstr false hit");
    if (strcasestr("OPENARENA.WAV", ".wav") == NULL)
        FAIL("strcasestr case");
    printf("  PASS: strstr + strcasestr\n");

    /* 2. strchr + strrchr */
    const char *s = "a/b/c";
    if (strchr(s, '/') != s + 1) FAIL("strchr first");
    if (strrchr(s, '/') != s + 3) FAIL("strrchr last");
    if (strrchr(s, 'z') != NULL) FAIL("strrchr miss");
    printf("  PASS: strchr + strrchr\n");

    /* 3. memmove overlap */
    char buf[16] = "abcdefghij";
    memmove(buf + 2, buf, 6);
    if (strncmp(buf, "ababcdef", 8) != 0) FAIL("memmove fwd");
    char buf2[16] = "abcdefghij";
    memmove(buf2, buf2 + 2, 6);
    if (strncmp(buf2, "cdefgh", 6) != 0) FAIL("memmove back");
    printf("  PASS: memmove (both directions)\n");

    /* 4. strdup + strncat */
    char *dup = strdup("kernel");
    if (!dup || strcmp(dup, "kernel") != 0) FAIL("strdup");
    free(dup);
    char cat[16] = "ab";
    strncat(cat, "cdef", 2);
    if (strcmp(cat, "abcd") != 0) FAIL("strncat: %s", cat);
    printf("  PASS: strdup + strncat\n");

    /* 5. the character classes */
    if (tolower('A') != 'a' || toupper('z') != 'Z') FAIL("case");
    if (!isspace(' ') || !isspace('\t') || isspace('x')) FAIL("isspace");
    printf("  PASS: tolower/toupper/isspace\n");

    /* 6. strtok_r */
    char line[32] = "a,b,c";
    char *save = NULL;
    char *t = strtok_r(line, ",", &save);
    if (!t || strcmp(t, "a") != 0) FAIL("tok1: %s", t ? t : "NULL");
    t = strtok_r(NULL, ",", &save);
    if (!t || strcmp(t, "b") != 0) FAIL("tok2");
    t = strtok_r(NULL, ",", &save);
    if (!t || strcmp(t, "c") != 0) FAIL("tok3");
    if (strtok_r(NULL, ",", &save) != NULL) FAIL("tok4 not NULL");
    printf("  PASS: strtok_r\n");

    /* 7. snprintf truncates + terminates */
    char small[4];
    int n = snprintf(small, sizeof(small), "%s", "wubuwizard");
    if (n != 10) FAIL("snprintf len = %d", n);
    if (strcmp(small, "wub") != 0) FAIL("snprintf trunc: %s", small);
    printf("  PASS: snprintf (bounded + terminated)\n");

    /* 8. strerror */
    if (strstr(strerror(2), "no such file") == NULL) FAIL("strerror(2)");
    printf("  PASS: strerror\n");

    printf("=== ALL LIBC-STRING TESTS PASSED (the kernel libc is whole) ===\n");
    return 0;
}
