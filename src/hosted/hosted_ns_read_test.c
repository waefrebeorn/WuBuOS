/* hosted_ns_read_test.c -- the gate for styxfs_ns_read (namespace bytes, one owner).
 *
 * The reader was first exercised only through the preview pane and a screenshot. A screenshot
 * proves a picture appeared; it cannot prove that a DIRECTORY is refused, that an unknown name
 * is refused, or that both "/name" and "name" resolve to the same bytes.
 *
 * g_fs is defined here rather than linking the whole 9P server: the function's contract is
 * about the table and the name, so the test owns the table.
 */
#include "hosted_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int styxfs_ns_read(const char *path, unsigned char **out, size_t *out_len);

styxfs_file_t g_fs[STYXFS_MAX_FILES];
int           g_nfiles = 0;
uint64_t      g_next_path = 1;

static int pass, fail;
static void check(int c, const char *w) { if (c) { pass++; return; } printf("  [FAIL] %s\n", w); fail++; }

static void add(const char *name, int dir, const char *data) {
    styxfs_file_t *f = &g_fs[g_nfiles++];
    memset(f, 0, sizeof *f);
    strncpy(f->name, name, sizeof f->name - 1);
    f->qtype = dir ? STX_QTDIR : STX_QTFILE;
    f->path = g_next_path++;
    if (data) {
        size_t n = strlen(data);
        memcpy(f->data, data, n);
        f->data_len = n;
        f->length = (uint32_t)n;
    }
}

int main(void) {
    printf("=== hosted_ns_read (gate for the namespace byte reader) ===\n");
    add("prog", 1, NULL);
    add("photo.jpg", 0, "WUBU!");

    unsigned char *b = NULL; size_t n = 0;
    check(styxfs_ns_read("photo.jpg", &b, &n) == 0, "reads by root-relative name");
    check(n == 5 && b && memcmp(b, "WUBU!", 5) == 0, "returns the file's own bytes");
    free(b); b = NULL; n = 0;

    check(styxfs_ns_read("/photo.jpg", &b, &n) == 0, "reads by the listing's path form");
    check(n == 5 && b && memcmp(b, "WUBU!", 5) == 0, "both path forms give the same bytes");
    free(b); b = NULL; n = 0;

    check(styxfs_ns_read("prog", &b, &n) != 0, "a directory has no bytes");
    check(styxfs_ns_read("nothing.png", &b, &n) != 0, "an unknown name is refused");
    check(styxfs_ns_read(NULL, &b, &n) != 0, "a null path is refused");
    check(styxfs_ns_read("photo.jpg", NULL, &n) != 0, "a null output is refused");

    /* an empty file is a refusal, not a zero-length success: a caller that gets 0 bytes back
     * would feed a decoder an empty buffer and report "cannot decode" for a file that exists */
    add("empty.txt", 0, NULL);
    b = NULL; n = 0;
    check(styxfs_ns_read("empty.txt", &b, &n) != 0, "an empty file is refused rather than returning 0 bytes");
    check(b == NULL, "a refusal leaves the output pointer null");

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
