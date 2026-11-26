/*
 * file_picker.h -- Open/Save dialog (Win11 parity: Windows.FilePicker).
 *
 * One modal picker at a time. It lists a real directory, navigates into folders, accepts a
 * typed name, and reports the chosen absolute path through a callback. Cancel (Esc / the
 * Cancel button) reports nothing.
 */
#ifndef WUBU_FILE_PICKER_H
#define WUBU_FILE_PICKER_H

#define WUBU_FILE_PICKER_W 560
#define WUBU_FILE_PICKER_H 420

/* Returns 0 when the dialog is up, -1 when it could not open (no window or no readable dir). */
int file_picker_open(const char *start_dir, int save_mode,
                     int (*on_done)(const char *path, void *user), void *user);

int         file_picker_active(void);
const char *file_picker_result(void);      /* NULL until something was accepted */
const char *file_picker_dir(void);
int         file_picker_count(void);
const char *file_picker_entry(int i);
int         file_picker_entry_is_dir(int i);
int         file_picker_selected(void);

/* Default completion sink: notifies the chosen path (weak notify, so a narrow link still works). */
int         wubu_fp_report(const char *path, void *user);

#endif /* WUBU_FILE_PICKER_H */
