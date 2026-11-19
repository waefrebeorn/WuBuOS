/* wubu_wemu_bridge.h — WEMU user-mode <-> VSL Linux personality bridge.
 *
 * The FEX-Emu/box64/qemu-user fast path, our own: WEMU interprets the
 * guest (true sandbox — guest bytes never execute natively), and every
 * guest syscall dispatches into the ONE VSL Linux table via
 * vsl_syscall_dispatch(). The bridge owns only what a user-mode
 * emulator must own: the guest<->host ABI boundary (pointer marshalling)
 * and guest architectural state (FS/GS bases, halt on exit).
 *
 * License: WaefreBeorn Umbrella License v3.0. */
#ifndef WUBU_WEMU_BRIDGE_H
#define WUBU_WEMU_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Captured guest behavior. out/err accumulate guest writes to fd 1/2
 * (the VSL write handler ALSO passes them through to the host fds).
 * missing_nrs records every nr the VSL table answered -ENOSYS for. */
typedef struct {
    char *out;  size_t out_len, out_cap;
    char *err;  size_t err_len, err_cap;
    int exit_code;
    int exited;                 /* guest called exit/exit_group */
    int fault;                  /* x86 vector, 0 = clean */
    uint64_t fault_rip;
    uint64_t insns;             /* instructions executed */
    uint64_t syscalls;          /* guest syscalls dispatched */
    uint64_t missing_nrs[64];
    size_t n_missing;
} wubu_wemu_capture;

void wubu_wemu_capture_init(wubu_wemu_capture *cap);
void wubu_wemu_capture_free(wubu_wemu_capture *cap);

/* Run a static x86-64 ELF under WEMU with the Linux personality.
 * argv is NULL-terminated (argv[0] should be the program name).
 * Returns 0 when the guest exited via exit/exit_group (code in
 * cap->exit_code), -1 on load failure or a guest CPU fault (details
 * in cap). cap may be NULL. */
int wubu_wemu_run_elf(const char *path, char *const argv[],
                      wubu_wemu_capture *cap);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_WEMU_BRIDGE_H */