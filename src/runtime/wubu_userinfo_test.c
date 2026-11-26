/*
 * wubu_userinfo_test.c -- the account surface must report what passwd says, and must not invent a
 * display name, a picture or a password tool.
 *
 * Runs against a fake passwd tree ($WUBU_PASSWD_ROOT) and then against the real one.
 *
 * C11.
 */
#include "wubu_userinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

int main(void)
{
    char root[] = "/tmp/uitestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }
    char etc[512];
    snprintf(etc, sizeof etc, "%s/etc", root);
    mkdir(etc, 0755);

    char passwd[600];
    snprintf(passwd, sizeof passwd, "%s/passwd", etc);
    FILE *f = fopen(passwd, "w");
    if (!f) { printf("cannot write fake passwd\n"); return 1; }
    fputs("# fake passwd for the gate\n"
          "root:x:0:0:root:/root:/bin/bash\n"
          "wubu:x:1000:1000:Waefre Beorn,Room 1,555-0100,555-0101:/home/wubu:/bin/bash\n"
          "broken:x:1001\n"                       /* too few fields: must be skipped */
          "nobody:x:65534:65534::/nonexistent:/usr/sbin/nologin\n", f);
    fclose(f);
    setenv("WUBU_PASSWD_ROOT", root, 1);

    printf("=== Test: reading the account ===\n");
    WubuUserInfo u;
    CHECK(wubu_userinfo_read(1000, &u) == 0, "a real uid is found in the passwd tree");
    CHECK(u.found, "the record is marked found");
    CHECK(strcmp(u.login, "wubu") == 0, "the login is read");
    CHECK(u.uid == 1000 && u.gid == 1000, "uid and gid are read");
    CHECK(strcmp(u.home, "/home/wubu") == 0, "the home directory is read");
    CHECK(strcmp(u.shell, "/bin/bash") == 0, "the shell is read");

    /* The display name is the part before the first comma: gecos carries room/phone after it. */
    CHECK(strcmp(wubu_userinfo_display(&u), "Waefre Beorn") == 0,
          "the display name stops at the first comma in gecos");
    CHECK(strcmp(wubu_userinfo_type(&u), "Standard user") == 0, "a non-root account is a standard user");

    /* root is an administrator, and uid 0 with a name is not a special case in the parser. */
    WubuUserInfo r;
    CHECK(wubu_userinfo_read(0, &r) == 0, "uid 0 is read");
    CHECK(strcmp(wubu_userinfo_type(&r), "Administrator") == 0, "uid 0 is an administrator");
    CHECK(strcmp(wubu_userinfo_display(&r), "root") == 0, "without gecos the login is shown");

    /* An account whose line is malformed is NOT half-read. */
    WubuUserInfo bad;
    CHECK(wubu_userinfo_read(1001, &bad) == -1, "a malformed passwd line is not a record");
    CHECK(!bad.found, "a malformed line leaves the record unfound");

    WubuUserInfo missing;
    CHECK(wubu_userinfo_read(4242, &missing) == -1, "an unknown uid is not found");
    CHECK(strcmp(wubu_userinfo_display(&missing), "") == 0, "an unknown uid has no name");
    CHECK(strcmp(wubu_userinfo_type(&missing), "unknown") == 0, "an unknown uid has no type");

    printf("\n=== Test: an empty gecos is not an empty name ===\n");
    WubuUserInfo nb;
    CHECK(wubu_userinfo_read(65534, &nb) == 0, "the account with no gecos is read");
    CHECK(strcmp(wubu_userinfo_display(&nb), "nobody") == 0,
          "an empty gecos falls back to the login");
    CHECK(!nb.has_face, "a home that does not exist has no picture");

    printf("\n=== Test: the picture and the password tool ===\n");
    /* Give the fake account a home with a .face: the picture must be detected, and its absence too. */
    char home[512];
    snprintf(home, sizeof home, "%s/home", root);
    mkdir(home, 0755);
    unsetenv("WUBU_PASSWD_ROOT");
    setenv("HOME", home, 1);

    char passwd2[600];
    snprintf(passwd2, sizeof passwd2, "%s/passwd", etc);
    f = fopen(passwd2, "w");
    if (f) {
        char line[700];
        snprintf(line, sizeof line, "probe:x:2000:2000:Probe User:%s:/bin/sh\n", home);
        fputs(line, f);
        fclose(f);
    }
    setenv("WUBU_PASSWD_ROOT", root, 1);

    WubuUserInfo p;
    CHECK(wubu_userinfo_read(2000, &p) == 0, "the probe account is read");
    CHECK(!p.has_face, "no .face file means no picture");
    char face[600];
    snprintf(face, sizeof face, "%s/.face", home);
    f = fopen(face, "wb");
    if (f) { fputs("P6\n1 1\n255\n\0\0\0", f); fclose(f); }
    CHECK(wubu_userinfo_read(2000, &p) == 0, "the probe account re-reads");
    CHECK(p.has_face, "a .face file is detected as the picture");

    /* The password tool is either found by path or reported absent -- never assumed. */
    if (p.can_change_password)
        printf("  measured: password tool at %s\n", p.passwd_tool);
    else
        printf("  measured: no passwd binary on this machine\n");
    CHECK(!p.can_change_password || p.passwd_tool[0] != '\0',
          "a found password tool has a path");

    printf("\n=== Test: the real machine (skip when it exposes nothing) ===\n");
    unsetenv("WUBU_PASSWD_ROOT");
    WubuUserInfo real;
    if (wubu_userinfo_read((int)getuid(), &real) != 0) {
        printf("  SKIPPED: this machine has no readable /etc/passwd\n");
    } else {
        CHECK(real.found && real.login[0], "the real user is named");
        printf("  measured: %s (%s, uid %d)\n", wubu_userinfo_display(&real),
               wubu_userinfo_type(&real), real.uid);
    }

    char rm[700];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  userinfo: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
