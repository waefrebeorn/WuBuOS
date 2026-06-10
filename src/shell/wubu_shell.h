/*
 * wubu_shell.h — WuBuOS Unified GUI Shell Header
 *
 * Cell 207: Integration — runs the Win98 GUI shell on any platform backend.
 */

#ifndef WUBU_SHELL_H
#define WUBU_SHELL_H

#include <stdint.h>

/* Shell entry point — runs the full Win98 GUI shell */
int wubu_shell_run(int width, int height);

/* Shell shutdown */
void wubu_shell_shutdown(void);

#endif /* WUBU_SHELL_H */