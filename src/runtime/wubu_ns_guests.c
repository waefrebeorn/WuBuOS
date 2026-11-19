/*
 * wubu_ns_guests.c -- /n/guests: the running-container control plane.
 *
 * Renders from the LIVE registry in wubu_host_exec.c (the single owner of
 * ct state) on every sync -- no private mirror. Containers launched through
 * guests/<name>/launch are owned here: reaped and destroyed once they exit.
 *
 * The 9P seams (probe/write) are declared weak in styxfs_callbacks.c; these
 * real definitions override them at link time wherever this module is linked
 * (product RT_OBJS + the guests gates). A write to guests/<name>/launch goes
 * through wubu_ns_guests_launch -> wubu_ct_create/set_cmd/start -- the SAME
 * validated exec path as the in-process API, never a lower-level backdoor.
 *
 * C11, minimal includes: FS core (wubu_ns_fs.o) + container lifecycle only.
 */
#include "../egojar.h"
#include "wubu_ns_guests.h"
#include "wubu_ns_bridge_internal.h"
#include "styx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#define GUESTS_MAX 64   /* matches WUBU_CT_REGISTRY_MAX in wubu_host_exec.c */

/* -- Ownership of namespace-launched guests ------------------------- */

static WubuCt *g_owned[GUESTS_MAX];
static int g_owned_n = 0;
static int g_in_sync = 0;   /* re-entrancy guard: retire -> sync nests */

void wubu_ns_guests_forget(WubuCt *ct) {
    for (int i = 0; i < g_owned_n; i++) {
        if (g_owned[i] == ct) {
            g_owned[i] = g_owned[--g_owned_n];
            g_owned[g_owned_n] = NULL;
            return;
        }
    }
}

/* -- Name / path validation ----------------------------------------- */

/* Guest names are single path components: alnum plus . _ - , never "." or
 * "..", never a slash. This is the gate that keeps a control write from
 * addressing anything outside its own guests/<name> subtree. */
static int name_valid(const char *name) {
    if (!name || !*name) return 0;
    size_t l = strlen(name);
    if (l >= WUBU_CT_NAME_MAX) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (size_t i = 0; i < l; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* Extract the subpath after the "guests" path component, so the seam works
 * whether the namespace root is mounted at "/" (sub = "foo/launch") or at
 * "/n" (path = "/n/guests/foo/launch"). Refuses ".." anywhere before it. */
static int guests_sub(const char *path, char *out, size_t outsz) {
    if (!path || !out || outsz == 0) return 0;
    out[0] = '\0';
    const char *p = path;
    while (*p == '/') p++;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') return 0;
        if (len == 6 && strncmp(p, "guests", 6) == 0) {
            snprintf(out, outsz, "%s", slash ? slash + 1 : "");
            return 1;
        }
        if (!slash) break;
        p = slash + 1;
    }
    return 0;
}

/* -- 9P seams -------------------------------------------------------- */

int wubu_ns_guests_probe(const char *path, uint8_t *qid_type) {
    char sub[512];
    if (!guests_sub(path, sub, sizeof(sub))) return 0;
    if (sub[0] == '\0') { *qid_type = STX_QTDIR; return 1; }  /* guests/ */

    char *slash = strchr(sub, '/');
    size_t nlen = slash ? (size_t)(slash - sub) : strlen(sub);
    char name[WUBU_CT_NAME_MAX];
    if (nlen == 0 || nlen >= sizeof(name)) return 0;
    memcpy(name, sub, nlen); name[nlen] = '\0';
    if (!name_valid(name)) return 0;

    if (!slash) { *qid_type = STX_QTDIR; return 1; }          /* guests/<name> */
    if (strcmp(slash + 1, "launch") == 0 ||
        strcmp(slash + 1, "state") == 0) {
        *qid_type = STX_QTFILE;                               /* control file */
        return 1;
    }
    return 0;
}

int wubu_ns_guests_write(const char *path, const uint8_t *data,
                         uint32_t count, uint32_t *nwritten) {
    char sub[512];
    if (!guests_sub(path, sub, sizeof(sub))) return 0;   /* not ours */

    /* Only guests/<name>/launch is a control write. */
    char *slash = strchr(sub, '/');
    if (!slash || strcmp(slash + 1, "launch") != 0) return 0;
    size_t nlen = (size_t)(slash - sub);
    char name[WUBU_CT_NAME_MAX];
    if (nlen == 0 || nlen >= sizeof(name)) return -1;
    memcpy(name, sub, nlen); name[nlen] = '\0';
    if (!name_valid(name)) return -1;

    if (count > 4095) return -1;
    char cmd[4096];
    if (count > 0 && data) memcpy(cmd, data, count);
    cmd[count] = '\0';
    size_t l = strlen(cmd);
    while (l > 0 && (cmd[l-1] == '\n' || cmd[l-1] == '\r' ||
                     cmd[l-1] == ' '  || cmd[l-1] == '\t'))
        cmd[--l] = '\0';
    if (l == 0) return -1;                 /* empty command: refuse loudly */

    if (wubu_ns_guests_launch(name, cmd) != 0) return -1;
    if (nwritten) *nwritten = count;
    return 1;
}

/* -- Command-line splitting (execv semantics, no shell) -------------- */

/* Split buf in place into argv, honouring single/double quotes and
 * backslash escapes. Returns argc, or -1 if more than maxargs tokens.
 * The raw string is NEVER handed to /bin/sh -c (archd N1 doctrine). */
static int split_cmdline(char *buf, char **argv, int maxargs) {
    int argc = 0;
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (argc >= maxargs) return -1;
        char *out = p;   /* token start */
        char *w = p;     /* compaction cursor (w <= p always) */
        char quote = 0;
        while (*p) {
            char c = *p;
            if (quote) {
                if (c == quote) { quote = 0; p++; continue; }
                if (quote == '"' && c == '\\' && p[1]) { p++; *w++ = *p++; continue; }
                *w++ = c; p++; continue;
            }
            if (c == '\'' || c == '"') { quote = c; p++; continue; }
            if (c == '\\' && p[1]) { p++; *w++ = *p++; continue; }
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
            *w++ = c; p++;
        }
        char delim = *p;   /* capture BEFORE terminating (w may equal p) */
        *w = '\0';
        argv[argc++] = out;
        if (delim) p++;
    }
    return argc;
}

/* -- Launch (the validated exec path) -------------------------------- */

int wubu_ns_guests_launch(const char *name, const char *cmdline) {
    if (!name_valid(name) || !cmdline) return -1;
    if (wubu_ct_registry_find(name)) return -1;   /* one running guest/name */
    if (g_owned_n >= GUESTS_MAX) return -1;

    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", cmdline);
    char *argv[WUBU_CT_MAX_ARGS];
    int argc = split_cmdline(buf, argv, WUBU_CT_MAX_ARGS);
    if (argc <= 0) return -1;

    /* Default root "/" = host-namespace native guest (the existing exec
     * path's convention); an operator can pin a real rootfs via env. */
    const char *root = getenv("WUBU_GUESTS_LAUNCH_ROOT");
    if (!root || !*root) root = "/";

    WubuCt *ct = wubu_ct_create(name, root, CT_NATIVE);
    if (!ct) return -1;
    if (wubu_ct_set_cmd(ct, argc, argv) != 0) { wubu_ct_destroy(ct); return -1; }
    if (wubu_ct_start(ct) != 0)               { wubu_ct_destroy(ct); return -1; }

    g_owned[g_owned_n++] = ct;   /* this module owns what it launched */
    wubu_ns_guests_sync();
    return 0;
}

/* -- Sync: render /n/guests from live state -------------------------- */

int wubu_ns_guests_sync(void) {
    if (g_in_sync) return 0;       /* nested retire-sync: outer one renders */
    if (!g_ns_root) return -1;
    g_in_sync = 1;

    /* 1. Reap namespace-launched guests that have exited. wubu_ct_state()
     *    retires them (detach + nested sync, a no-op under the guard);
     *    destroy() forgets the owned entry and frees. forget() shifts the
     *    list, so re-scan the same index after a reap. */
    for (int i = 0; i < g_owned_n; ) {
        WubuCt *ct = g_owned[i];
        if (wubu_ct_state(ct) != CT_RUNNING)
            wubu_ct_destroy(ct);
        else
            i++;
    }

    /* 2. Snapshot the registry, then render each LIVE running guest. */
    WubuCt *snap[GUESTS_MAX];
    int n = 0;
    for (int i = 0; i < wubu_ct_registry_count() && n < GUESTS_MAX; i++)
        snap[n++] = wubu_ct_registry_get(i);

    if (ns_mkdir("guests") != 0) { g_in_sync = 0; return -1; }

    char list[8192]; size_t off = 0; list[0] = '\0';
    char running[GUESTS_MAX][WUBU_CT_NAME_MAX];
    int nrun = 0;

    for (int i = 0; i < n; i++) {
        WubuCt *ct = snap[i];
        CtState s = wubu_ct_state(ct);   /* live refresh; may retire */
        if (s != CT_RUNNING) continue;

        int w = snprintf(list + off, sizeof(list) - off,
                         "%s\t%d\t%s\t%s\t%s\n",
                         ct->name, ct->pid, ct->root,
                         wubu_ct_runtime_name(ct->runtime),
                         wubu_ct_state_name(s));
        if (w < 0 || (size_t)w >= sizeof(list) - off) break;
        off += (size_t)w;

        if (nrun < GUESTS_MAX) {
            strncpy(running[nrun], ct->name, WUBU_CT_NAME_MAX - 1);
            running[nrun][WUBU_CT_NAME_MAX - 1] = '\0';
            nrun++;
        }

        char sub[512], body[1024];
        snprintf(sub, sizeof(sub), "guests/%s", ct->name);
        ns_mkdir(sub);
        snprintf(body, sizeof(body),
                 "state: %s\npid: %d\nroot: %s\npersonality: %s\nstyx: %s\n",
                 wubu_ct_state_name(s), ct->pid, ct->root,
                 wubu_ct_runtime_name(ct->runtime), ct->styx_path);
        snprintf(sub, sizeof(sub), "guests/%s/state", ct->name);
        ns_write(sub, body);
        snprintf(sub, sizeof(sub), "guests/%s/launch", ct->name);
        ns_write(sub, "# write a command line to launch this guest\n");
    }
    ns_write("guests/list", list);

    /* 3. Remove published entries for guests no longer running. Collect
     *    names first, then delete (deleting while enumerating shifts). */
    char gdir[4096];
    snprintf(gdir, sizeof(gdir), "%s/guests", g_ns_root);
    DIR *d = opendir(gdir);
    if (d) {
        char stale[GUESTS_MAX][WUBU_CT_NAME_MAX];
        int nstale = 0;
        struct dirent *de;
        while ((de = readdir(d)) != NULL && nstale < GUESTS_MAX) {
            if (de->d_name[0] == '.') continue;
            int keep = 0;
            for (int i = 0; i < nrun; i++)
                if (strcmp(running[i], de->d_name) == 0) { keep = 1; break; }
            if (!keep) {
                strncpy(stale[nstale], de->d_name, WUBU_CT_NAME_MAX - 1);
                stale[nstale][WUBU_CT_NAME_MAX - 1] = '\0';
                nstale++;
            }
        }
        closedir(d);
        for (int i = 0; i < nstale; i++) {
            char p[4096 + 80];
            snprintf(p, sizeof(p), "%s/%s/state", gdir, stale[i]);  unlink(p);
            snprintf(p, sizeof(p), "%s/%s/launch", gdir, stale[i]); unlink(p);
            snprintf(p, sizeof(p), "%s/%s", gdir, stale[i]);        rmdir(p);
        }
    }

    g_in_sync = 0;
    return 0;
}
