/*
 * wubu_console.h -- live ring-0 console REPL (TempleOS-style).
 *
 * The metal kernel boots to a live interactive shell on COM1: type a
 * command (or HolyC once the compiler is ported), the kernel executes it
 * in ring 0, output returns on the same line.  This is the live
 * development environment the OS is built around -- the AGI (Colonel)
 * drives the same wubu_console_exec() entry the serial task uses.
 */
#ifndef WUBU_CONSOLE_H
#define WUBU_CONSOLE_H

/* Execute one command line (no trailing newline).  Returns 0 if handled. */
int wubu_console_exec(const char *line);

/* The console task entry (poll serial, echo, dispatch). Never returns. */
void wubu_console_task(void *arg);

/* Print the prompt. */
void wubu_console_prompt(void);

#endif /* WUBU_CONSOLE_H */
