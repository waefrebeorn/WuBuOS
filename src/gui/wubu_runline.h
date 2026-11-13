/*
 * wubu_runline.h -- run one command line and show what happened.
 *
 * This is the single implementation behind BOTH Win+R and clicking a desktop
 * shortcut.  It used to exist only inside run_dialog.c, which meant the two
 * paths could not share behaviour: a shortcut that ran a program had no way to
 * reuse "run it, capture both streams, put the result somewhere durable".
 *
 * WHAT IT DOES
 *   Executes the command through /bin/sh -c with stderr merged into stdout,
 *   writes "$ <command>" + a separator + the output into a guest file, flushes
 *   it to disk, and opens it in Notepad.
 *
 * WHAT IT DOES NOT DO
 *   It is not a sandbox: commands run on the host under the same uid as the
 *   desktop.  It blocks the UI until the command exits.  Both are documented in
 *   place rather than hidden.
 *
 * FAILURE POLICY
 *   Returns -1 and leaves no partial file when the command cannot even start.
 *   Output that will not fit a guest file is marked "... truncated", never
 *   silently chopped.
 */
#ifndef WUBU_RUNLINE_H
#define WUBU_RUNLINE_H

/* Run `command_line` (may be NULL/empty, in which case nothing happens).
 * `result_file` is the guest path the report is written to.
 * Returns 0 when the report was written, -1 otherwise. */
int wubu_runline_execute(const char *command_line, const char *result_file);

#endif /* WUBU_RUNLINE_H */
