/*
 * wubu_edid_kms.c -- see wubu_edid_kms.h.  Enumeration only; never DRM master.
 */
#include "wubu_edid_kms.h"

#include <dirent.h>
#include <fcntl.h>      /* open, O_CLOEXEC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     /* close */

#include <xf86drm.h>
#include <xf86drmMode.h>

/* Read one connector's EDID property blob, if it has one.  Returns bytes stored. */
static size_t read_connector_edid(int fd, drmModeConnector *conn,
                                  uint8_t *buf, size_t cap)
{
    for (int i = 0; i < conn->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, conn->props[i]);
        if (!prop) continue;

        bool is_edid = (strcmp(prop->name, "EDID") == 0);
        uint64_t value = conn->prop_values ? conn->prop_values[i] : 0;
        drmModeFreeProperty(prop);

        if (!is_edid || value == 0) continue;

        drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(fd, (uint32_t)value);
        if (!blob) continue;

        size_t got = blob->length > cap ? cap : (size_t)blob->length;
        memcpy(buf, blob->data, got);
        drmModeFreePropertyBlob(blob);
        return got;
    }
    return 0;
}

/* Enumerate one card.  Returns connectors written starting at out[*idx]. */
static int scan_card(const char *card_path, const char *card_name,
                     wubu_connector_edid_t *out, int cap, int idx)
{
    int fd = open(card_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return idx;                  /* unreadable card: skip, not fatal */

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) { close(fd); return idx; }

    for (int i = 0; i < res->count_connectors && idx < cap; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        bool connected = (conn->connection == DRM_MODE_CONNECTED);

        /* Report every connector we can see, connected or not: a port that exists but
         * has nothing plugged in is still information, and dropping it was the bug
         * that made the sysfs scanner miss three of four ports on real hardware. */
        wubu_connector_edid_t *c = &out[idx];
        memset(c, 0, sizeof(*c));

        snprintf(c->dir, sizeof(c->dir), "%.*s",
                 (int)(sizeof(c->dir) - 1), card_name);

        /* libdrm exposes the connector TYPE and NUMBER; combine them into the name
         * users recognise ("eDP-1"), matching what xrandr calls the same port. */
        char type[32];
        switch (conn->connector_type) {
            case DRM_MODE_CONNECTOR_eDP:     snprintf(type, sizeof(type), "eDP"); break;
            case DRM_MODE_CONNECTOR_DSI:     snprintf(type, sizeof(type), "DSI"); break;
            case DRM_MODE_CONNECTOR_LVDS:    snprintf(type, sizeof(type), "LVDS"); break;
            case DRM_MODE_CONNECTOR_HDMIA:   snprintf(type, sizeof(type), "HDMI-A"); break;
            case DRM_MODE_CONNECTOR_HDMIB:   snprintf(type, sizeof(type), "HDMI-B"); break;
            case DRM_MODE_CONNECTOR_DisplayPort: snprintf(type, sizeof(type), "DP"); break;
            case DRM_MODE_CONNECTOR_VGA:     snprintf(type, sizeof(type), "VGA"); break;
            case DRM_MODE_CONNECTOR_DVII:    snprintf(type, sizeof(type), "DVI-I"); break;
            case DRM_MODE_CONNECTOR_DVID:    snprintf(type, sizeof(type), "DVI-D"); break;
            case DRM_MODE_CONNECTOR_DVIA:    snprintf(type, sizeof(type), "DVI-A"); break;
            case DRM_MODE_CONNECTOR_Composite: snprintf(type, sizeof(type), "CVBS"); break;
            default:                         snprintf(type, sizeof(type), "UNK"); break;
        }
        snprintf(c->name, sizeof(c->name), "%s-%d", type, conn->connector_type_id);

        c->connected = connected;
        c->enabled   = connected;            /* no separate enable flag at this layer */

        size_t got = read_connector_edid(fd, conn, c->raw, WUBU_EDID_RAW_MAX);
        c->raw_len = got;
        if (got > 0 && wubu_edid_parse(c->raw, got, &c->edid) == 0)
            c->parsed = true;

        idx++;
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    close(fd);
    return idx;
}

int wubu_edid_kms_scan(wubu_connector_edid_t *out, int cap)
{
    if (!out || cap <= 0) return -1;

    memset(out, 0, (size_t)cap * sizeof(*out));

    DIR *d = opendir("/dev/dri");
    if (!d) return -1;                       /* no DRM at all: honest failure */

    int n = 0, opened = 0;
    struct dirent *e;

    while ((e = readdir(d)) != NULL && n < cap) {
        /* Only cardN devices; render nodes do not carry connectors. */
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        if (strchr(e->d_name, '-') != NULL) continue;

        char path[256], name[64];
        snprintf(path, sizeof(path), "/dev/dri/%.*s",
                 (int)(sizeof(path) - 10), e->d_name);
        snprintf(name, sizeof(name), "%.*s",
                 (int)(sizeof(name) - 1), e->d_name);

        int before = n;
        n = scan_card(path, name, out, cap, n);
        if (n > before) opened++;
    }

    closedir(d);
    return opened > 0 ? n : -1;
}
