/* wubu_dxvk_conf.c -- Shared DXVK config-file core.
 *
 * Implements the config read/write + line set/replace/remove + UI parse/build
 * logic that was previously duplicated inline in both src/runtime/wubu_proton.c
 * (runtime VSL-proton) and src/gui/wubu_proton_dxvk.c (GUI desktop-proton). The
 * two subsystems keep their own prefix-path resolution and (GUI) prefix-state
 * glue; this module owns the file manipulation, exactly once.
 *
 * C11, minimal includes. Path-agnostic and state-free.
 */
#include "wubu_dxvk_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dxvk_conf_write(const char *path, const char *content) {
    if (!path || !content) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

int dxvk_conf_read(const char *path, char *out, size_t size) {
    if (!path || !out || size == 0) return -1;
    FILE *f = fopen(path, "r");
    if (!f) {
        out[0] = '\0';
        return -1;
    }
    size_t n = fread(out, 1, size - 1, f);
    out[n] = '\0';
    fclose(f);
    return 0;
}

/* Locate the line whose first token is `key` (followed by optional spaces then
 * '='). Returns 1 and fills [line_start, line_end) (line_end is the position of
 * the trailing '\n' or end-of-string) or 0 if not found. */
static int find_key_line(const char *buf, const char *key,
                         size_t *line_start, size_t *line_end) {
    size_t klen = strlen(key);
    const char *p = buf;
    while ((p = strstr(p, key)) != NULL) {
        const char *ls = p;
        while (ls > buf && ls[-1] != '\n') ls--;
        if (ls == buf || ls[-1] == ' ' || ls[-1] == '\t' || ls[-1] == '\n') {
            const char *q = p + klen;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                const char *le = p;
                while (*le && *le != '\n') le++;
                *line_start = (size_t)(ls - buf);
                *line_end   = (size_t)(le - buf);
                return 1;
            }
        }
        p += klen;
    }
    return 0;
}

int dxvk_conf_set_key(char *buf, size_t bufsz, const char *key, const char *value) {
    if (!buf || !key || bufsz == 0) return -1;
    size_t ls, le;
    if (find_key_line(buf, key, &ls, &le)) {
        char tmp[8192];
        size_t pre = ls;                 /* bytes before the target line */
        const char *tail = buf + le;     /* trailing '\n' + rest (or "\0") */
        int n;
        if (value) {
            n = snprintf(tmp, sizeof(tmp), "%.*s%s = %s%s",
                         (int)pre, buf, key, value, tail);
        } else {
            /* drop the line and its trailing newline */
            const char *t = tail;
            if (*t == '\n') t++;
            n = snprintf(tmp, sizeof(tmp), "%.*s%s", (int)pre, buf, t);
        }
        if (n < 0 || (size_t)n >= sizeof(tmp) || (size_t)n + 1 > bufsz) return -1;
        memcpy(buf, tmp, (size_t)n + 1);
        return 0;
    }
    /* key absent */
    if (!value) return 0;                /* nothing to remove */
    size_t len = strlen(buf);
    int sep = (len > 0 && buf[len - 1] != '\n') ? 1 : 0;
    int need = (int)(len + (size_t)sep + strlen(key) + 3 + strlen(value) + 2);
    if ((size_t)need > bufsz) return -1;
    snprintf(buf + len, bufsz - len, "%s%s = %s\n", sep ? "\n" : "", key, value);
    return 0;
}

int dxvk_conf_get_key(const char *buf, const char *key, char *out, size_t size) {
    if (!buf || !key || !out || size == 0) return -1;
    size_t ls, le;
    if (!find_key_line(buf, key, &ls, &le)) return -1;
    const char *p = buf + ls + strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '=') p++;
    while (*p == ' ' || *p == '\t') p++;
    const char *e = p;
    while (*e && *e != '\n' && *e != '\r') e++;
    size_t vlen = (size_t)(e - p);
    if (vlen >= size) vlen = size - 1;
    memcpy(out, p, vlen);
    out[vlen] = '\0';
    return 0;
}

int dxvk_conf_parse_ui(const char *config, DxvkConfigUI *ui) {
    if (!config || !ui) return -1;
    memset(ui, 0, sizeof(*ui));
    char v[256];
    if (dxvk_conf_get_key(config, "hud", v, sizeof(v)) == 0) {
        ui->dxvk_hud_enabled = true;
        strncpy(ui->dxvk_hud_options, v, sizeof(ui->dxvk_hud_options) - 1);
    }
    if (dxvk_conf_get_key(config, "async", v, sizeof(v)) == 0)
        ui->dxvk_async = (strstr(v, "true") != NULL);
    if (dxvk_conf_get_key(config, "nvapiHack", v, sizeof(v)) == 0)
        ui->dxvk_nvapi_hack = (strstr(v, "true") != NULL);
    if (dxvk_conf_get_key(config, "presentMode", v, sizeof(v)) == 0)
        ui->dxvk_present_mode_mailbox = (strstr(v, "1") != NULL);
    if (dxvk_conf_get_key(config, "maxDeviceMemory", v, sizeof(v)) == 0)
        ui->dxvk_max_device_memory = atoi(v);
    if (dxvk_conf_get_key(config, "maxSharedMemory", v, sizeof(v)) == 0)
        ui->dxvk_max_shared_memory = atoi(v);
    if (dxvk_conf_get_key(config, "d3d10", v, sizeof(v)) == 0)
        ui->dxvk_d3d10 = (strstr(v, "true") != NULL);
    if (dxvk_conf_get_key(config, "d3d10_1", v, sizeof(v)) == 0)
        ui->dxvk_d3d10_1 = (strstr(v, "true") != NULL);
    return 0;
}

int dxvk_conf_build_ui(const DxvkConfigUI *ui, char *buf, size_t bufsz) {
    if (!ui || !buf || bufsz == 0) return -1;
    int pos = snprintf(buf, bufsz, "[dxvk]\n");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "hud = %s\n",
                    ui->dxvk_hud_enabled
                        ? (ui->dxvk_hud_options[0] ? ui->dxvk_hud_options : "fps")
                        : "");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "async = %s\n",
                    ui->dxvk_async ? "true" : "false");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "nvapiHack = %s\n",
                    ui->dxvk_nvapi_hack ? "true" : "false");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "presentMode = %d\n",
                    ui->dxvk_present_mode_mailbox ? 1 : 0);
    if (ui->dxvk_max_device_memory > 0)
        pos += snprintf(buf + pos, bufsz - (size_t)pos, "maxDeviceMemory = %d\n",
                        ui->dxvk_max_device_memory);
    if (ui->dxvk_max_shared_memory > 0)
        pos += snprintf(buf + pos, bufsz - (size_t)pos, "maxSharedMemory = %d\n",
                        ui->dxvk_max_shared_memory);
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "d3d10 = %s\n",
                    ui->dxvk_d3d10 ? "true" : "false");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "d3d10_1 = %s\n",
                    ui->dxvk_d3d10_1 ? "true" : "false");
    pos += snprintf(buf + pos, bufsz - (size_t)pos, "stateCache = %s\n",
                    ui->dxvk_state_cache ? "true" : "false");
    return 0;
}
