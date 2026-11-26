/*
 * wubu_userinfo.c -- see wubu_userinfo.h. Read-only: one small file, one stat, no shell, no allocator.
 *
 * C11.
 */
#include "wubu_userinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *wubu_userinfo_passwd_root(void)
{
    const char *env = getenv("WUBU_PASSWD_ROOT");
    if (env && env[0]) return env;
    return "/";
}

static bool is_exe(const char *path)
{
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

/* Split one passwd line into its seven fields, in place. Returns false unless all seven are present. */
static bool split_passwd(char *line, char *f[7])
{
    int n = 0;
    char *p = line;
    f[n++] = p;
    while (*p && n < 7) {
        if (*p == ':') {
            *p = '\0';
            f[n++] = p + 1;
        }
        p++;
    }
    return n == 7;
}

int wubu_userinfo_read(int uid, WubuUserInfo *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    out->uid = uid;

    char path[WUBU_USER_PATH_MAX + 32];
    snprintf(path, sizeof path, "%s/etc/passwd", wubu_userinfo_passwd_root());

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[1024];
    bool found = false;
    while (!found && fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char copy[1024];
        snprintf(copy, sizeof copy, "%s", line);
        char *fields[7];
        if (!split_passwd(copy, fields)) continue;

        int this_uid = atoi(fields[2]);
        if (this_uid != uid) continue;

        snprintf(out->login, sizeof out->login, "%s", fields[0]);
        snprintf(out->gecos, sizeof out->gecos, "%s", fields[4]);
        snprintf(out->home, sizeof out->home, "%s", fields[5]);
        snprintf(out->shell, sizeof out->shell, "%s", fields[6]);
        out->gid = atoi(fields[3]);
        out->found = true;
        found = true;
    }
    fclose(f);

    if (!found) return -1;

    /* The picture every desktop looks for. A missing one is not a failure -- it is a default avatar. */
    if (out->home[0]) {
        char face[WUBU_USER_PATH_MAX * 2];
        snprintf(face, sizeof face, "%s/.face", out->home);
        struct stat st;
        out->has_face = (stat(face, &st) == 0 && S_ISREG(st.st_mode));
    }

    /* Changing a password needs a tool; pretending otherwise is the greyed-button lie. */
    static const char *tools[] = { "/usr/bin/passwd", "/bin/passwd" };
    for (unsigned i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        if (is_exe(tools[i])) {
            snprintf(out->passwd_tool, sizeof out->passwd_tool, "%s", tools[i]);
            out->can_change_password = true;
            break;
        }
    }
    return 0;
}

const char *wubu_userinfo_display(const WubuUserInfo *u)
{
    static char buf[160];
    buf[0] = '\0';
    if (!u) return buf;

    /* gecos may carry a full name plus comma-separated junk (room, phone). Show the name only. */
    if (u->gecos[0]) {
        size_t n = strcspn(u->gecos, ",");
        if (n >= sizeof buf) n = sizeof buf - 1;
        memcpy(buf, u->gecos, n);
        buf[n] = '\0';
        return buf;
    }
    snprintf(buf, sizeof buf, "%s", u->login);
    return buf;
}

const char *wubu_userinfo_type(const WubuUserInfo *u)
{
    if (!u || !u->found) return "unknown";
    return u->uid == 0 ? "Administrator" : "Standard user";
}
