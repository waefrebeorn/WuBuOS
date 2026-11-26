/*
 * wubu_pkgx_test.c -- registration, identity, enumeration, resolution, removal, and the two refusals that
 * keep a packaged runtime from becoming a path-escape: an absolute exec and a "../" exec.
 */
#include "wubu_pkgx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL %s\n", m); fails++; } \
                         else printf("  ok   %s\n", m); } while (0)

static char g_root[256];

static void write_file(const char *path, const char *text, int exec)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
    if (exec) chmod(path, 0755);
}

static int mk_pkg(const char *name, const char *publisher, const char *exec_rel, int make_payload)
{
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s/%s", g_root, name);
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/package.conf", dir);
    char conf[512];
    snprintf(conf, sizeof conf, "name=%s\nversion=1.2.3\npublisher=%s\nexec=%s\n", name, publisher, exec_rel);
    write_file(path, conf, 0);
    if (make_payload) {
        snprintf(path, sizeof path, "%s/%s", dir, exec_rel);
        /* the payload may live in a subdirectory: create it, or the write silently does nothing and the
         * registration test fails for a reason that has nothing to do with registration */
        for (char *p = path + strlen(dir) + 1; *p; p++)
            if (*p == '/') { *p = 0; mkdir(path, 0755); *p = '/'; }
        write_file(path, "#!/bin/true\n", 1);
    }
    return 0;
}

int main(void)
{
    char tmpl[] = "/tmp/wubu_pkgx_XXXXXX";
    char *root = mkdtemp(tmpl);
    CHECK(root != NULL, "temp packages root created");
    if (!root) return 1;
    snprintf(g_root, sizeof g_root, "%s", root);
    setenv("WUBU_PKGS", g_root, 1);

    /* identity */
    char f1[WUBU_PKGX_FAMILY_MAX], f2[WUBU_PKGX_FAMILY_MAX], f3[WUBU_PKGX_FAMILY_MAX];
    CHECK(wubu_pkgx_family_name("Notes", "CN=WaefreBeorn", f1, sizeof f1) == 0, "a family name is built");
    CHECK(wubu_pkgx_family_name("Notes", "CN=WaefreBeorn", f2, sizeof f2) == 0 && strcmp(f1, f2) == 0,
          "the same publisher yields the same family name");
    CHECK(wubu_pkgx_family_name("Notes", "CN=SomeoneElse", f3, sizeof f3) == 0 && strcmp(f1, f3) != 0,
          "a different publisher yields a different family name");
    CHECK(strncmp(f1, "Notes_", 6) == 0 && strlen(f1) == 14, "the family name has the MSIX shape name_8hex");

    /* registration */
    WubuPkgRegistry r;
    CHECK(wubu_pkgx_list(&r) == 0 && r.count == 0, "an empty packages root lists no packages");

    mk_pkg("Notes", "CN=WaefreBeorn", "bin/notes", 1);
    WubuPkgApp a;
    char notes_dir[512];
    snprintf(notes_dir, sizeof notes_dir, "%s/Notes", g_root);
    CHECK(wubu_pkgx_register(notes_dir, &a) == 0, "a well-formed package registers");
    CHECK(strcmp(a.name, "Notes") == 0 && strcmp(a.version, "1.2.3") == 0, "name and version are read");
    CHECK(strcmp(a.family, f1) == 0, "the registered family name matches the computed one");

    char exec[600];
    CHECK(wubu_pkgx_exec_path(&a, exec, sizeof exec) == 0 && strstr(exec, "/Notes/bin/notes") != NULL,
          "the entry point is the package root plus the declared exec");

    /* refusals */
    mk_pkg("Escape", "CN=WaefreBeorn", "../outside", 1);
    char edir[512];
    snprintf(edir, sizeof edir, "%s/Escape", g_root);
    CHECK(wubu_pkgx_register(edir, NULL) == 2, "an exec that escapes the package is refused");
    mk_pkg("Absolute", "CN=WaefreBeorn", "/bin/sh", 1);
    snprintf(edir, sizeof edir, "%s/Absolute", g_root);
    CHECK(wubu_pkgx_register(edir, NULL) == 2, "an absolute exec is refused");
    mk_pkg("NoPayload", "CN=WaefreBeorn", "bin/missing", 0);
    snprintf(edir, sizeof edir, "%s/NoPayload", g_root);
    CHECK(wubu_pkgx_register(edir, NULL) == 2, "a package whose entry point is missing is refused");

    /* enumeration + resolution */
    mk_pkg("Clock", "CN=Other", "clock", 1);
    CHECK(wubu_pkgx_list(&r) == 2 && strcmp(r.a[0].name, "Clock") == 0 && strcmp(r.a[1].name, "Notes") == 0,
          "two registered packages list in name order (the invalid ones do not)");
    CHECK(wubu_pkgx_resolve("Notes", &a) == 0 && strcmp(a.name, "Notes") == 0, "resolve by name");
    CHECK(wubu_pkgx_resolve(f1, &a) == 0 && strcmp(a.name, "Notes") == 0, "resolve by family name");
    CHECK(wubu_pkgx_resolve("Nope", &a) == 1, "an unknown id does not resolve");

    /* removal */
    CHECK(wubu_pkgx_unregister("Notes") == 0, "a package unregisters");
    CHECK(wubu_pkgx_list(&r) == 1 && strcmp(r.a[0].name, "Clock") == 0, "and is gone from the list");
    CHECK(wubu_pkgx_unregister("Notes") == 1, "unregistering it twice reports not-found");
    CHECK(wubu_pkgx_unregister("../etc") == 1, "a name that is a path is refused");

    /* clean up */
    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_root);
    if (system(cmd) != 0) printf("  (cleanup of %s failed)\n", g_root);
    printf("wubu_pkgx: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
