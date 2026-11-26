/*
 * wubu_priority.c -- per-process priority, from the reference desktop's own rule data.
 *
 * WHY THIS EXISTS
 *   The CachyOS reference ships ananicy-cpp: 15,439 rules mapping a program name to a priority
 *   TYPE (Game, BG_CPUIO, Launcher, ...), and 15 types mapping a type to nice / ionice / sched.
 *   We had the sysctl half of its tuning and no live per-process half, so a game and a shader
 *   compiler competed on equal terms -- the measured cause of the frame spikes the user sees.
 *
 * HONESTY RULES
 *   * The type table is DATA, and every value in it is the reference's value; the test proves it
 *     by reading /etc/ananicy.d/00-types.types back.
 *   * apply() reads the priority back with getpriority(). Lowering niceness needs CAP_SYS_NICE, so
 *     without privilege this reports DENIED per process instead of claiming a change.
 *   * Scanning reads the real /proc (WUBU_PROC_ROOT overrides it for tests) and never invents a
 *     process it did not see.
 *
 * C11, minimal includes, no allocation during a scan.
 */
#define _GNU_SOURCE
#include "wubu_priority.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PR_MAX_TYPES 32
#define PR_MAX_RULES 20000   /* the reference carries 15,439; a 4096 cap silently dropped 73 % */
#define PR_NAME_LEN  96

typedef struct {
    char name[PR_NAME_LEN];
    int  nice;
    int  has_nice;
    int  ionice;          /* best-effort level, 0..7 */
    int  has_ionice;
    int  ioclass;         /* 0 none, 1 best-effort, 2 idle, 3 realtime */
    int  sched;           /* 0 normal, 1 idle */
    int  has_sched;
    int  oom;
    int  has_oom;
} PrType;

typedef struct {
    char   name[PR_NAME_LEN];
    PrType t;
} PrRule;

static PrType  g_types[PR_MAX_TYPES];
static int     g_type_count;
static PrRule  g_rules[PR_MAX_RULES];
static int     g_rule_count;
static const char *g_loaded_from;

/* -- the type table, verbatim from the reference -----------------------------
 * Values transcribed from etc/ananicy.d/00-types.types; test_priority.c re-reads that file and
 * fails if any of them has drifted. Kept here as a fallback for a tree with no reference image. */
static const PrType kBuiltinTypes[] = {
    { "Game",          -5, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "LowLatency_RT",-12, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "Player-Audio",  -4, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "Player-Video",  -4, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "Image-View",    -4, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "Doc-View",      -4, 1,  4, 1, 1, 0, 1, 0, 0 },
    { "BG_CPUIO",      16, 1,  7, 1, 2, 1, 1, 0, 0 },
    { "BG_CPU",        14, 1,  4, 1, 1, 1, 1, 0, 0 },
    { "Launcher",      16, 1,  7, 1, 2, 0, 1, 0, 0 },
    { "Heavy_CPU",      9, 1,  7, 1, 1, 0, 0, 0, 0 },
    { "Chat",          -3, 1,  7, 1, 1, 0, 0, 0, 0 },
    { "Service",       10, 1,  6, 1, 1, 0, 0, 0, 0 },
    { "IN_DIFF",        0, 1,  7, 1, 1, 0, 0, 0, 0 },
    { "OOM_KILL",       0, 0,  0, 0, 0, 0, 0, 1, 1000 },
    { "OOM_NO_KILL",    0, 0,  0, 0, 0, 0, 0, 1, -1000 },
};

void wubu_priority_reset(void) {
    g_type_count = g_rule_count = 0;
    g_loaded_from = NULL;
}

int wubu_priority_load_builtin_types(void) {
    g_type_count = 0;
    for (unsigned i = 0; i < sizeof kBuiltinTypes / sizeof kBuiltinTypes[0]; i++)
        g_types[g_type_count++] = kBuiltinTypes[i];
    g_loaded_from = "builtin";
    return g_type_count;
}

int wubu_priority_type_count(void)     { return g_type_count; }
int wubu_priority_rule_count(void)     { return g_rule_count; }
const char *wubu_priority_source(void) { return g_loaded_from ? g_loaded_from : "(none)"; }

/* -- a tiny scanner for the reference's JSON-per-line dialect ------------------
 * The files are one object per line with double-quoted keys. A general JSON parser would be the
 * wrong size here; this reads exactly the fields the format uses and refuses everything else. */
static int field_str(const char *line, const char *key, char *out, size_t cap) {
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(line, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), '"');
    if (!p) return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = '\0';
    return 1;
}

static int field_int(const char *line, const char *key, int *out) {
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(line, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    *out = atoi(p + 1);
    return 1;
}

static void parse_type_object(const char *line, PrType *t) {
    memset(t, 0, sizeof *t);
    field_str(line, "type", t->name, sizeof t->name);
    t->has_nice   = field_int(line, "nice", &t->nice);
    t->has_ionice = field_int(line, "ionice", &t->ionice);
    t->has_oom    = field_int(line, "oom_score_adj", &t->oom);
    char buf[32];
    if (field_str(line, "ioclass", buf, sizeof buf)) {
        t->ioclass = strcmp(buf, "idle") == 0 ? 2 : strcmp(buf, "realtime") == 0 ? 3
                   : strcmp(buf, "best-effort") == 0 ? 1 : 0;
    }
    if (field_str(line, "sched", buf, sizeof buf))
        t->has_sched = 1, t->sched = strcmp(buf, "idle") == 0 ? 1 : 0;
}

/* Load one .types file: every line that names a type. */
int wubu_priority_load_types(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    int before = g_type_count;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || !strstr(line, "\"type\"")) continue;
        if (g_type_count >= PR_MAX_TYPES) break;
        PrType t;
        parse_type_object(line, &t);
        if (t.name[0]) g_types[g_type_count++] = t;
    }
    fclose(f);
    g_loaded_from = path;
    return g_type_count - before;
}

static const PrType *find_type(const char *name) {
    for (int i = 0; i < g_type_count; i++)
        if (strcmp(g_types[i].name, name) == 0) return &g_types[i];
    return NULL;
}

const WubuPriorityRuleView *wubu_priority_type_at(int i) {
    static WubuPriorityRuleView v;
    if (i < 0 || i >= g_type_count) return NULL;
    v.name = g_types[i].name;
    v.nice = g_types[i].has_nice ? g_types[i].nice : 0;
    v.ioclass = g_types[i].ioclass;
    v.ionice = g_types[i].has_ionice ? g_types[i].ionice : 0;
    v.sched_idle = g_types[i].has_sched && g_types[i].sched == 1;
    v.oom_score_adj = g_types[i].has_oom ? g_types[i].oom : -100000;
    return &v;
}

/* Load one .rules file: name -> type (the rule may also carry its own fields). */
int wubu_priority_load_rules(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    int before = g_rule_count;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || !strstr(line, "\"name\"") || strstr(line, "\"type\":")
            && !strstr(line, "\"name\":")) continue;
        if (g_rule_count >= PR_MAX_RULES) break;
        char nm[PR_NAME_LEN], ty[PR_NAME_LEN];
        if (!field_str(line, "name", nm, sizeof nm)) continue;
        PrRule r;
        memset(&r, 0, sizeof r);
        snprintf(r.name, sizeof r.name, "%s", nm);
        if (field_str(line, "type", ty, sizeof ty)) {
            const PrType *t = find_type(ty);
            if (!t) continue;                 /* unknown type: no invented defaults */
            r.t = *t;
        }
        /* a rule may override the type's fields */
        int v;
        if (field_int(line, "nice", &v))   { r.t.nice = v; r.t.has_nice = 1; }
        if (field_int(line, "ionice", &v)) { r.t.ionice = v; r.t.has_ionice = 1; }
        g_rules[g_rule_count++] = r;
    }
    fclose(f);
    return g_rule_count - before;
}

/* Load every .rules file under a directory tree (the reference nests them by category).
 * Depth-bounded so a symlink loop cannot spin: the format is a directory or two at most. */
int wubu_priority_load_rules_dir(const char *dir, int depth) {
    if (depth > 4) return 0;
    DIR *d = opendir(dir);
    if (!d) return -1;
    int loaded = 0;
    struct dirent *e;
    char path[1024];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat sb;
        if (stat(path, &sb) != 0) continue;
        if (S_ISDIR(sb.st_mode)) {
            int sub = wubu_priority_load_rules_dir(path, depth + 1);
            if (sub > 0) loaded += sub;
        } else {
            size_t L = strlen(e->d_name);
            if (L > 6 && strcmp(e->d_name + L - 6, ".rules") == 0) {
                int n = wubu_priority_load_rules(path);
                if (n > 0) loaded += n;
            }
        }
    }
    closedir(d);
    return loaded;
}

const WubuPriorityRuleView *wubu_priority_rule_at(int i) {
    static WubuPriorityRuleView v;
    if (i < 0 || i >= g_rule_count) return NULL;
    v.name = g_rules[i].name;
    v.nice = g_rules[i].t.has_nice ? g_rules[i].t.nice : 0;
    v.ioclass = g_rules[i].t.ioclass;
    v.ionice = g_rules[i].t.has_ionice ? g_rules[i].t.ionice : 0;
    v.sched_idle = g_rules[i].t.has_sched && g_rules[i].t.sched == 1;
    v.oom_score_adj = g_rules[i].t.has_oom ? g_rules[i].t.oom : -100000;
    return &v;
}

/* /proc/<pid>/comm is TRUNCATED TO 15 CHARACTERS by the kernel, so a rule named
 * "fossilize_replay" (16) could never match the process it names. Match on the comm's actual
 * length, and accept a rule whose name is a longer form of it -- that is what the truncation
 * means. A comm that is not a prefix of the rule is still a non-match. */
static const PrRule *match_rule(const char *comm) {
    if (!comm || !*comm) return NULL;
    size_t n = strlen(comm);
    for (int i = 0; i < g_rule_count; i++) {
        if (strncmp(g_rules[i].name, comm, n) != 0) continue;
        if (g_rules[i].name[n] == '\0' || n >= 15) return &g_rules[i];
    }
    return NULL;
}

/* -- ioprio without a libc wrapper ------------------------------------------ */
static int ioprio_set(int which, int who, int ioclass, int level) {
#ifdef SYS_ioprio_set
    int prio = (ioclass << 13) | (level & 0x1ff);
    return (int)syscall(SYS_ioprio_set, which, who, prio);
#else
    (void)which; (void)who; (void)ioclass; (void)level;
    return -1;
#endif
}

const char *wubu_priority_proc_root(void) {
    const char *r = getenv("WUBU_PROC_ROOT");
    return (r && *r) ? r : "/proc";
}

/* Apply one rule to one pid, and verify the priority actually moved. */
int wubu_priority_apply_pid(int pid, const char *comm, WubuPriorityOut *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->pid = pid;
    snprintf(out->comm, sizeof out->comm, "%s", comm ? comm : "");

    const PrRule *r = match_rule(comm ? comm : "");
    if (!r) { out->result = WUBU_PR_OK /* untouched */; out->nice_after = 0; return 0; }

    if (r->t.has_nice) {
        errno = 0;
        int before = getpriority(PRIO_PROCESS, (id_t)pid);
        if (setpriority(PRIO_PROCESS, (id_t)pid, r->t.nice) != 0) {
            out->result = WUBU_PR_DENIED;
            out->nice_after = before;
            return -1;
        }
        int after = getpriority(PRIO_PROCESS, (id_t)pid);
        out->nice_after = after;
        if (after != r->t.nice) {          /* the kernel clamped it (RLIMIT_NICE) */
            out->result = WUBU_PR_CLAMPED;
            return -1;
        }
    }
    if (r->t.ioclass) {
        if (ioprio_set(1 /* IOPRIO_WHO_PROCESS */, pid, r->t.ioclass,
                       r->t.has_ionice ? r->t.ionice : 0) != 0) {
            out->result = WUBU_PR_DENIED;
            return -1;
        }
        out->ionice_applied = 1;
    }
    out->result = WUBU_PR_APPLIED;
    return 0;
}

/* -- one full scan of the process table ------------------------------------- */
int wubu_priority_scan(WubuPriorityOut *out, int cap, WubuPriorityStats *stats) {
    if (stats) memset(stats, 0, sizeof *stats);
    const char *root = wubu_priority_proc_root();
    DIR *d = opendir(root);
    if (!d) return -1;

    int n = 0, matched_count = 0;   /* matches are counted even when the caller wants no detail */
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        char p[512];
        snprintf(p, sizeof p, "%s/%s/comm", root, e->d_name);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char comm[PR_NAME_LEN] = {0};
        if (!fgets(comm, sizeof comm, f)) { fclose(f); continue; }
        fclose(f);
        size_t L = strlen(comm);
        while (L && (comm[L - 1] == '\n' || comm[L - 1] == '\r')) comm[--L] = '\0';

        WubuPriorityOut o;
        int pid = atoi(e->d_name);
        int matched = wubu_priority_apply_pid(pid, comm, &o);
        if (!match_rule(comm)) {                     /* no rule: counted, not touched */
            if (stats) stats->unmatched++;
            continue;
        }
        if (stats) {
            switch (o.result) {
                case WUBU_PR_APPLIED: stats->applied++; break;
                case WUBU_PR_CLAMPED: stats->clamped++; break;
                case WUBU_PR_DENIED:  stats->denied++;  break;
                default: break;
            }
        }
        if (out && n < cap) out[n++] = o;
        matched_count++;
        (void)matched;
    }
    closedir(d);
    if (out) { /* detail array is shorter than the count only when it overflowed */ }

    if (stats) stats->scanned = stats->applied + stats->clamped + stats->denied;
    return matched_count;
}
