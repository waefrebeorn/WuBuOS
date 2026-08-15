/* wubu_test.h — Shared test utilities for WuBuOS
 *
 * Self-contained header: include in any test .c file that needs common
 * test macros and helpers. Replaces duplicated FAIL/CHECK macros and
 * read_file utilities across 29+ test files.
 *
 * Usage: #include "wubu_test.h"
 */

#ifndef WUBU_TEST_H
#define WUBU_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- Test result tracking ---- */
static int wubu_test_pass = 0;
static int wubu_test_fail = 0;
static int wubu_test_total = 0;

#define WUBU_TEST_START() do { wubu_test_pass = wubu_test_fail = wubu_test_total = 0; } while(0)

#define WUBU_CHECK(cond, msg) do { \
    wubu_test_total++; \
    if (cond) { wubu_test_pass++; } \
    else { wubu_test_fail++; printf("FAIL: %s\n", msg); } \
} while(0)

#define WUBU_TEST_SUMMARY() do { \
    printf("=== %s: %d/%d passed, %d failed ===\n", \
           __func__, wubu_test_pass, wubu_test_total, wubu_test_fail); \
} while(0)

#define WUBU_TEST_EXIT() do { \
    printf("\n=== SUMMARY: %d/%d passed, %d failed ===\n", \
           wubu_test_pass, wubu_test_total, wubu_test_fail); \
    return wubu_test_fail > 0 ? 1 : 0; \
} while(0)

/* ---- File I/O helpers ---- */

/* Read a text file into a buffer. Returns 1 on success, 0 on failure.
 * Strips trailing whitespace/newlines. */
static int wubu_test_read_file(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' ' || out[n-1] == '\r'))
        out[--n] = '\0';
    return 1;
}

/* Write a text file. Returns 1 on success, 0 on failure. */
static int wubu_test_write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

/* Create a directory path (like mkdir -p). Returns 0 on success. */
static int wubu_test_mkpath(const char *p) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "mkdir -p '%s'", p);
    return system(buf);
}

#endif /* WUBU_TEST_H */

/* ---- Legacy FAIL macro (for backward compatibility) ---- */
#ifndef FAIL
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while(0)
#endif
