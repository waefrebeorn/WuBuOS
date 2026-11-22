/*
 * wubu_console_drv.c -- the ring-0 `drv` command.
 *
 * One concern: render the ONE driver registry to the live console. Kept in
 * its own TU so a hosted gate can exercise the real command (with a klog
 * stub) without linking the whole command collection.
 *
 * C11, freestanding.
 */
#include "wubu_drv.h"
#include "klog.h"

int cmd_drv(void)
{
    char line[128];
    if (wubu_drv_registry_line(line, sizeof(line)) > 0)
        klog_printf("-- driver registry: %s --\n", line);
    char summary[8192];
    if (wubu_drv_summary(summary, sizeof(summary)) > 0)
        klog_printf("%s", summary);
    return 0;
}
