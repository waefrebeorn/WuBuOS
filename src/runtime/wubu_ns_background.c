/*
 * wubu_ns_background.c -- the /n/background control subtree (P3).
 *
 * Snapshots the session-boot's live state into /n/background files via the
 * shared ns_mkdir/ns_write primitives. No daemon: the session owner calls
 * wubu_ns_publish_background() after boot and on every state change.
 * C11, runtime host; links only wubu_ns_fs.c + wubu_bus/wubu_session_boot.
 */
#include "wubu_ns_background.h"
#include <stdio.h>
/* ns_mkdir / ns_write / g_ns_root come from wubu_ns_fs.c (the FS core). */
extern int ns_mkdir(const char *sub);
extern int ns_write(const char *sub, const char *buf);

int wubu_ns_publish_background(wubu_session_boot_t *boot) {
    if (!boot) return -1;
    if (ns_mkdir("background") != 0) return -1;
    if (ns_mkdir("background/bus") != 0) return -1;
    if (ns_mkdir("background/autostart") != 0) return -1;

    /* session-boot mode (a NAME only; the authoritative session/ state is
     * published by wubu_ns_publish_login under background/session/). Named
     * session_boot to avoid colliding with the login publisher's directory. */
    if (ns_write("background/session_boot", "desktop\n") != 0) return -1;

    /* bus: one well-known name per line + live connection count */
    wubu_bus_t *bus = wubu_session_boot_bus(boot);
    if (!bus) return -1;
    int nn = wubu_bus_name_count(bus);
    int used = 0;
    char buf[4096];
    if (nn > 0) {
        int off = 0;
        for (int k = 0; k < nn && off < (int)sizeof(buf) - 2; k++) {
            const char *nm = wubu_bus_name_at(bus, k);
            if (!nm) continue;
            int r = snprintf(buf + off, sizeof(buf) - (size_t)off, "%s\n", nm);
            if (r < 0) break;
            used += r;
            off += r;
        }
    }
    if (used > 0) {
        if (ns_write("background/bus/names", buf) != 0) return -1;
    } else {
        if (ns_write("background/bus/names", "\n") != 0) return -1;
    }
    char small[64];
    snprintf(small, sizeof(small), "%d\n", wubu_bus_conn_count(bus));
    if (ns_write("background/bus/nconn", small) != 0) return -1;

    /* autostart: one Name= line per launchable entry + the count */
    int ne = wubu_session_boot_count(boot);
    char acount[64];
    snprintf(acount, sizeof(acount), "%d\n", ne);
    if (ns_write("background/autostart/count", acount) != 0) return -1;
    for (int i = 0; i < ne; i++) {
        const wubu_xdga_entry_t *e = wubu_session_boot_entry(boot, i);
        if (!e) continue;
        char sub[64];
        snprintf(sub, sizeof(sub), "background/autostart/%d", i);
        if (ns_write(sub, e->name) != 0) return -1;
    }
    return 0;
}

int wubu_ns_publish_login(wubu_logindd_t *login) {
    if (!login) return -1;
    if (ns_mkdir("background/session") != 0) return -1;

    const wubu_logindd_session_t *act = wubu_logindd_active(login);

    char small[128];
    char nbuf[1024];
    if (act) {
        snprintf(small, sizeof(small), "%u\n", act->id);
        if (ns_write("background/session/active", small) != 0) return -1;
        if (ns_write("background/session/seat", act->seat) != 0) return -1;
        const char *ts = wubu_logindd_type_str(act->type);
        if (ts && ns_write("background/session/type", ts) != 0) return -1;
    } else {
        if (ns_write("background/session/active", "none\n") != 0) return -1;
        if (ns_write("background/session/seat", "none\n") != 0) return -1;
    }

    int n = wubu_logindd_session_count(login);
    snprintf(small, sizeof(small), "%d\n", n);
    if (ns_write("background/session/count", small) != 0) return -1;
    if (n > 0) {
        int off = 0;
        for (int i = 0; i < n && off < (int)sizeof(nbuf) - 4; i++) {
            const wubu_logindd_session_t *s = wubu_logindd_session_at(login, i);
            if (!s) continue;
            const char *st = wubu_logindd_state_str(s->state);
            off += snprintf(nbuf + off, sizeof(nbuf) - (size_t)off,
                            "c%u %s %s %s\n", s->id, s->seat,
                            st ? st : "?", wubu_logindd_type_str(s->type));
        }
        if (ns_write("background/session/list", nbuf) != 0) return -1;
    }
    return 0;
}

int wubu_ns_publish_auth(wubu_auth_t *auth) {
    if (!auth) return -1;
    if (ns_mkdir("background/auth") != 0) return -1;

    char small[32];
    snprintf(small, sizeof(small), "%d\n", wubu_auth_has_agent(auth));
    if (ns_write("background/auth/has_agent", small) != 0) return -1;

    int n = wubu_auth_rule_count(auth);
    snprintf(small, sizeof(small), "%d\n", n);
    if (ns_write("background/auth/rulecount", small) != 0) return -1;

    char buf[8192];
    int off = 0;
    for (int i = 0; i < n && off < (int)sizeof(buf) - 4; i++) {
        uint32_t subj; char action[WUBU_AUTH_MAX_ACTION];
        wubu_auth_decision_t d;
        if (wubu_auth_rule_at(auth, i, &subj, action, sizeof(action), &d) != 0)
            continue;
        const char *dn = (d == WUBU_AUTH_DENY) ? "deny" :
                         (d == WUBU_AUTH_INTERACT) ? "interact" : "allow";
        char subjstr[16];
        if (subj == WUBU_AUTH_UID_ANY) snprintf(subjstr, sizeof(subjstr), "any");
        else snprintf(subjstr, sizeof(subjstr), "%u", subj);
        off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                        "%s:%s=%s\n", subjstr, action, dn);
    }
    if (off > 0) {
        if (ns_write("background/auth/rules", buf) != 0) return -1;
    } else {
        if (ns_write("background/auth/rules", "\n") != 0) return -1;
    }
    return 0;
}
