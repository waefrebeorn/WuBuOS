/*
 * wubu_walkthrough.h -- the first-run walkthrough.
 *
 * Opens the Getting Started walkthrough: four-plus steps, each with a
 * pose from the official clipart pack, real buttons (Back / Next /
 * Finish) and a step indicator.
 *
 * C11, opaque API, no god headers.
 */

#ifndef WUBU_WALKTHROUGH_H
#define WUBU_WALKTHROUGH_H

/* Open the walkthrough window. Returns 1 if it opened, 0 on failure. */
int wubu_walkthrough_run(void);

/* Number of steps (hosts may render their own chrome). */
int wubu_walkthrough_steps(void);

#endif /* WUBU_WALKTHROUGH_H */
