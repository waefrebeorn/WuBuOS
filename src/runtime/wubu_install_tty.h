/*
 * wubu_install_tty.h -- a stdio front end for the configurator (P1 OOBE).
 *
 * wubu_install_wizard asks through injected I/O; this is the first real
 * front end: prompts to stdout, answers from stdin, the password typed with
 * echo turned off where the termios API is available (POSIX; Windows falls
 * back to an echoed read -- a real install injects a masked GUI/console UI).
 * On success the /n/install answers document is written to `answers_path`.
 *
 * Pure C11 + stdio. Same portability rules as wubu_install: no egojar/sys.
 */
#ifndef WUBU_INSTALL_TTY_H
#define WUBU_INSTALL_TTY_H

#include "wubu_install.h"

/* Run the wizard over stdin/stdout and, if the user confirms, write the
 * serialized plan to `answers_path`. Returns 1 = confirmed + written,
 * 0 = cancelled, -1 = invalid answer or write failure. */
int wubu_install_run_tty(wubu_install_plan *plan, const char *answers_path);

#endif /* WUBU_INSTALL_TTY_H */
