/*
 * taskmgr.h -- the Task Manager entry point.
 *
 * The Task Manager is the wubulens project; this module only opens it in a
 * real pty-backed terminal. See taskmgr.c.
 *
 * Opaque, C11, minimal includes.
 */

#ifndef WUBU_TASKMGR_H
#define WUBU_TASKMGR_H

typedef struct DosGuiWindow DosGuiWindow;

/* Open the Task Manager: a terminal window running wubulens. */
DosGuiWindow* taskmgr_launch(void);

/* True when a Task Manager window carries a live pty session. The launcher
 * attaches one; a drawn stub does not. Keeps DosGuiWindow opaque. */
int taskmgr_has_session(const DosGuiWindow *win);

/* Where the wubulens binary was found (or "wubulens" to let PATH decide).
 * Exposed for the test that proves a real binary resolves. */
const char *taskmgr_wubulens_path(void);

#endif /* WUBU_TASKMGR_H */
