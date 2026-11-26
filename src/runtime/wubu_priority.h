/*
 * wubu_priority.h -- per-process priority rules, harvested from the reference desktop.
 *
 * The CachyOS image carries ananicy-cpp with 15,439 name->type rules and 15 types that carry
 * nice / ionice / sched. This module loads that data and applies it, verifying every change.
 */
#ifndef WUBU_PRIORITY_H
#define WUBU_PRIORITY_H

typedef enum {
    WUBU_PR_OK = 0,     /* matched nothing, so nothing to do */
    WUBU_PR_APPLIED,    /* written and read back */
    WUBU_PR_CLAMPED,    /* the kernel clamped the value (RLIMIT_NICE) */
    WUBU_PR_DENIED      /* no privilege */
} WubuPriorityResult;

typedef struct {
    const char *name;
    int nice;
    int ioclass;        /* 0 none, 1 best-effort, 2 idle, 3 realtime */
    int ionice;
    int sched_idle;
    int oom_score_adj;  /* -100000 means "type does not set it" */
} WubuPriorityRuleView;

typedef struct {
    int  pid;
    char comm[96];
    int  nice_after;
    int  ionice_applied;
    int  oom_applied;
    WubuPriorityResult result;
} WubuPriorityOut;

typedef struct {
    int scanned;    /* processes a rule touched */
    int applied;
    int clamped;
    int denied;
    int unmatched;
} WubuPriorityStats;

void wubu_priority_reset(void);
int  wubu_priority_load_builtin_types(void);
int  wubu_priority_load_types(const char *path);
int  wubu_priority_load_rules(const char *path);

int  wubu_priority_dropped(int which);   /* 0 unknown type, 1 no policy, 2 unparsed name */
int  wubu_priority_type_count(void);
int  wubu_priority_rule_count(void);
const char *wubu_priority_source(void);
const WubuPriorityRuleView *wubu_priority_type_at(int i);
const WubuPriorityRuleView *wubu_priority_rule_at(int i);

const char *wubu_priority_proc_root(void);
int  wubu_priority_apply_pid(int pid, const char *comm, WubuPriorityOut *out);
int  wubu_priority_scan(WubuPriorityOut *out, int cap, WubuPriorityStats *stats);
int  wubu_priority_load_rules_dir(const char *dir, int depth);

#endif /* WUBU_PRIORITY_H */
