/*
 * taskmgr.c -- the Task Manager entry point.
 *
 * The Task Manager IS the wubulens project (Waefrebeorn/wubulens): a
 * plain-language GPU lens and a real task manager in C11. There is no second
 * implementation here -- this module only opens it, in a real pty-backed
 * terminal window, exactly the way the OS opens a shell.
 *
 * The binary is found in this order:
 *   $WUBULENS   ~/.local/bin/wubulens   /usr/local/bin/wubulens
 *   /usr/bin/wubulens   /bin/wubulens   "wubulens" (PATH)
 * When none is present the terminal shows the failure itself; this module
 * does not draw a fabricated process table in its place. The stub it replaces
 * did exactly that.
 *
 * C11, minimal includes, self-contained.
 */
#include "taskmgr.h"
#include "../gui/wm.h"
#include "../apps/cmd/cmd.h"
#include "../apps/apps.h"          /* the terminal window adapters */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Where wubulens lives. Exposed so a test can prove a real binary was found. */
const char *taskmgr_wubulens_path(void)
{
    static char path[512];

    const char *env = getenv("WUBULENS");
    if (env && env[0]) return env;

    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(path, sizeof(path), "%s/.local/bin/wubulens", home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
            return path;
    }
    static const char *dirs[] = { "/usr/local/bin/wubulens",
                                  "/usr/bin/wubulens",
                                  "/bin/wubulens" };
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        struct stat st;
        if (stat(dirs[i], &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
            return dirs[i];
    }
    return "wubulens";      /* let PATH decide; the pty shows if it fails */
}

int taskmgr_has_session(const DosGuiWindow *win)
{
    return win && win->user_data != NULL;
}

DosGuiWindow* taskmgr_launch(void) {
    DosGuiWindow *win = wm_create(80, 60, 920, 620, "Task Manager (wubulens)");
    if (!win) return NULL;

    /* A real pty-backed terminal running the lens -- not a drawn fake. */
    WubuCmd *cmd = wubu_cmd_create(110, 34);
    if (cmd) {
        wubu_cmd_spawn_shell(cmd, taskmgr_wubulens_path());
        win->user_data = cmd;
        win->on_draw  = terminal_draw;
        win->on_key   = terminal_key;
        win->on_mouse = terminal_mouse;
    }
    return win;
}
