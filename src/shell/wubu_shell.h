/*
 * wubu_shell.h -- WuBuOS command shell (public API)
 *
 * A real, line-editing command shell for the hosted (Linux) context:
 * command history, tab-completion, pipelines (|) and I/O redirection
 * (>, >>, <). This is the component that closes BATTLESHIP gaps 228-231
 * (shell_history / shell_completion / shell_pipe / shell_redirect) which
 * were previously pure form-without-function.
 *
 * The bare-metal kernel keeps its own minimal wubu_shell_run stub in
 * metal_main.c (no stdio/heap there); this hosted implementation is the
 * genuine one and is what `make shell` / `make test_shell` exercise.
 */

#ifndef WUBU_SHELL_H
#define WUBU_SHELL_H

#include <stdint.h>

/*
 * Shell task entry point. Signature matches the task_create() ABI
 * (void *arg) used by the kernel and by wubu_metal.c, so the weak hook
 * in the metal build resolves to this strong definition when linked.
 */
void wubu_shell_run(void *arg);

#endif /* WUBU_SHELL_H */
