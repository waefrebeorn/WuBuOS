/*
 * wubu_edid_dump.c -- print what each connector reports about its monitor.
 *
 * WHY THIS EXISTS
 *   wubu_edid_scan and wubu_edid_parse were correct but had no caller: nothing in the
 *   system consumed them.  This tool closes that gap on the HOSTED path -- it scans the
 *   kernel's per-connector EDID blobs and prints identity, connection state and the
 *   preferred timing, so a human can see what the display stack believes.
 *
 * It is deliberately a tool and not library code: parsing and scanning stay pure and
 * reusable, while this file only formats.  Run it before suspecting the parser.
 *
 * USAGE
 *   wubu-edid-dump [drm_root]     default root: /sys/class/drm
 *
 * EXIT STATUS
 *   0 = at least one connector found
 *   1 = no connectors / unreadable root
 *   2 = usage error
 */
#include <stdio.h>
#include <string.h>

#include "wubu_edid_scan.h"

int main(int argc, char **argv)
{
    const char *root = (argc > 1) ? argv[1] : "/sys/class/drm";

    wubu_connector_edid_t out[WUBU_EDID_MAX_CONNECTORS];
    memset(out, 0, sizeof(out));

    int n = wubu_edid_scan(root, out, WUBU_EDID_MAX_CONNECTORS);
    if (n <= 0) {
        fprintf(stderr, "wubu-edid-dump: no connectors found under %s\n", root);
        return 1;
    }

    printf("WuBuOS connector EDID report (%s)\n", root);
    printf("%-16s %-11s %-7s %-6s %-8s %s\n",
           "CONNECTOR", "STATUS", "KMS-EN", "EDID", "SIZE", "IDENTITY");
    printf("--------------------------------------------------------------------------------\n");

    int parsed = 0, connected = 0;

    for (int i = 0; i < n; i++) {
        const wubu_connector_edid_t *c = &out[i];

        if (c->connected) connected++;
        if (c->parsed)    parsed++;

        char size[16] = "-";
        char identity[128] = "";

        if (c->parsed) {
            snprintf(size, sizeof(size), "%dx%d", c->edid.pref_w, c->edid.pref_h);
            if (c->edid.name[0])
                snprintf(identity, sizeof(identity), "%s %s %dHz",
                         c->edid.manuf, c->edid.name, c->edid.pref_refresh_mhz);
            else
                snprintf(identity, sizeof(identity), "%s %dx%d@%dHz",
                         c->edid.manuf, c->edid.pref_w, c->edid.pref_h,
                         c->edid.pref_refresh_mhz);
            if (c->edid.mm_w && c->edid.mm_h) {
                char mm[48];
                snprintf(mm, sizeof(mm), " (%dx%dmm)", c->edid.mm_w, c->edid.mm_h);
                strncat(identity, mm, sizeof(identity) - strlen(identity) - 1);
            }
        } else if (c->raw_len > 0) {
            snprintf(identity, sizeof(identity),
                     "unparseable blob (%zu bytes)", c->raw_len);
        }

        printf("%-16s %-11s %-7s %-6s %-8s %s\n",
               c->name,
               c->connected ? "connected" : "disconnected",
               c->enabled   ? "yes" : "no",
               c->parsed    ? "yes" : (c->raw_len ? "bad" : "none"),
               size, identity);
    }

    printf("--------------------------------------------------------------------------------\n");
    printf("%d connector(s): %d connected, %d identified from EDID\n",
           n, connected, parsed);

    /* KMS-EN is the kernel's userspace-enable flag, NOT "is this panel showing
     * anything": when another session holds DRM master it reads disabled even on a
     * fully live display (that is exactly what this machine reports for its own
     * panel).  Printed because the value is real, labelled because it is easy to
     * misread -- STATUS is the field that answers "is something plugged in". */
    printf("KMS-EN reflects the kernel enable flag; use STATUS for presence.\n");

    return 0;
}
