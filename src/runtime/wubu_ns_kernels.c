/*
 * wubu_ns_kernels.c -- the /n/kernels control subtree.
 *
 * See wubu_ns_kernels.h.  The subtree is rendered from the LIVE interchange
 * state on every refresh, so the files can never go stale relative to the
 * network they describe.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_kernels.h"
#include "wubu_kernel_xchg.h"

#include <stdio.h>

int wubu_ns_kernels_refresh(void)
{
    char sub[128], buf[2048];
    const char *act = wubu_kernel_active();

    snprintf(sub, sizeof(sub), "kernels/active");
    if (ns_write(sub, (act && act[0]) ? act : "(none)") != 0) return -1;

    snprintf(sub, sizeof(sub), "kernels/list");
    if (wubu_kernel_xchg_dump(buf, sizeof(buf)) != 0) return -1;
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "kernels/history");
    if (wubu_kernel_history(buf, sizeof(buf)) != 0) return -1;
    return ns_write(sub, buf);
}

int wubu_ns_publish_kernels(void)
{
    if (ns_mkdir("kernels") != 0) return -1;
    return wubu_ns_kernels_refresh();
}

int wubu_ns_kernels_select(const char *name)
{
    if (!name || !name[0]) return -1;
    return wubu_kernel_swap(name, NULL, 0);
}
