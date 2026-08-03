/*
 * tandem.h -- Tandem: the user+AGI shared desktop window.
 */
#ifndef TANDEM_H
#define TANDEM_H

typedef struct DosGuiWindow DosGuiWindow;

DosGuiWindow *tandem_launch(void);

/* hosted test hooks (no GUI needed): advance the timing loop. */
int tandem_test_tick(int n);
int tandem_test_proposals(void);
int tandem_test_events(void);

#endif
