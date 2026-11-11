/*
 * wubu_ns_kernels_test.c -- the /n/kernels subtree test.
 *
 * Proves the namespace is a real window on the interchange, over a REAL ns
 * root: publish creates the files, refresh re-renders them from live state,
 * and the operator's select path changes what the files say.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_kernels.h"
#include "wubu_kernel_xchg.h"

#include <stdio.h>
#include <string.h>

#define NSROOT "/tmp/ns_kernels_test"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static int demo_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    (void)regs;
    if (num == 4242) { *out = 42; return 1; }
    return 0;
}
static const wubu_kernel_ops_t k_demo = {
    "demo", 1, 1, "demo", demo_try, NULL, NULL
};

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return (int)n;
}

int main(void)
{
    printf("=== test_ns_kernels ===\n");
    char path[256], buf[2048];

    *(const char **)&g_ns_root = NSROOT;
    CHECK(wubu_ns_publish_kernels() == 0, "publish creates the subtree");

    snprintf(path, sizeof(path), "%s/kernels/active", g_ns_root);
    CHECK(read_file(path, buf, sizeof(buf)) > 0, "active is readable");
    CHECK(strcmp(buf, "(none)") == 0, "nothing is active before any selection");

    snprintf(path, sizeof(path), "%s/kernels/list", g_ns_root);
    CHECK(read_file(path, buf, sizeof(buf)) > 0, "list is readable");
    CHECK(strstr(buf, "kernels 0") != NULL, "the list reflects the real registry");

    /* the control path is fail-closed too */
    CHECK(wubu_ns_kernels_select("demo") == -1, "selecting an unregistered kernel fails");
    CHECK(wubu_ns_kernels_select("") == -1, "an empty name is rejected");
    CHECK(wubu_ns_kernels_select(NULL) == -1, "a null name is rejected");

    CHECK(wubu_kernel_register(&k_demo) == 0, "a kernel is registered");
    CHECK(wubu_ns_kernels_select("demo") == 0, "select through /n works");
    CHECK(wubu_ns_kernels_refresh() == 0, "refresh succeeds");

    snprintf(path, sizeof(path), "%s/kernels/active", g_ns_root);
    CHECK(read_file(path, buf, sizeof(buf)) > 0, "active re-read");
    CHECK(strcmp(buf, "demo") == 0, "the namespace shows the new active kernel");

    snprintf(path, sizeof(path), "%s/kernels/history", g_ns_root);
    CHECK(read_file(path, buf, sizeof(buf)) > 0, "history is readable");
    CHECK(strstr(buf, "-> demo") != NULL, "the swap was logged");

    snprintf(path, sizeof(path), "%s/kernels/list", g_ns_root);
    CHECK(read_file(path, buf, sizeof(buf)) > 0, "list re-read");
    CHECK(strstr(buf, "kernel demo ") != NULL, "the new kernel appears in the list");
    CHECK(strstr(buf, "active demo") != NULL, "the list marks the active kernel");

    if (failures == 0) printf("ALL NS_KERNELS TESTS PASSED\n");
    else printf("%d NS_KERNELS FAILURES\n", failures);
    return failures ? 1 : 0;
}
