/*
 * file_picker.h -- Open/Save API. The dialog IS Explorer in pick mode.
 *
 * Kept as a thin facade so call sites do not know which component draws the list, and so the OS
 * has exactly ONE file list implementation.
 */
#ifndef WUBU_FILE_PICKER_H
#define WUBU_FILE_PICKER_H

/* Returns 0 when the picker is up, -1 when it could not open. */
int  file_picker_open(const char *start_dir, int save_mode,
                      int (*on_done)(const char *path, void *user), void *user);

int         file_picker_active(void);
int         file_picker_save_mode(void);
int         file_picker_accept(void);
void        file_picker_cancel(void);
const char *file_picker_typed(void);

/* Default completion sink: notifies the chosen path. */
int         wubu_fp_report(const char *path, void *user);

#endif /* WUBU_FILE_PICKER_H */
