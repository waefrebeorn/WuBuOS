/*
 * mime_contract_test.c -- hostile-input contract sweep for the .desktop parser.
 *
 * Shortcut icons resolve through parse_desktop_file(), which reads files the
 * USER wrote (or a distribution shipped).  Feeding that path malformed data is
 * how "shortcuts are bugged, so many crash bugs" actually shows up.
 *
 * Each case runs in a FORKED CHILD with alarm(1): a segfault or a hang costs one
 * child and is reported as a failure, instead of taking the whole gate down the
 * way an in-process fault would.
 */

#include "wubu_mime.h"
#include "wubu_mime_internal.h"   /* parse_desktop_file lives here */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

static const char *kDir = "/home/wubu/.hermes/cache/scratch";

/* Run fn() in a child; return its exit status, or -1 when it died by signal. */
typedef void (*probe_fn)(const char *);
static int run_isolated(probe_fn fn, const char *path)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        alarm(1);
        fn(path);
        _exit(0);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) return -1;          /* crashed */
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) return WEXITSTATUS(st);
    return 0;
}

static bool parse_ok(const char *p) { DesktopEntry e; return parse_desktop_file(p, &e); }

/* ---- probes: one per hostile shape ------------------------------- */

static void p_empty(const char *path)      { DesktopEntry e; (void)parse_desktop_file(path, &e); }
static void p_no_keys(const char *path)    { DesktopEntry e; (void)parse_desktop_file(path, &e); }
static void p_unterminated(const char *p)  { DesktopEntry e; (void)parse_desktop_file(p, &e); }
static void p_long_value(const char *p)    { DesktopEntry e; (void)parse_desktop_file(p, &e); }
static void p_many_actions(const char *p)  { DesktopEntry e; (void)parse_desktop_file(p, &e); }
static void p_nul_bytes(const char *p)     { DesktopEntry e; (void)parse_desktop_file(p, &e); }
static void p_missing(const char *p)       { DesktopEntry e; (void)parse_desktop_file(p, &e); }

/* A section header whose ']' is missing must be ignored, not fatal. */
static void p_bad_section(const char *p)   { DesktopEntry e; (void)parse_desktop_file(p, &e); }

int main(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/mime-contract-%d", kDir, (int)getpid());
    FILE *f;

    /* 1. empty file: not an entry, but must not crash */
    f = fopen(path, "w"); fclose(f);
    TEST(run_isolated(p_empty, path) == 0, "empty file does not crash the parser");
    TEST(!parse_ok(path), "an empty file is not a valid entry");

    /* 2. section header with no keys */
    f = fopen(path, "w"); fprintf(f, "[Desktop Entry]\n"); fclose(f);
    TEST(run_isolated(p_no_keys, path) == 0, "header-only file does not crash");
    TEST(!parse_ok(path), "no Name means not a valid entry");

    /* 3. unterminated section header */
    f = fopen(path, "w"); fprintf(f, "[Desktop Action Foo\nExec=whatever\n"); fclose(f);
    TEST(run_isolated(p_unterminated, path) == 0,
         "unterminated '[' section does not crash");

    /* 4. a value far longer than any field (fgets splits it) */
    f = fopen(path, "w");
    fprintf(f, "[Desktop Entry]\nName=Big\nExec=");
    for (int i = 0; i < 8000; i++) fputc('a', f);
    fprintf(f, "\n");
    fclose(f);
    TEST(run_isolated(p_long_value, path) == 0,
         "an over-long Exec does not crash the parser");
    {
        DesktopEntry e;
        TEST(parse_desktop_file(path, &e) && e.exec[0], "over-long Exec still yields a value");
    }

    /* 5. more actions than MAX_DESKTOP_ACTIONS -- must cap, not overflow */
    f = fopen(path, "w");
    fprintf(f, "[Desktop Entry]\nName=Many\n");
    for (int i = 0; i < 20; i++)
        fprintf(f, "[Desktop Action act%d]\nName=Action %d\nExec=cmd%d\n", i, i, i);
    fclose(f);
    TEST(run_isolated(p_many_actions, path) == 0,
         "20 action sections do not overflow the action table");
    {
        DesktopEntry e;
        TEST(parse_desktop_file(path, &e), "many-action file parses");
        TEST(e.action_count == MAX_DESKTOP_ACTIONS,
             "action_count is capped at MAX_DESKTOP_ACTIONS");
    }

    /* 5b. the shortcut path end to end: the wizard writes Exec=<target>, and that
     * target must survive parsing as a LAUNCHABLE command (not empty, not mangled).
     * A shortcut whose Exec is empty is exactly "shortcuts aren't shortcuts". */
    f = fopen(path, "w");
    fprintf(f, "[Desktop Entry]\nType=Application\nName=Notes\n"
               "Exec=notepad %%s/notes.txt\nTerminal=false\n");
    fclose(f);
    {
        DesktopEntry e;
        TEST(parse_desktop_file(path, &e), "a shortcut entry parses");
        TEST(e.exec[0] != '\0', "the shortcut carries a non-empty Exec");
        TEST(strstr(e.exec, "notepad") != NULL,
             "the shortcut's Exec names the real program");
        /* Field codes (%s/%f) must survive parsing intact -- desktop_launch_icon()
         * strips them at launch time, so a parser that dropped them would hand
         * back a command with its argument missing. */
        TEST(strstr(e.exec, "%s/notes.txt") != NULL,
             "the Exec keeps its argument, field code unharmed");
    }

    /* 6. the regression this commit fixes: two actions land in DIFFERENT slots */
    f = fopen(path, "w");
    fprintf(f, "[Desktop Entry]\nName=Two\nActions=open;edit\n"
               "[Desktop Action open]\nName=Open\nExec=open-tool %%f\n"
               "[Desktop Action edit]\nName=Edit\nExec=edit-tool %%f\n");
    fclose(f);
    {
        DesktopEntry e;
        TEST(parse_desktop_file(path, &e), "two-action file parses");
        TEST(e.action_count == 2, "both actions counted");
        /* The Actions= list holds lower-case ids; each section's Name= is the
         * DISPLAY label and legitimately overwrites the id, so the id is not
         * asserted here.  What matters is that the two sections occupy two
         * DIFFERENT slots -- before the fix they both wrote slot 0. */
        TEST(strcmp(e.action_list[0].name, e.action_list[1].name) != 0,
             "two actions occupy different slots, not one overwritten twice");
        TEST(strcmp(e.action_list[0].name, "Open") == 0, "slot 0 shows its display Name");
        TEST(strstr(e.action_list[0].exec, "open-tool") != NULL,
             "slot 0 carries the open Exec");
        TEST(strstr(e.action_list[1].exec, "edit-tool") != NULL,
             "slot 1 carries the edit Exec -- actions were previously unusable");
    }

    /* 7. NUL bytes embedded in the file must stop the line, not corrupt */
    f = fopen(path, "w");
    fprintf(f, "[Desktop Entry]\nName=Nul");
    fputc('\0', f);
    fprintf(f, "\nExec=after-nul\n");
    fclose(f);
    TEST(run_isolated(p_nul_bytes, path) == 0, "embedded NUL does not crash");

    /* 8. path that does not exist */
    snprintf(path, sizeof(path), "%s/definitely-not-here-%d.desktop", kDir, (int)getpid());
    TEST(run_isolated(p_missing, path) == 0, "missing file does not crash");
    TEST(!parse_ok(path), "missing file reports invalid");

    /* 9. a '[' line that is not a group header at all */
    snprintf(path, sizeof(path), "%s/mime-contract-%d", kDir, (int)getpid());
    f = fopen(path, "w");
    fprintf(f, "[\nName=Orphan\n[Desktop Entry]\nName=Real\nExec=real-tool\n");
    fclose(f);
    TEST(run_isolated(p_bad_section, path) == 0, "orphan '[' does not crash");

    unlink(path);
    printf("=== mime_contract: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
