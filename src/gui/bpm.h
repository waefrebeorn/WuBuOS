/*
 * bpm.h -- Big Picture Mode (the gamepad-first fullscreen
 * shell over the era apps).
 */
#ifndef WUBU_DOSGUI_BPM_H
#define WUBU_DOSGUI_BPM_H

#include <stdint.h>

/* the launch hook (injectable — the tests record instead of exec) */
typedef int (*BpmLaunchFn)(int idx);

/* BP1: init the shell. */
void bpm_init(void);

/* BP2: the launch hook override. */
void bpm_set_launch(BpmLaunchFn fn);

/* BP3: open / close / is-open. */
void bpm_open(void);
void bpm_close(void);
int  bpm_is_open(void);

/* BP4: the grid rows. */
int bpm_rows(void);

/* BP5: the gamepad navigation (d-pad + A/B). Returns 1 if consumed. */
int bpm_input(uint32_t key, uint32_t mods, int down);

/* BP6: the current selection. */
int bpm_selection(void);

/* BP7: the test hooks. */
typedef struct {
    int open;
    int selection;
    int rows;
} bpm_view_t;
int bpm_get(bpm_view_t *out);

#endif
