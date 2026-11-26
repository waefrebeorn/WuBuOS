/*
 * wubu_biometric.c -- see wubu_biometric.h. No shell, no popen, no allocator: stat, readdir, getenv.
 *
 * C11.
 */
#include "wubu_biometric.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *wubu_biometric_root(void)
{
    /* Not cached: a root captured before the override was set silently reads the real machine. */
    static char root[WUBU_BIO_PATH_MAX];
    const char *env = getenv("WUBU_FPRINT_ROOT");
    snprintf(root, sizeof root, "%s", (env && env[0]) ? env : "/");
    return root;
}

static bool is_exe(const char *path)
{
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

/* Does `dir` exist and hold at least one entry whose name starts with `prefix`? */
static bool dir_has_prefix(const char *dir, const char *prefix)
{
    DIR *d = opendir(dir);
    if (!d) return false;

    bool found = false;
    size_t plen = prefix ? strlen(prefix) : 0;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!plen || strncmp(e->d_name, prefix, plen) == 0) found = true;
    }
    closedir(d);
    return found;
}

static int count_entries(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        n++;
    }
    closedir(d);
    return n;
}

/* The reader, the way the kernel would expose it. Checked in order of directness. */
static bool find_reader(WubuBiometric *b)
{
    const char *root = wubu_biometric_root();

    /* An explicit device wins: a platform that names its reader something else wires it here. */
    const char *env = getenv("WUBU_FPRINT_DEVICE");
    if (env && env[0]) {
        struct stat st;
        if (stat(env, &st) == 0) {
            snprintf(b->reader_path, sizeof b->reader_path, "%s", env);
            return true;
        }
        return false;      /* an override naming something that is not there means "no reader" */
    }

    char path[WUBU_BIO_PATH_MAX];
    snprintf(path, sizeof path, "%s/sys/class", root);
    if (dir_has_prefix(path, "fingerprint")) {
        snprintf(b->reader_path, sizeof b->reader_path, "%s/sys/class/fingerprint", root);
        return true;
    }

    snprintf(path, sizeof path, "%s/dev", root);
    if (dir_has_prefix(path, "fprint")) {
        snprintf(b->reader_path, sizeof b->reader_path, "%s/dev/fprint*", root);
        return true;
    }
    if (dir_has_prefix(path, "finger")) {
        snprintf(b->reader_path, sizeof b->reader_path, "%s/dev/*finger*", root);
        return true;
    }
    return false;
}

/* Enrolled prints: fprintd keeps them per user; libfprint-era tools used a system directory. */
static bool find_enrolment(WubuBiometric *b)
{
    const char *root = wubu_biometric_root();
    char dir[WUBU_BIO_PATH_MAX];
    char user[64] = {0};

    const char *env_user = getenv("USER");
    if (env_user && env_user[0]) snprintf(user, sizeof user, "%s", env_user);
    else {
        const char *log = getenv("LOGNAME");
        if (log && log[0]) snprintf(user, sizeof user, "%s", log);
    }

    /* Per-user under XDG data, then the system store. */
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(dir, sizeof dir, "%s/.local/share/fprint", home);
        if (count_entries(dir) > 0) {
            snprintf(b->enrolled_dir, sizeof b->enrolled_dir, "%s", dir);
            b->enrolled = count_entries(dir);
            return true;
        }
    }
    if (user[0]) {
        snprintf(dir, sizeof dir, "%s/var/lib/fprint/%s", root, user);
        if (count_entries(dir) > 0) {
            snprintf(b->enrolled_dir, sizeof b->enrolled_dir, "%s", dir);
            b->enrolled = count_entries(dir);
            return true;
        }
    }
    /* fprintd >= 1.x also writes a user file rather than a directory tree. */
    if (user[0] && home && home[0]) {
        snprintf(dir, sizeof dir, "%s/.local/share/fprint/%s", home, user);
        if (count_entries(dir) > 0) {
            snprintf(b->enrolled_dir, sizeof b->enrolled_dir, "%s", dir);
            b->enrolled = count_entries(dir);
            return true;
        }
    }
    return false;
}

int wubu_biometric_probe(WubuBiometric *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->probed = true;

    const char *root = wubu_biometric_root();

    /* The agents, by the usual locations. PATH resolution is deliberately NOT done: a page that says
     * "found" because a name exists in PATH but cannot be executed is the greyed-button lie again. */
    static const char *const dirs[] = {
        "/usr/bin", "/usr/local/bin", "/bin", "/usr/sbin"
    };
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char p[WUBU_BIO_PATH_MAX];
        snprintf(p, sizeof p, "%s%s/fprintd-enroll", root[0] == '/' && root[1] == '\0' ? "" : root,
                 dirs[i]);
        if (!out->agent_enroll && is_exe(p)) {
            out->agent_enroll = true;
            snprintf(out->enroll_path, sizeof out->enroll_path, "%s", p);
        }
        snprintf(p, sizeof p, "%s%s/fprintd-list", root[0] == '/' && root[1] == '\0' ? "" : root,
                 dirs[i]);
        if (!out->agent_list && is_exe(p)) {
            out->agent_list = true;
            snprintf(out->list_path, sizeof out->list_path, "%s", p);
        }
    }

    out->reader_present = find_reader(out);
    (void)find_enrolment(out);

    return (out->agent_enroll ? 1 : 0) + (out->reader_present ? 1 : 0) + (out->enrolled > 0 ? 1 : 0);
}

void wubu_biometric_status(const WubuBiometric *b, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!b) { snprintf(out, cap, "not probed"); return; }
    if (!b->probed) { snprintf(out, cap, "not probed"); return; }

    if (!b->reader_present) {
        snprintf(out, cap, "No fingerprint reader on this machine%s",
                 b->agent_enroll ? " (the enrolment service is installed)" : "");
        return;
    }
    if (!b->agent_enroll) {
        snprintf(out, cap, "A reader is present, but no enrolment service (fprintd-enroll) is installed");
        return;
    }
    if (b->enrolled <= 0) {
        snprintf(out, cap, "Reader ready at %s -- no fingerprints enrolled yet", b->reader_path);
        return;
    }
    snprintf(out, cap, "%d fingerprint%s enrolled", b->enrolled, b->enrolled == 1 ? "" : "s");
}
