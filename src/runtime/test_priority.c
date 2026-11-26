/*
 * test_priority.c -- the priority engine against the REAL reference data and a FAKE process table.
 *
 * Three things are asserted here, and each one can fail for a different reason:
 *   1. PROVENANCE: every value in the built-in type table still appears verbatim in the CachyOS
 *      image's etc/ananicy.d/00-types.types. A embedded table that quietly drifts is worse than
 *      no table, because it looks authoritative.
 *   2. PARSING: the loader reads the reference's own .types and .rules dialect (one object per
 *      line, double-quoted keys) and rejects an unknown type instead of inventing defaults.
 *   3. APPLYING: a real child process is re-niced through setpriority and re-read through
 *      getpriority; a value the kernel will not grant reports CLAMPED or DENIED, never APPLIED.
 *
 * C11, _GNU_SOURCE for setpriority/getpriority prototypes under -std=c11.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wubu_priority.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (cond) { g_pass++; }                                     \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }  \
    } while (0)

#define REF_TYPES "/home/wubu/opt/refos/cachyos/rootfs/etc/ananicy.d/00-types.types"
#define REF_RULES "/home/wubu/opt/refos/cachyos/rootfs/etc/ananicy.d/00-default/Games/steam-shader-compilation.rules"

static char *slurp(const char *p) {
    FILE *f = fopen(p, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    b[got] = '\0';
    fclose(f);
    return b;
}

int main(void) {
    printf("+--------------------------------------------------------------+\n");
    printf("|  Per-process priority (rules from the CachyOS image)          |\n");
    printf("+--------------------------------------------------------------+\n");

    /* -- 1. provenance: the table is the reference's numbers -- */
    char *ref = slurp(REF_TYPES);
    CHECK(ref != NULL, "the reference type table is readable");
    wubu_priority_reset();
    int builtin = wubu_priority_load_builtin_types();
    CHECK(builtin == 15, "fifteen types, as the reference defines");
    if (ref) {
        int traced = 0;
        for (int i = 0; i < wubu_priority_type_count(); i++) {
            const WubuPriorityRuleView *v = wubu_priority_type_at(i);
            char pat[160];
            snprintf(pat, sizeof pat, "\"%s\"", v->name);
            const char *p = strstr(ref, pat);
            if (!p) continue;
            const char *eol = strchr(p, '\n');
            char line[512];
            size_t L = eol ? (size_t)(eol - p) : strlen(p);
            if (L >= sizeof line) L = sizeof line - 1;
            memcpy(line, p, L);
            line[L] = '\0';
            char num[32];
            snprintf(num, sizeof num, "%d", v->nice);
            if (strstr(line, num)) traced++;
        }
        CHECK(traced == 15, "every built-in nice value appears in the reference file");
    }

    /* -- 2. parsing the reference's own dialect -- */
    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    int n = wubu_priority_load_types(REF_TYPES);
    CHECK(n == 15, "the loader reads all fifteen reference types");
    int game = -1, bg = -1;
    for (int i = 0; i < wubu_priority_type_count(); i++) {
        if (strcmp(wubu_priority_type_at(i)->name, "Game") == 0) game = i;
        if (strcmp(wubu_priority_type_at(i)->name, "BG_CPUIO") == 0) bg = i;
    }
    CHECK(game >= 0 && wubu_priority_type_at(game)->nice == -5, "Game parses to nice -5");
    CHECK(bg >= 0 && wubu_priority_type_at(bg)->nice == 16 && wubu_priority_type_at(bg)->ioclass == 2,
          "BG_CPUIO parses to nice 16 with the idle ioclass");

    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    int r = wubu_priority_load_rules(REF_RULES);
    CHECK(r == 1, "the shader-compilation rule loads");
    CHECK(wubu_priority_rule_count() == 1 &&
          strcmp(wubu_priority_rule_at(0)->name, "fossilize_replay") == 0,
          "and it names fossilize_replay");
    CHECK(wubu_priority_rule_at(0)->nice == 16, "resolved through its BG_CPUIO type, not invented");

    /* an unknown type must be DROPPED, not given a default */
    {
        const char *tmp = "/tmp/wubu-priority-unknown.rules";
        FILE *f = fopen(tmp, "w");
        fprintf(f, "{ \"name\": \"mystery\", \"type\": \"NoSuchType\" }\n");
        fclose(f);
        int before = wubu_priority_rule_count();
        wubu_priority_load_rules(tmp);
        CHECK(wubu_priority_rule_count() == before, "an unknown type produces no rule");
        unlink(tmp);
    }

    /* -- 2b. the WHOLE reference tree, not one file --
     * The first live run loaded 4,096 rules because of a cap and nobody noticed; this assertion
     * is the reason that class of bug cannot come back silently. It is a bound, not a fingerprint:
     * the tree may grow, but a 73 % collapse must fail. */
    {
        const char *dir = "/home/wubu/opt/refos/cachyos/rootfs/etc/ananicy.d";
        wubu_priority_reset();
        int ty = wubu_priority_load_types(REF_TYPES);
        int rules = wubu_priority_load_rules_dir(dir, 0);
        CHECK(ty == 15, "the tree's type table loads");
        CHECK(rules > 15000 && rules < 16000, "the whole rule tree loads (>15k, <16k)");
        CHECK(wubu_priority_rule_count() == rules, "and every loaded rule is retained");
    }

    /* -- 3. applying to a real process -- */
    /* A fake process table so the scan sees exactly what the test put there. */
    const char *proc = "/tmp/wubu-priority-proc";
    char cmd[256];
    snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s/4242' '%s/4343'", proc, proc, proc);
    if (system(cmd) != 0) { fprintf(stderr, "warn: could not build fake proc\n"); }
    FILE *c1 = fopen("/tmp/wubu-priority-proc/4242/comm", "w");
    fprintf(c1, "fossilize_replay\n"); fclose(c1);
    FILE *c2 = fopen("/tmp/wubu-priority-proc/4343/comm", "w");
    fprintf(c2, "something_else\n"); fclose(c2);

    setenv("WUBU_PROC_ROOT", proc, 1);
    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    wubu_priority_load_rules(REF_RULES);

    WubuPriorityStats st;
    int seen = wubu_priority_scan(NULL, 0, &st);
    CHECK(seen >= 1, "the scan finds the ruled process");
    CHECK(st.unmatched == 1, "and counts the unrelated one without touching it");

    /* A real child: raising niceness is allowed without privilege, lowering is not. */
    pid_t child = fork();
    if (child == 0) { pause(); _exit(0); }
    CHECK(child > 0, "child spawned");

    WubuPriorityOut o;
    int rc = wubu_priority_apply_pid((int)child, "fossilize_replay", &o);
    CHECK(rc == 0 && o.result == WUBU_PR_APPLIED, "a background rule applies to a real pid");
    CHECK(getpriority(PRIO_PROCESS, child) == 16, "and getpriority reads back exactly 16");

    /* A Game rule lowers niceness: without CAP_SYS_NICE the kernel refuses. */
    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    wubu_priority_load_types(REF_TYPES);
    {
        const char *tmp = "/tmp/wubu-priority-game.rules";
        FILE *f = fopen(tmp, "w");
        fprintf(f, "{ \"name\": \"mygame\", \"type\": \"Game\" }\n");
        fclose(f);
        wubu_priority_load_rules(tmp);
        unlink(tmp);
    }
    rc = wubu_priority_apply_pid((int)child, "mygame", &o);
    CHECK(rc != 0 && (o.result == WUBU_PR_DENIED || o.result == WUBU_PR_CLAMPED),
          "lowering niceness without privilege is DENIED or CLAMPED, never APPLIED");

    /* -- oom_score_adj: the one policy an unprivileged owner may RAISE --
     * The fake proc root is for the SCAN only; writing oom_score_adj must reach the kernel, so
     * the seam is dropped here. Leaving it set made this block fail for the right reason: the
     * engine refused to claim a write into a directory that has no such file. */
    unsetenv("WUBU_PROC_ROOT");
    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    {
        const char *tmp = "/tmp/wubu-priority-oom.rules";
        FILE *f = fopen(tmp, "w");
        fprintf(f, "{ \"name\": \"oomtest\", \"oom_score_adj\": 500 }\n");
        fclose(f);
        CHECK(wubu_priority_load_rules(tmp) == 1, "an inline oom_score_adj rule loads");
        unlink(tmp);
    }
    rc = wubu_priority_apply_pid((int)child, "oomtest", &o);
    CHECK(rc == 0 && o.result == WUBU_PR_APPLIED, "raising oom_score_adj applies");
    {
        char op[128], line[64] = {0};
        snprintf(op, sizeof op, "/proc/%d/oom_score_adj", (int)child);
        FILE *f = fopen(op, "r");
        if (f) { if (!fgets(line, sizeof line, f)) line[0] = 0; fclose(f); }
        CHECK(strncmp(line, "500", 3) == 0, "and /proc/<pid>/oom_score_adj reads back 500");
    }
    /* OOM_NO_KILL wants -1000, which needs CAP_SYS_RESOURCE: must be refused, not faked. */
    wubu_priority_reset();
    wubu_priority_load_builtin_types();
    wubu_priority_load_types(REF_TYPES);
    {
        const char *tmp = "/tmp/wubu-priority-nokill.rules";
        FILE *f = fopen(tmp, "w");
        fprintf(f, "{ \"name\": \"oomtest\", \"type\": \"OOM_NO_KILL\" }\n");
        fclose(f);
        wubu_priority_load_rules(tmp);
        unlink(tmp);
    }
    rc = wubu_priority_apply_pid((int)child, "oomtest", &o);
    CHECK(rc != 0 && o.result == WUBU_PR_DENIED, "lowering oom_score_adj is DENIED without privilege");

    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", proc);
    if (system(cmd) != 0) { /* nothing to do */ }

    free(ref);
    printf("\n  Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
