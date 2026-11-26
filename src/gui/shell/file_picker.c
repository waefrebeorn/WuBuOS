/*
 * file_picker.c -- the Open/Save entry points, delegated to Explorer.
 *
 * This module used to carry its OWN file list, selection, navigation, sorting and hit-testing:
 * a second copy of the component the OS already ships. Two lists meant two bugs for every fix,
 * and the picker could never navigate like Explorer because it was not Explorer.
 *
 * The picker is now Explorer in pick mode (explorer_open_pick). This file keeps the call sites
 * stable and holds NO state of its own, so it cannot disagree with what the user sees.
 *
 * C11, minimal includes.
 */
#include "file_picker.h"

#include <stdio.h>

#include "../explorer.h"

int file_picker_open(const char *start_dir, int save_mode,
                     int (*on_done)(const char *path, void *user_picker), void *user) {
    /* Explorer's completion contract is (path, user). A caller returning int is expressing
     * "did I handle it", which the picker does not interpret, so the pointer types line up. */
    return explorer_open_pick(start_dir, save_mode,
                              (void (*)(const char *, void *))on_done, user);
}

int file_picker_active(void)          { return explorer_pick_active(); }
int file_picker_save_mode(void)       { return explorer_pick_save_mode(); }
int file_picker_accept(void)          { return explorer_pick_accept(); }
void file_picker_cancel(void)         { explorer_pick_cancel(); }
const char *file_picker_typed(void)   { return explorer_pick_typed(); }

/* Default sink: report the chosen path. A picker whose result nobody can see is a dialog that
 * lies about having done something. */
int wubu_fp_report(const char *path, void *user) {
    (void)user;
    extern __attribute__((weak)) void wubu_notify_simple(const char *, const char *,
                                                         const char *, const char *, int, int);
    if (wubu_notify_simple)
        wubu_notify_simple("Files", "Selected", path, NULL, 1, 3000);
    fprintf(stderr, "[file_picker] selected: %s\n", path ? path : "(null)");
    return 0;
}
