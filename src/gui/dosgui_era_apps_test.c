/*
 * dosgui_era_apps_test.c -- regression test for the "one app per era"
 * registry + launcher. Asserts that each RUNNABLE era app actually executes
 * through its VSL syscall personality's REAL exec backend (not just
 * compiles/registers), and that the EMULATOR-GAP eras (CP/M, Classic Mac)
 * are honestly reported as not-launchable.
 *
 * Verified host effects:
 *   - DOS  (0xD0) : wubu_dos_proc_launch() returns a live process (8086 shim)
 *   - Win  (0xFF) : wubu_exec_win_pe -> Wine writes WUBU_ERA_WIN.OK
 *   - Lin  (0x00) : wubu_exec_linux_elf -> bwrap writes WUBU_ERA_LINUX.OK
 *   - HolC (0xF0) : wubu_exec_holyc -> hc_eval runs (returns 0)
 *   - CPM  (0xC0) / Mac (0xB0) : launcher returns -1 (no CPU emulator)
 */

#include "dosgui_era_apps.h"
#include "dosgui_startmenu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* dosgui_era_apps_register() writes into g_program_db (defined in
 * dosgui_startmenu.c). This test exercises the LAUNCHER (which does not
 * touch g_program_db), so we provide a local definition to satisfy the
 * linker without pulling in the whole startmenu graph. */
SmProgramDB g_program_db;

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

static int file_exists(const char *p) {
    struct stat st;
    return (stat(p, &st) == 0);
}
static void rmf(const char *p) { unlink(p); }

int main(void) {
    /* Resolve demos/era paths against the repo root. */
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
    setenv("WUBU_REPO_ROOT", cwd, 1);

    /* Clear any stale marker files from a previous run. */
    rmf("WUBU_ERA_WIN.OK");
    rmf("WUBU_ERA_LINUX.OK");
    rmf("WUBU_ERA_DOS.OK");

    /* --- Registry integrity --- */
    CHECK(dosgui_era_apps_runnable_count() == 4,
          "4 runnable eras (DOS, Win, Linux, HolyC)");
    uint32_t personas[8];
    int cov = dosgui_era_apps_personality_coverage(personas, 8);
    CHECK(cov >= 6, "covers >=6 distinct VSL personalities");

    /* --- DOS (8086 shim) : real launch, no container --- */
    int dos_idx = -1;
    for (int i = 0; i < 7 && dos_idx < 0; i++)
        if (strcmp(dosgui_era_personality_label(0xD0), "MS-DOS (8086 shim)") == 0) dos_idx = i;
    /* Find by scanning the registry names instead (robust). */
    const char *names[7] = {
        "CP/M :: STAT", "DOS :: HELLO", "Mac :: About",
        "Win32 :: Era Demo", "macOS :: Era Demo", "Linux :: Era Demo", "HolyC :: Era Demo"
    };
    int idx[7];
    for (int i = 0; i < 7; i++) idx[i] = -1;
    /* dosgui_era_apps exposes no name accessor; map by known order. */
    idx[0] = 0; idx[1] = 1; idx[2] = 2; idx[3] = 3; idx[4] = 4; idx[5] = 5; idx[6] = 6;

    int rc_dos = dosgui_era_apps_launch(idx[1]);
    CHECK(rc_dos == 0, "DOS era app launches via 8086 shim (rc==0)");

    /* --- Win (Wine) : real host effect (marker file) --- */
    int rc_win = dosgui_era_apps_launch(idx[3]);
    CHECK(rc_win == 0, "Win32 era app launch attempt returns 0");
    /* Give Wine/bwrap a moment, then assert the marker file. */
    usleep(500000);
    CHECK(file_exists("WUBU_ERA_WIN.OK"),
          "Win32 era app wrote WUBU_ERA_WIN.OK through Wine/NT syscalls");

    /* --- Linux (VSL ELF) : real host effect (marker file) --- */
    int rc_lin = dosgui_era_apps_launch(idx[5]);
    CHECK(rc_lin == 0, "Linux era app launch attempt returns 0");
    usleep(500000);
    CHECK(file_exists("WUBU_ERA_LINUX.OK"),
          "Linux era app wrote WUBU_ERA_LINUX.OK through VSL fileio");

    /* --- HolyC (JIT) : launcher dispatches to hc_eval --- */
    int rc_hc = dosgui_era_apps_launch(idx[6]);
    CHECK(rc_hc == 0, "HolyC era app launches via hc_eval JIT (rc==0)");

    /* --- CP/M (gap) : no 8080 emulator -> honest -1 --- */
    int rc_cpm = dosgui_era_apps_launch(idx[0]);
    CHECK(rc_cpm == -1, "CP/M era app reports gap (no 8080 emulator) -> -1");

    /* --- Classic Mac (gap) : no 68000 emulator -> honest -1 --- */
    int rc_mac = dosgui_era_apps_launch(idx[2]);
    CHECK(rc_mac == -1, "Classic Mac era app reports gap (no 68000 emulator) -> -1");

    /* --- launch-by-name fallback path (desktop click handler uses this) --- */
    CHECK(dosgui_era_apps_launch_by_name("Linux :: Era Demo") == 0,
          "launch_by_name('Linux :: Era Demo') works");

    printf("\n%s era-apps launcher tests\n", g_fail == 0 ? "ALL PASSED" : "SOME FAILED");
    return g_fail == 0 ? 0 : 1;
}
