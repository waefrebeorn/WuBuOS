/*
 * getting_started.c -- the "hello world" path.
 *
 * This used to be a plain text document in a window. It is now the
 * walkthrough: the same real steps, plus the official clipart, real
 * buttons, and a step indicator. See wubu_walkthrough.c -- this file is
 * only the entry point, kept because the launcher and the first-run test
 * both call gs_launch().
 *
 * The steps are the real ones -- HolyD is the in-tree compiler, the
 * terminal and the VWS designer are both reachable from the startmenu, and
 * a .wet applet is Lua. Nothing here is aspirational.
 */
#include "getting_started.h"
#include "wubu_walkthrough.h"

DosGuiWindow *gs_launch(void) {
    wubu_walkthrough_run();
    return NULL;   /* the walkthrough owns its window */
}
