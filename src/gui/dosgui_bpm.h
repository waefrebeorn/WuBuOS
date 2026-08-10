/*
 * dosgui_bpm.h -- Big Picture Mode (the gamepad-first fullscreen
 * shell over the era apps).
 */
#ifndef WUBU_DOSGUI_BPM_H
#define WUBU_DOSGUI_BPM_H

#include <stdint.h>

/* the launch hook (injectable — the tests record instead of exec) */
typedef int (*BpmLaunchFn)(int idx);

/* BP1: init the shell. */
void dosgui_bpm_init(void);

/* BP2: the launch hook override. */
void dosgui_bpm_set_launch(BpmLaunchFn fn);

/* BP3: open / close / is-open. */
void dosgui_bpm_open(void);
void dosgui_bpm_close(void);
int  dosgui_bpm_is_open(void);

/* BP4: the grid rows. */
int dosgui_bpm_rows(void);

/* BP5: the gamepad navigation (d-pad + A/B). Returns 1 if consumed. */
int dosgui_bpm_input(uint32_t key, uint32_t mods, int down);

/* BP6: the current selection. */
int dosgui_bpm_selection(void);

/* BP7: the test hooks. */
typedef struct {
    int open;
    int selection;
    int rows;
} dosgui_bpm_view_t;
int dosgui_bpm_get(dosgui_bpm_view_t *out);

#endif
