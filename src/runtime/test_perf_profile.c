/*
 * test_perf_profile.c -- the performance profile must be checkable, not asserted.
 *
 * Asserts four things:
 *   1. the table is non-empty and every knob is a /proc/sys path;
 *   2. reads never crash and report "unavailable" (-1) rather than a bogus match;
 *   3. apply() is HONEST: an applied knob must read back as matching, otherwise it reports
 *      denied -- a write that did not take is not an applied setting;
 *   4. PROVENANCE: every declared value appears in the CachyOS reference sysctl file when that
 *      file is present, so the profile cannot drift away from the distro it was harvested from.
 */
#include "../runtime/wubu_perf_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

/* EVERY file the profile was harvested from. One path was not enough: the first nine values come
 * from 70-cachyos-settings.conf, the rest from the other three, and a provenance test that only
 * knows one source reports the others as invented. */
static const char *const kRefs[] = {
    "/home/wubu/opt/refos/cachyos/rootfs/usr/lib/sysctl.d/70-cachyos-settings.conf",
    "/home/wubu/opt/refos/cachyos/rootfs/usr/lib/sysctl.d/50-pid-max.conf",
    "/home/wubu/opt/refos/cachyos/rootfs/usr/lib/sysctl.d/10-arch.conf",
    "/home/wubu/opt/refos/cachyos/rootfs/usr/lib/sysctl.d/50-default.conf",
    "/home/wubu/opt/refos/cachyos/rootfs/etc/sysctl.d/99-local.conf",
};

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(void) {
    printf("+------------------------------------------------------+\n");
    printf("|  Performance profile test (values from CachyOS)       |\n");
    printf("+------------------------------------------------------+\n\n");

    int n = wubu_perf_knob_count();
    CHECK(n >= 8, "the profile carries a real set of knobs");

    int readable = 0, matched = 0;
    for (int i = 0; i < n; i++) {
        const char *p = wubu_perf_knob_path(i);
        const char *v = wubu_perf_knob_value(i);
        const char *w = wubu_perf_knob_why(i);
        CHECK(p && strncmp(p, "/proc/sys/", 10) == 0, "every knob is a /proc/sys path");
        CHECK(v && *v, "every knob declares a value");
        CHECK(w && *w, "every knob says WHY (a setting without a reason is cargo cult)");

        int m = wubu_perf_matches(i);
        CHECK(m == -1 || m == 0 || m == 1, "matches() returns only -1/0/1");
        if (m == 1) matched++;
        if (m != -1) readable++;
    }
    printf("  current system: %d of %d knobs readable, %d already matching\n", readable, n, matched);
    CHECK(readable > 0, "at least one knob is readable on this kernel");

    /* Honesty of apply: without privileges it must say DENIED, and if it claims APPLIED the
     * value must actually read back. */
    int applied_claims = 0;
    for (int i = 0; i < n; i++) {
        if (wubu_perf_matches(i) == 1) continue;
        int r = wubu_perf_apply(i);
        CHECK(r == PERF_APPLIED || r == PERF_DENIED || r == PERF_UNREADABLE,
              "apply() returns one of the three defined outcomes");
        if (r == PERF_APPLIED) {
            applied_claims++;
            CHECK(wubu_perf_matches(i) == 1, "an APPLIED knob reads back as matching");
        }
    }
    if (applied_claims == 0) printf("  apply: no privileges here -- every change reported as denied\n");

    /* Provenance. */
    char *ref = NULL;
    for (unsigned i = 0; i < sizeof kRefs / sizeof kRefs[0]; i++) {
        char *part = slurp(kRefs[i]);
        if (!part) continue;
        size_t have = ref ? strlen(ref) : 0, add = strlen(part);
        char *merged = realloc(ref, have + add + 2);
        if (!merged) { free(part); break; }
        memcpy(merged + have, part, add + 1);
        if (have) { merged[have + add] = '\n'; merged[have + add + 1] = '\0'; }
        ref = merged;
        free(part);
    }
    if (!ref) {
        printf("  reference sysctl file not present -- provenance check SKIPPED (declared gap)\n");
    } else {
        int found = 0;
        for (int i = 0; i < n; i++) {
            const char *p = wubu_perf_knob_path(i);
            const char *key = strrchr(p, '/');
            key = key ? key + 1 : p;
            /* the file writes "vm.swappiness = 100"; match on the dotted name and the value */
            char dotted[64];
            const char *dir = strstr(p, "/proc/sys/");
            snprintf(dotted, sizeof dotted, "%s", dir ? dir + 10 : p);
            for (char *s = dotted; *s; s++) if (*s == '/') *s = '.';
            char needle[96];
            snprintf(needle, sizeof needle, "%s = %s", dotted, wubu_perf_knob_value(i));
            if (strstr(ref, needle)) found++;
            else printf("  not in reference: %s\n", needle);
            (void)key;
        }
        char msg[128];
        snprintf(msg, sizeof msg, "every knob traces to the CachyOS reference (%d/%d)",
                 found, n);
        CHECK(found == n, msg);
        free(ref);
    }

    printf("\n  Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
