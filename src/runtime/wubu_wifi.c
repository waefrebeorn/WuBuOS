/*
 * wubu_wifi.c -- Wi-Fi association, spoken rather than shelled.
 *
 * THE PROBLEM THIS SOLVES
 *   The tree had WiFi *regulatory* routing (channels, DFS, TX power) and no way
 *   to ASSOCIATE: a laptop could not get online, which makes eleven online
 *   workflows in docs/compendium/04-roadmap/daily-driver-benchmarks.md moot.
 *
 * THE APPROACH
 *   No shelling out. wpa_supplicant exposes a line protocol on a Unix socket
 *   (`/run/wpa_supplicant/<ifname>`, the wpa_ctrl interface), and this module is
 *   a client for it: send a command plus "\n", read until the reply ends. The
 *   parser and the command sequence are pure functions over strings, so the
 *   whole workflow is gate-able without hardware, and the socket is one small
 *   transport behind a seam.
 *
 * WHAT IT REFUSES TO DO
 *   * It never builds a shell command -- a socket write cannot become a shell.
 *   * It refuses an SSID or passphrase that cannot be quoted safely instead of
 *     escaping it into something the supplicant would read differently: a
 *     passphrase containing a quote is an ERROR here, not an injection.
 *   * "Connected" means wpa_state=COMPLETED from STATUS. OK from ENABLE_NETWORK
 *     means the command was accepted, not that we are on the network.
 *
 * C11, opaque state, minimal includes.
 */

#include "wubu_wifi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define WIFI_SOCK_DIR   "/run/wpa_supplicant"
#define WIFI_REPLY_MAX  4096

struct WubuWifi {
    char ifname[32];
    int  fd;
    /* Test seam: when set, every command goes here instead of the socket. */
    wubu_wifi_xfer_fn xfer;
    void *user;
};

static struct WubuWifi g_wifi;

/* -- Transport ---------------------------------------------------- */

static int sock_xfer(void *user, const char *cmd, char *reply, size_t n) {
    struct WubuWifi *w = user;
    if (w->fd < 0) return -1;
    size_t len = strlen(cmd);
    if (write(w->fd, cmd, len) != (ssize_t)len) return -1;
    if (write(w->fd, "\n", 1) != 1) return -1;
    /* The control socket answers in one read for these commands and prefixes an
     * unsolicited "<prio>" banner; strip it. */
    ssize_t got = read(w->fd, reply, n - 1);
    if (got <= 0) return -1;
    reply[got] = '\0';
    char *banner = strchr(reply, '>');
    if (banner && banner == reply + 1) memmove(reply, banner + 1, strlen(banner + 1) + 1);
    return 0;
}

static int wifi_cmd(const char *cmd, char *reply, size_t n) {
    if (!cmd) return -1;
    int rc = g_wifi.xfer ? g_wifi.xfer(g_wifi.user, cmd, reply, n)
                         : sock_xfer(&g_wifi, cmd, reply, n);
    if (rc != 0) return -1;
    /* FAIL is a refusal with an optional reason after it. */
    if (strncmp(reply, "FAIL", 4) == 0) return -1;
    return 0;
}

int wubu_wifi_open(const char *ifname) {
    if (!ifname || !*ifname || strlen(ifname) >= sizeof g_wifi.ifname) return -1;
    memset(&g_wifi, 0, sizeof g_wifi);
    g_wifi.fd = -1;
    snprintf(g_wifi.ifname, sizeof g_wifi.ifname, "%s", ifname);

    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s/%s", WIFI_SOCK_DIR, ifname);

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) { close(fd); return -1; }
    g_wifi.fd = fd;
    return 0;
}

void wubu_wifi_close(void) {
    if (g_wifi.fd >= 0) close(g_wifi.fd);
    g_wifi.fd = -1;
}

int wubu_wifi_set_transport(wubu_wifi_xfer_fn fn, void *user) {
    g_wifi.xfer = fn;
    g_wifi.user = user;
    if (fn) g_wifi.fd = -1;
    return 0;
}

const char *wubu_wifi_iface(void) { return g_wifi.ifname[0] ? g_wifi.ifname : NULL; }

/* -- Parsing: SCAN_RESULTS ---------------------------------------- */
/* Line shape: "bssid / frequency / signal level / flags / ssid" ...
 * The ssid may contain spaces and slashes, so the ssid is everything after the
 * FOURTH separator, not the fifth field. */
int wubu_wifi_scan_parse(const char *raw, WubuWifiNet *out, int max) {
    if (!raw || !out || max <= 0) return -1;
    int n = 0;
    const char *p = raw;
    while (*p && n < max) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char line[512];
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len); line[len] = '\0';

        /* The supplicant separates the five fields with TABS; the frequency field
         * itself is "/2412/", so counting slashes is wrong -- and an SSID may
         * contain both slashes and spaces, so only the tab count bounds it. */
        char *f[5] = {0};
        int nf = 0;
        f[nf++] = line;
        for (char *q = line; *q && nf < 5; q++)
            if (*q == '\t') { *q = '\0'; f[nf++] = q + 1; }

        if (nf == 5 && f[4][0]) {
            char *ssid = f[4];
            while (*ssid == ' ') ssid++;
            if (*ssid) {
                WubuWifiNet *e = &out[n];
                memset(e, 0, sizeof *e);
                snprintf(e->ssid, sizeof e->ssid, "%s", ssid);
                int freq = 0, sig = 0;
                if (sscanf(f[1], "%*[^0-9]%d", &freq) == 1) e->freq = freq;
                if (sscanf(f[2], "%d", &sig) == 1) e->signal = sig;
                e->secure = (strstr(f[3], "WPA") || strstr(f[3], "RSN") ||
                             strstr(f[3], "WEP")) ? 1 : 0;
                n++;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return n;
}

int wubu_wifi_scan(WubuWifiNet *out, int max) {
    if (!out || max <= 0) return -1;
    char reply[WIFI_REPLY_MAX];
    if (wifi_cmd("SCAN", reply, sizeof reply) != 0) return -2;
    if (wifi_cmd("SCAN_RESULTS", reply, sizeof reply) != 0) return -2;
    return wubu_wifi_scan_parse(reply, out, max);
}

/* -- Joining ------------------------------------------------------ */

/* A supplicant parameter is quoted: an SSID or passphrase containing '"' or '\\'
 * cannot be represented, so it is refused rather than escaped into something
 * with different meaning. */
static int quotable(const char *s) {
    if (!s) return 0;
    for (const char *p = s; *p; p++)
        if (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r') return 0;
    return 1;
}

int wubu_wifi_join(const char *ssid, const char *psk) {
    if (!ssid || !*ssid || strlen(ssid) > 32) return -1;
    if (psk && !*psk) psk = NULL;
    if (psk && (strlen(psk) < 8 || strlen(psk) > 63)) return -1;   /* WPA2 rules */
    if (!quotable(ssid) || (psk && !quotable(psk))) return -1;

    char reply[WIFI_REPLY_MAX], cmd[256];
    if (wifi_cmd("ADD_NETWORK", reply, sizeof reply) != 0) return -2;
    int id = atoi(reply);
    if (id < 0) return -2;

    snprintf(cmd, sizeof cmd, "SET_NETWORK %d ssid \"%s\"", id, ssid);
    if (wifi_cmd(cmd, reply, sizeof reply) != 0) return -3;

    if (psk) {
        snprintf(cmd, sizeof cmd, "SET_NETWORK %d psk \"%s\"", id, psk);
    } else {
        /* open network: explicitly key_mgmt NONE, or the supplicant keeps the
         * previous security of a reused id and the join silently fails. */
        snprintf(cmd, sizeof cmd, "SET_NETWORK %d key_mgmt NONE", id);
    }
    if (wifi_cmd(cmd, reply, sizeof reply) != 0) return -3;

    snprintf(cmd, sizeof cmd, "ENABLE_NETWORK %d", id);
    if (wifi_cmd(cmd, reply, sizeof reply) != 0) return -4;
    (void)wifi_cmd("SAVE_CONFIG", reply, sizeof reply);   /* best effort */
    return id;
}

/* -- Status ------------------------------------------------------- */

WubuWifiState wubu_wifi_status(char *detail, size_t n) {
    char reply[WIFI_REPLY_MAX];
    if (detail && n) detail[0] = '\0';
    if (wifi_cmd("STATUS", reply, sizeof reply) != 0) return WUBU_WIFI_DOWN;

    const char *st = strstr(reply, "wpa_state=");
    if (detail && n) {
        const char *eol = strchr(reply, '\n');
        size_t len = eol ? (size_t)(eol - reply) : strlen(reply);
        if (len >= n) len = n - 1;
        memcpy(detail, reply, len); detail[len] = '\0';
    }
    if (!st) return WUBU_WIFI_DOWN;
    st += 10;
    if (strncmp(st, "COMPLETED", 9) == 0) return WUBU_WIFI_CONNECTED;
    if (strncmp(st, "SCANNING", 8) == 0) return WUBU_WIFI_SCANNING;
    if (strncmp(st, "ASSOCIATING", 11) == 0) return WUBU_WIFI_ASSOCIATING;
    if (strncmp(st, "4WAY_HANDSHAKE", 14) == 0) return WUBU_WIFI_AUTHENTICATING;
    return WUBU_WIFI_DISCONNECTED;
}

const char *wubu_wifi_state_name(WubuWifiState s) {
    switch (s) {
        case WUBU_WIFI_CONNECTED:      return "Connected";
        case WUBU_WIFI_SCANNING:       return "Scanning";
        case WUBU_WIFI_ASSOCIATING:    return "Associating";
        case WUBU_WIFI_AUTHENTICATING: return "Authenticating";
        case WUBU_WIFI_DISCONNECTED:   return "Disconnected";
        default:                       return "Wi-Fi off";
    }
}
