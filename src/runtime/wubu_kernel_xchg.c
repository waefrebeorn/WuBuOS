/*
 * wubu_kernel_xchg.c -- the kernel interchange network.
 *
 * A registry of OS kernels with a versioned contract, an ACTIVE selection that
 * can be swapped at runtime, and a router that lets the first kernel claiming
 * a syscall answer it.  See wubu_kernel_xchg.h for the contract.
 *
 * C11, opaque registry, no external dependencies.
 */
#include "wubu_kernel_xchg.h"

#include <stdio.h>
#include <string.h>

/* A registered kernel.  The ops table is owned by the kernel (it is static
 * const data in the backend); the registry only holds the pointer. */
static const wubu_kernel_ops_t *g_kernels[WUBU_KERNEL_MAX];
static int g_n_kernels;
static int g_active = -1;              /* index, -1 = none selected */
static int g_our_schema = 1;           /* our /n/trace schema major */

/* Swap history, newest first.  This is the interchange's observable network
 * log: an operator can see that the box moved linux -> nt -> macos without
 * trusting anyone's recollection. */
#define KHIST 8
static char g_hist[KHIST][2 * WUBU_KERNEL_NAME_MAX + 4];
static int  g_n_hist;

static int find_kernel(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < g_n_kernels; i++)
        if (g_kernels[i] && strcmp(g_kernels[i]->name, name) == 0)
            return i;
    return -1;
}

static void hist_add(const char *from, const char *to)
{
    if (g_n_hist < KHIST) g_n_hist++;
    for (int i = g_n_hist - 1; i > 0; i--)
        memcpy(g_hist[i], g_hist[i - 1], sizeof(g_hist[0]));
    snprintf(g_hist[0], sizeof(g_hist[0]), "%s -> %s",
             (from && from[0]) ? from : "(none)",
             (to && to[0]) ? to : "(none)");
}

/* Routing order: the ACTIVE kernel first, then the rest in registration
 * order.  Every kernel gets asked, so a foreign kernel's unimplemented call
 * can still be answered by one that knows it -- that IS the interchange. */
static int route_order(int k)
{
    if (g_active < 0) return k;
    if (k == 0) return g_active;
    int i = k - 1;
    if (i >= g_active) i++;
    return i;
}

/* -- registry ------------------------------------------------------- */

int wubu_kernel_register(const wubu_kernel_ops_t *ops)
{
    if (!ops || !ops->name || !ops->name[0]) return -1;
    if (strlen(ops->name) >= WUBU_KERNEL_NAME_MAX) return -1;
    if (find_kernel(ops->name) >= 0) return -1;      /* duplicate name */
    if (g_n_kernels >= WUBU_KERNEL_MAX) return -1;
    /* A kernel that can neither dispatch, probe nor exec is inert: registering
     * it would put a name in the network that can never answer anything. */
    if (!ops->try_dispatch && !ops->probe && !ops->exec) return -1;
    g_kernels[g_n_kernels++] = ops;
    return 0;
}

int wubu_kernel_unregister(const char *name)
{
    int i = find_kernel(name);
    if (i < 0) return -1;
    if (i == g_active) return -1;                    /* swap first */
    for (int j = i; j + 1 < g_n_kernels; j++)
        g_kernels[j] = g_kernels[j + 1];
    g_n_kernels--;
    if (g_active > i) g_active--;                    /* indices shift */
    return 0;
}

int wubu_kernel_count(void)
{
    return g_n_kernels;
}

const char *wubu_kernel_name(int i)
{
    if (i < 0 || i >= g_n_kernels || !g_kernels[i]) return NULL;
    return g_kernels[i]->name;
}

const wubu_kernel_ops_t *wubu_kernel_ops(const char *name)
{
    int i = find_kernel(name);
    return (i < 0) ? NULL : g_kernels[i];
}

/* -- contract ------------------------------------------------------- */

void wubu_kernel_set_our_schema(int major)
{
    g_our_schema = major;
}

int wubu_kernel_our_schema(void)
{
    return g_our_schema;
}

int wubu_kernel_schema_ok(const char *name, int our_schema)
{
    const wubu_kernel_ops_t *op = wubu_kernel_ops(name);
    if (!op) return 0;
    if (op->schema_version == 0) return 1;           /* emits nothing */
    return op->schema_version == our_schema;
}

/* -- selection / swap ----------------------------------------------- */

int wubu_kernel_select(const char *name)
{
    int i = find_kernel(name);
    if (i < 0) return -1;
    if (!wubu_kernel_schema_ok(name, g_our_schema)) return -1;   /* fail closed */
    if (i == g_active) return 0;             /* no change: no log entry */
    /* Every change of the active kernel is a network event.  select() and
     * swap() both funnel through here, so the log cannot disagree with the
     * state it describes. */
    hist_add((g_active >= 0) ? g_kernels[g_active]->name : NULL, name);
    g_active = i;
    return 0;
}

const char *wubu_kernel_active(void)
{
    return (g_active >= 0) ? g_kernels[g_active]->name : NULL;
}

int wubu_kernel_swap(const char *name, char *prev, size_t prev_n)
{
    int old = g_active;
    if (wubu_kernel_select(name) != 0) return -1;   /* active unchanged */
    if (prev && prev_n) {
        snprintf(prev, prev_n, "%s",
                 (old >= 0) ? g_kernels[old]->name : "(none)");
    }
    return 0;
}

/* -- routing -------------------------------------------------------- */

int64_t wubu_kernel_interchange(uint64_t num, uint64_t *regs,
                                const char **handled_by)
{
    if (handled_by) *handled_by = NULL;
    for (int k = 0; k < g_n_kernels; k++) {
        const wubu_kernel_ops_t *op = g_kernels[route_order(k)];
        if (!op || !op->try_dispatch) continue;
        int64_t out = 0;
        if (op->try_dispatch(num, regs, &out)) {
            if (handled_by) *handled_by = op->name;
            return out;
        }
    }
    return WUBU_KERNEL_ENOSYS;
}

int64_t wubu_kernel_dispatch_active(uint64_t num, uint64_t *regs)
{
    if (g_active < 0) return WUBU_KERNEL_ENOSYS;
    const wubu_kernel_ops_t *op = g_kernels[g_active];
    if (!op || !op->try_dispatch) return WUBU_KERNEL_ENOSYS;
    int64_t out = 0;
    if (!op->try_dispatch(num, regs, &out)) return WUBU_KERNEL_ENOSYS;
    return out;
}

const char *wubu_kernel_who_execs(const char *path, const uint8_t *hdr, size_t n)
{
    for (int k = 0; k < g_n_kernels; k++) {
        const wubu_kernel_ops_t *op = g_kernels[route_order(k)];
        if (op && op->probe && op->probe(path, hdr, n)) return op->name;
    }
    return NULL;
}

/* -- observability -------------------------------------------------- */

int wubu_kernel_xchg_dump(char *buf, size_t n)
{
    if (!buf || n == 0) return -1;
    size_t used = 0;
    int rc = snprintf(buf, n, "kernels %d active %s\n", g_n_kernels,
                      (g_active >= 0) ? g_kernels[g_active]->name : "(none)");
    if (rc < 0 || (size_t)rc >= n) return -1;
    used = (size_t)rc;
    for (int i = 0; i < g_n_kernels; i++) {
        const wubu_kernel_ops_t *op = g_kernels[i];
        rc = snprintf(buf + used, n - used,
                      "kernel %s abi=%d schema=%d fmt=%s%s\n",
                      op->name, op->abi_version, op->schema_version,
                      op->formats ? op->formats : "-",
                      (i == g_active) ? " active=1" : "");
        if (rc < 0 || (size_t)rc >= n - used) return -1;   /* truncated */
        used += (size_t)rc;
    }
    return 0;
}

int wubu_kernel_history(char *buf, size_t n)
{
    if (!buf || n == 0) return -1;
    buf[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < g_n_hist; i++) {
        int rc = snprintf(buf + used, n - used, "%s\n", g_hist[i]);
        if (rc < 0 || (size_t)rc >= n - used) return -1;
        used += (size_t)rc;
    }
    return 0;
}
