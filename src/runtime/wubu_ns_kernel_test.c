/*
 * wubu_ns_kernel_test.c -- verify the kernel + hw control plane (rip off
 * CachyOS kernel-manager / chwd through /n). Uses an injected mock GPU
 * detector (DI) so the test needs no proton2/host-exec graph. Scheduler is a
 * pure state store. Own small binary (link-budget discipline).
 */

#define _GNU_SOURCE
#include "wubu_ns_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else      { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

static char g_tmp[4096];
static int file_exists(const char *rel) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    struct stat st; return stat(p, &st) == 0;
}
static int file_contains(const char *rel, const char *needle) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char buf[4096]; int found = 0;
    while (fgets(buf, sizeof(buf), f))
        if (strstr(buf, needle)) { found = 1; break; }
    fclose(f); return found;
}

/* mock GPU detector: pretend an AMD GPU at a fixed BDF */
static int mock_gpu_detect(char *name, int name_len, char *pci, int pci_len) {
    snprintf(name, name_len, "amd");
    snprintf(pci, pci_len, "0000:03:00.0");
    return 0;
}

static void test_kernel_namespace(void) {
    printf("\n-- Kernel + HW control plane (rip off kernel-manager / chwd) --\n");
    wubu_ns_bridge_create(g_tmp);

    int rc = wubu_ns_publish_kernel(mock_gpu_detect);
    CHECK(rc == 0, "publish kernel + hw control plane");
    CHECK(file_exists("kernel/scheduler"), "/n/kernel/scheduler exists");
    CHECK(file_exists("hw/amd/mode"),       "/n/hw/amd/mode exists (detected GPU)");
    CHECK(file_exists("hw/amd/pci"),         "/n/hw/amd/pci exists");
    CHECK(file_contains("hw/amd/mode", "passthrough"),
          "/n/hw/amd/mode defaults to passthrough");

    /* scheduler: set + read-back + file reflects it */
    CHECK(wubu_ns_sched_set("bore") == 0, "sched_set bore accepted");
    CHECK(strcmp(wubu_ns_sched_get(), "bore") == 0, "sched_get returns bore");
    CHECK(file_contains("kernel/scheduler", "bore"),
          "/n/kernel/scheduler file reflects bore");

    CHECK(wubu_ns_sched_set("rt") == 0, "sched_set rt accepted");
    CHECK(strcmp(wubu_ns_sched_get(), "rt") == 0, "sched_get returns rt");

    CHECK(wubu_ns_sched_set("nonsense") == -1, "invalid policy rejected (-1)");

    /* hw mode switch (chwd vibe) */
    CHECK(wubu_ns_hw_set_mode("amd", "virtual") == 0, "hw_set_mode amd=virtual");
    CHECK(file_contains("hw/amd/mode", "virtual"),
          "/n/hw/amd/mode reflects the switch");
}

int main(void) {
    char tmpl[] = "/tmp/wubu_ns_kern_XXXXXX";
    char *d = mkdtemp(tmpl);
    if (!d) { printf("mkdtemp failed\n"); return 1; }
    strcpy(g_tmp, d);

    test_kernel_namespace();

    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);

    printf("\n==================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}
