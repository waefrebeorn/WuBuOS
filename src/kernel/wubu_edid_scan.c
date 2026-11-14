/*
 * wubu_edid_scan.c -- see wubu_edid_scan.h.  Directory scan + per-connector parse.
 */
#include "wubu_edid_scan.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A directory is a CONNECTOR when it carries any of the kernel's per-connector files.
 * Keying off the name ("cardN-*") breaks as conventions change; keying off the files
 * the kernel actually provides does not. */
static bool is_connector_dir(const char *root, const char *entry)
{
    static const char *const marks[] = { "status", "enabled", "modes", "edid" };
    char p[600];
    for (size_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        snprintf(p, sizeof(p), "%s/%s/%s", root, entry, marks[i]);
        FILE *fp = fopen(p, "r");
        if (fp) { fclose(fp); return true; }
    }
    return false;
}

/* Strip a leading "<card>-" prefix to get the connector the user recognises.
 * Returns the input unchanged when there is no such prefix, so unusual names still
 * work rather than coming back empty. */
static const char *strip_card_prefix(const char *dir_name, char *out, size_t outsz)
{
    const char *dash = strchr(dir_name, '-');
    if (!dash || dash == dir_name) {
        snprintf(out, outsz, "%.*s", (int)(outsz - 1), dir_name);
        return out;
    }
    snprintf(out, outsz, "%s", dash + 1);
    return out;
}

/* status/enabled hold a single word ("connected"/"disconnected",
 * "enabled"/"disabled"); read only the first token. */
static bool read_flag(const char *path, const char *on_word)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char buf[32] = {0};
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return false; }
    fclose(fp);

    return strncmp(buf, on_word, strlen(on_word)) == 0;
}

int wubu_edid_scan(const char *drm_root,
                   wubu_connector_edid_t *out, int cap)
{
    if (!drm_root || !*drm_root || !out || cap <= 0) return -1;

    DIR *d = opendir(drm_root);
    if (!d) return -1;                       /* headless: honest failure */

    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < cap) {
        if (e->d_name[0] == '.') continue;
        if (!is_connector_dir(drm_root, e->d_name)) continue;

        wubu_connector_edid_t *c = &out[n];
        memset(c, 0, sizeof(*c));
        /* Bounded copy: d_name can be longer than our field on unusual kernels,
         * and a silent mid-name cut would make two ports look like one. */
        snprintf(c->dir, sizeof(c->dir), "%.*s",
                 (int)(sizeof(c->dir) - 1), e->d_name);
        strip_card_prefix(e->d_name, c->name, sizeof(c->name));

        char path[600];

        snprintf(path, sizeof(path), "%s/%s/status", drm_root, e->d_name);
        c->connected = read_flag(path, "connected");

        snprintf(path, sizeof(path), "%s/%s/enabled", drm_root, e->d_name);
        c->enabled = read_flag(path, "enabled");

        /* Missing, empty and unparseable EDIDs all leave the connector REPORTED --
         * "port present, nothing readable from it" is different from "no port", and
         * dropping it would hide a real monitor behind a parser or bus problem. */
        snprintf(path, sizeof(path), "%s/%s/edid", drm_root, e->d_name);
        FILE *fp = fopen(path, "rb");
        if (fp) {
            size_t got = fread(c->raw, 1, WUBU_EDID_RAW_MAX, fp);
            fclose(fp);
            c->raw_len = got;
            if (got > 0 && wubu_edid_parse(c->raw, got, &c->edid) == 0)
                c->parsed = true;
        }

        n++;
    }

    closedir(d);
    return n;
}
