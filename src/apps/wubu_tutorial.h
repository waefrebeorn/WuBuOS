/*
 * wubu_tutorial.h -- interactive, in-shell tutorials.
 *
 * A tutorial is a .wet applet (Lua) that drives the same WET tree the apps it
 * teaches use.  The host supplies tut.* : a list of steps, Next/Back, and
 * "Try it" actions that launch the real thing.
 *
 * This is onboarding that teaches by DOING, not by reading: a step says "open
 * the designer", the button opens the designer, and the user sees the real
 * tool -- not a screenshot of one.
 */
#ifndef WUBU_TUTORIAL_H
#define WUBU_TUTORIAL_H

#include "../gui/wm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open a tutorial by name.  The script lives in src/apps/tutorials/<name>.wet.
 * Returns NULL if the tutorial is missing or fails to run. */
DosGuiWindow *wubu_tutorial_open(const char *name);

/* Launch the tutorial browser: the list of shipped tutorials, each one click. */
DosGuiWindow *wubu_tutorial_browser(void);

/* The last error, or NULL. */
const char *wubu_tutorial_error(void);

/* The shipped tutorial names, for the browser and for tests. */
int wubu_tutorial_count(void);
const char *wubu_tutorial_name(int i);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_TUTORIAL_H */
