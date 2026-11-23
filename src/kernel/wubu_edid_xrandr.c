/*
 * wubu_edid_xrandr.c -- see wubu_edid_xrandr.h.
 *
 * Parssing rules taken from the REAL output format rather than invented:
 *   "Screen 0: minimum ..."                        header, ignore
 *   "eDP-1 connected primary (normal ...)"         new connector; name = first token
 *   "\tEDID: "                                     an EDID hex block follows
 *   "\t\t00ffffffffffff00...<32 hex chars>"       16 EDID bytes per line
 *   "\t1920x1080  144.00 +  60.02"                 mode line, never hex
 *   "\t_MUFFIN_PRESENTATION_OUTPUT: 0 "            other property, never hex
 *
 * THE TWO TRAPS THIS PARSER HAS TO AVOID, both hit while writing it:
 *
 *   1. Consuming the first hex line before collecting it drops the first 16 bytes of
 *      every blob, so the checksum can never validate and nothing parses. Collection
 *      must begin WITH the line that triggered it.
 *   2. Reading ahead to find the end of the block swallows the line that terminates it.
 *      That line may be the NEXT CONNECTOR, and losing it silently hides a monitor.
 *      So the parser buffers candidate lines and only commits them once the block
 *      actually ends -- a line that turns out not to belong is processed normally.
 *
 * EDID lines are identified by TWO leading tabs plus hex-and-whitespace only. Mode
 * lines and property values have one leading tab, so they cannot be mistaken for data.
 */
#define _POSIX_C_SOURCE 200809L  /* popen/pclose */
#include "wubu_edid_xrandr.h"

#include <stdlib.h>
#include <string.h>

#define XRANDR_MAX_PENDING 16          /* 128 bytes = 8 lines; allow extension blocks */
#define XRANDR_LINE_MAX    512

typedef struct {
    char   lines[XRANDR_MAX_PENDING][XRANDR_LINE_MAX];
    int    count;
    size_t bytes;                      /* decoded bytes held */
    uint8_t raw[WUBU_EDID_RAW_MAX];
} edid_pending_t;

static int hex_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode hex pairs from one line into the pending buffer. */
static void decode_hex_line(edid_pending_t *p, const char *line)
{
    int hi = -1;
    for (const char *c = line; *c; c++) {
        int v = hex_val((unsigned char)*c);
        if (v < 0) continue;
        if (hi < 0) { hi = v; continue; }
        if (p->bytes < WUBU_EDID_RAW_MAX)
            p->raw[p->bytes++] = (uint8_t)((hi << 4) | v);
        hi = -1;
    }
}

/* An EDID hex line: two leading tabs (property VALUES indent further than keys), and
 * after that only hex digits and whitespace. */
static bool is_edid_line(const char *line)
{
    if (!(line[0] == '\t' && line[1] == '\t')) return false;

    int any = 0;
    for (const char *c = line + 2; *c; c++) {
        if (*c == '\n' || *c == '\r' || *c == ' ' || *c == '\t') continue;
        if (hex_val((unsigned char)*c) < 0) return false;
        any = 1;
    }
    return any;
}

int wubu_edid_xrandr_parse(FILE *fp, wubu_connector_edid_t *out, int cap)
{
    if (!fp || !out || cap <= 0) return -1;

    memset(out, 0, (size_t)cap * sizeof(*out));
    int n = 0;

    edid_pending_t pending;
    memset(&pending, 0, sizeof(pending));
    int in_block = 0;                 /* true between "EDID:" and its first non-hex line */

    char line[XRANDR_LINE_MAX];
    while (fgets(line, sizeof(line), fp)) {
        bool indented = (line[0] == ' ' || line[0] == '\t');

        /* --- inside an EDID block: hex lines accumulate, anything else ends it -- */
        if (in_block) {
            if (is_edid_line(line)) {
                if (pending.count < XRANDR_MAX_PENDING) {
                    snprintf(pending.lines[pending.count],
                             XRANDR_LINE_MAX, "%s", line);
                    pending.count++;
                    decode_hex_line(&pending, line);
                }
                continue;
            }
            /* Commit to the connector this block belongs to, then process THIS line
             * normally -- it may be the next connector, and dropping it would hide a
             * real monitor. */
            if (n > 0 && pending.bytes > 0) {
                wubu_connector_edid_t *c = &out[n - 1];
                memcpy(c->raw, pending.raw,
                       pending.bytes < WUBU_EDID_RAW_MAX ? pending.bytes
                                                        : WUBU_EDID_RAW_MAX);
                c->raw_len = pending.bytes;
                if (wubu_edid_parse(c->raw, pending.bytes, &c->edid) == 0)
                    c->parsed = true;
            }
            memset(&pending, 0, sizeof(pending));
            in_block = 0;
            /* fall through */
        }

        /* --- "EDID:" property opens a block ---------------------------------- */
        if (indented && strstr(line, "EDID:")) {
            in_block = 1;
            continue;
        }

        /* --- output line: starts in column 0 -------------------------------- */
        if (!indented) {
            int connected    = strstr(line, " connected ")  != NULL ||
                               strstr(line, " connected\n") != NULL;
            int disconnected = strstr(line, " disconnected ") != NULL ||
                               strstr(line, " disconnected\n") != NULL;
            if (!connected && !disconnected) continue;      /* Screen header etc. */

            if (n >= cap) break;
            wubu_connector_edid_t *c = &out[n];
            memset(c, 0, sizeof(*c));

            char name[WUBU_EDID_NAME] = {0};
            int k = 0;
            for (const char *p = line; *p && *p != ' ' && k < (int)sizeof(name) - 1; p++)
                name[k++] = *p;

            snprintf(c->dir, sizeof(c->dir), "%.*s",
                     (int)(sizeof(c->dir) - 1), name);
            snprintf(c->name, sizeof(c->name), "%.*s",
                     (int)(sizeof(c->name) - 1), name);

            c->connected = connected && !disconnected;
            c->enabled   = c->connected;     /* xrandr lists modes only for live outputs */
            n++;
            continue;
        }
    }

    /* Stream ended mid-block: commit rather than discarding a complete-looking EDID
     * whose terminating property never arrived. */
    if (n > 0 && pending.bytes > 0) {
        wubu_connector_edid_t *c = &out[n - 1];
        memcpy(c->raw, pending.raw,
               pending.bytes < WUBU_EDID_RAW_MAX ? pending.bytes : WUBU_EDID_RAW_MAX);
        c->raw_len = pending.bytes;
        if (wubu_edid_parse(c->raw, pending.bytes, &c->edid) == 0)
            c->parsed = true;
    }

    return n;
}

int wubu_edid_xrandr_scan(wubu_connector_edid_t *out, int cap)
{
    FILE *fp = popen("xrandr -q --props 2>/dev/null", "r");
    if (!fp) return -1;

    int n = wubu_edid_xrandr_parse(fp, out, cap);
    pclose(fp);
    return n;
}
