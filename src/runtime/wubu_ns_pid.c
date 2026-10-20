/*
 * wubu_ns_pid.c -- per-task namespace subtree at /n/pid/{tid}
 * (NAMESPACE #598). See wubu_ns_pid.h for the contract.
 */
#include "wubu_ns_pid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R_READ  0x1ULL   /* WUBU_RIGHT_READ stand-in */
#define R_WRITE 0x2ULL   /* WUBU_RIGHT_WRITE stand-in */

struct wubu_ns_pid_registry {
    struct {
        uint64_t        tid;
        wubu_ns_root_t *view;
        int             used;
    } slots[WUBU_NS_PID_MAX_TASKS];
    int n;
};

/* The singleton: the /n/pid tree is one kernel-wide registry. */
static wubu_ns_pid_registry_t g_pidreg;

void wubu_ns_pid_reset(void)
{
    for (int i = 0; i < WUBU_NS_PID_MAX_TASKS; i++) {
        if (g_pidreg.slots[i].used && g_pidreg.slots[i].view)
            wubu_ns_root_free(g_pidreg.slots[i].view);
        g_pidreg.slots[i].used = 0;
        g_pidreg.slots[i].view = NULL;
        g_pidreg.slots[i].tid  = 0;
    }
    g_pidreg.n = 0;
}

int wubu_ns_pid_register(uint64_t tid, const char *src_base)
{
    if (!src_base || !src_base[0]) return -1;
    for (int i = 0; i < WUBU_NS_PID_MAX_TASKS; i++)
        if (g_pidreg.slots[i].used && g_pidreg.slots[i].tid == tid)
            return -2;                       /* duplicate: one view per task */
    int slot = -1;
    for (int i = 0; i < WUBU_NS_PID_MAX_TASKS; i++) {
        if (!g_pidreg.slots[i].used) { slot = i; break; }
    }
    if (slot < 0) return -3;                 /* registry full */

    char name[WUBU_NS_ROOT_MAX_NAME];
    char at[WUBU_NS_ROOT_MAX_PATH];
    snprintf(name, sizeof(name), "pid-%llu", (unsigned long long)tid);
    snprintf(at, sizeof(at), "/n/pid/%llu", (unsigned long long)tid);

    wubu_ns_root_t *v = wubu_ns_root_create(name, WUBU_NS_ROOT_HOST);
    if (!v) return -1;
    /* The task's own base is read-write INSIDE its subtree. */
    if (wubu_ns_root_grant(v, src_base, at, R_READ | R_WRITE, 0) != 0) {
        wubu_ns_root_free(v);
        return -1;
    }
    g_pidreg.slots[slot].tid  = tid;
    g_pidreg.slots[slot].view = v;
    g_pidreg.slots[slot].used = 1;
    g_pidreg.n++;
    return 0;
}

wubu_ns_root_t *wubu_ns_pid_lookup(uint64_t tid)
{
    for (int i = 0; i < WUBU_NS_PID_MAX_TASKS; i++)
        if (g_pidreg.slots[i].used && g_pidreg.slots[i].tid == tid)
            return g_pidreg.slots[i].view;
    return NULL;
}

int wubu_ns_pid_unregister(uint64_t tid)
{
    for (int i = 0; i < WUBU_NS_PID_MAX_TASKS; i++) {
        if (g_pidreg.slots[i].used && g_pidreg.slots[i].tid == tid) {
            if (g_pidreg.slots[i].view)
                wubu_ns_root_free(g_pidreg.slots[i].view);
            g_pidreg.slots[i].used = 0;
            g_pidreg.slots[i].view = NULL;
            g_pidreg.slots[i].tid  = 0;
            g_pidreg.n--;
            return 0;
        }
    }
    return -1;
}

int wubu_ns_pid_count(void)
{
    return g_pidreg.n;
}

const char *wubu_ns_pid_path(uint64_t tid, char *out, size_t cap)
{
    if (!out || cap < 16) return NULL;
    snprintf(out, cap, "/n/pid/%llu", (unsigned long long)tid);
    return out;
}
