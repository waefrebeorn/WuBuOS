/* wubu_wemu_bridge.c — WEMU user-mode <-> VSL Linux personality bridge.
 *
 * ONE stack: WEMU interprets guest code (true sandbox), every guest
 * syscall goes through vsl_syscall_dispatch() — the single VSL table,
 * Linux personality selected. No second syscall table, no second
 * decoder. The bridge owns exactly the user-mode emulator's share:
 *
 *   1. guest<->host ABI marshalling: VSL handlers are host-backed and
 *      take HOST pointers, so guest pointers are bounced through host
 *      scratch (the qemu-user lock_user/unlock_user role), and results
 *      are copied back into guest RAM.
 *   2. guest architectural state: arch_prctl sets the WEMU FS/GS base
 *      (never the host's — that would corrupt the host TLS); exit /
 *      exit_group halt the CPU and record the code.
 *   3. guest memory management (mmap/brk/munmap) passes straight into
 *      VSL's virtual user arena (VSL_USER_BASE..): those addresses are
 *      guest addresses by construction, inside the 64MB guest RAM.
 *
 * License: WaefreBeorn Umbrella License v3.0. */
#include "wubu_wemu_bridge.h"
#include "wemu.h"
#include "wubu_vsl.h"
#include "wubu_kernel_xchg.h"
#include "vsl/vsl_syscall.h"
#include "vsl/vsl_prot.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/uio.h>

#define BRIDGE_RAM (64ull << 20)
#define MAX_INSNS  500000000ull
#define BOUNCE_MAX (1ull << 20)

typedef struct {
    WemuCpu *cpu;
    wubu_wemu_capture *cap;
} BridgeCtx;

/* ---- capture ------------------------------------------------------ */

void wubu_wemu_capture_init(wubu_wemu_capture *cap) {
    memset(cap, 0, sizeof *cap);
}

void wubu_wemu_capture_free(wubu_wemu_capture *cap) {
    if (!cap) return;
    free(cap->out);
    free(cap->err);
    memset(cap, 0, sizeof *cap);
}

static void cap_append(char **buf, size_t *len, size_t *cap,
                       const char *data, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap : 4096;
        while (nc < *len + n + 1) nc *= 2;
        char *nb = realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = 0;
}

static void record_missing(BridgeCtx *ctx, uint64_t nr) {
    wubu_wemu_capture *cap = ctx->cap;
    if (!cap) return;
    for (size_t i = 0; i < cap->n_missing; i++)
        if (cap->missing_nrs[i] == nr) return;
    if (cap->n_missing < 64) cap->missing_nrs[cap->n_missing++] = nr;
}

/* ---- guest memory helpers ----------------------------------------- */

static int gread(WemuCpu *cpu, uint64_t va, void *dst, size_t n) {
    return wemu_mem_read(cpu, va, dst, n);
}

static int gwrite(WemuCpu *cpu, uint64_t va, const void *src, size_t n) {
    return wemu_mem_write(cpu, va, src, n);
}

/* guest C string -> host buffer; -EFAULT if unterminated/out of RAM */
static int gstr(WemuCpu *cpu, uint64_t va, char *dst, size_t cap) {
    for (size_t i = 0; i < cap - 1; i++) {
        char c;
        if (gread(cpu, va + i, &c, 1) != 0) return -14;
        dst[i] = c;
        if (!c) return 0;
    }
    return -14;
}

/* ---- VSL protection hooks ------------------------------------------- */

/* Data access: WEMU calls this before every guest load (is_write=0) and
 * store (is_write=1). vsl_prot_allows() reads the SINGLE owner table (VSL's
 * mmap/mprot records); a legacy/unmanaged VA is always allowed (no
 * regression). PROT_NONE has no READ/WRITE bit, so it blocks both. */
int wubu_wemu_prot_hook(uint64_t va, size_t len, int is_write, void *ud) {
    (void)len; (void)ud;
    int bit = is_write ? VSL_PROT_WRITE : VSL_PROT_READ;
    return vsl_prot_allows(va, bit) ? 0 : -1;
}

/* Instruction fetch: the decoder consults this at RIP before fetching the
 * first byte. Same SINGLE owner table; a VSL page without PROT_EXEC faults,
 * legacy guest code stays executable. Narrow: checks the page holding RIP. */
int wubu_wemu_fetch_hook(uint64_t va, size_t len, void *ud) {
    (void)len; (void)ud;
    return vsl_prot_allows(va, VSL_PROT_EXEC) ? 0 : -1;
}

/* ---- the syscall hook --------------------------------------------- */

static int64_t bridge_syscall_inner(WemuCpu *cpu, uint64_t nr,
                              const uint64_t args[6], void *ud) {
    BridgeCtx *ctx = ud;
    wubu_wemu_capture *cap = ctx->cap;
    if (cap) cap->syscalls++;

    /* exit/exit_group: halt the guest, record the code. The VSL exit
     * handler also marks the VSL process zombie — dispatch first so
     * the process state stays truthful, then halt. */
    if (nr == 60 || nr == 231) {
        uint64_t regs[6] = { args[0], 0, 0, 0, 0, 0 };
        vsl_syscall_dispatch(nr, regs);
        if (cap) { cap->exit_code = (int)(args[0] & 0xff); cap->exited = 1; }
        wemu_cpu_halt(cpu);
        return 0;
    }

    /* arch_prctl: guest architectural state, owned by the emulator.
     * Setting the HOST FS/GS here would corrupt host libc TLS. */
    if (nr == 158) {
        switch (args[0]) {
        case 0x1002: wemu_cpu_set_fs_base(cpu, args[1]); return 0;
        case 0x1001: wemu_cpu_set_gs_base(cpu, args[1]); return 0;
        case 0x1003: {
            uint64_t v = wemu_cpu_fs_base(cpu);
            return gwrite(cpu, args[1], &v, 8) == 0 ? 0 : -14;
        }
        case 0x1004: {
            uint64_t v = wemu_cpu_gs_base(cpu);
            return gwrite(cpu, args[1], &v, 8) == 0 ? 0 : -14;
        }
        default: return -22; /* EINVAL */
        }
    }

    uint64_t regs[6] = { args[0], args[1], args[2], args[3], args[4], args[5] };
    char path[4096], path2[4096];
    int64_t rv;

    switch (nr) {
    case 1: { /* write(fd, buf, count): bounce in, tee fd 1/2 */
        if (args[2] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[2] ? (size_t)args[2] : 1);
        if (!buf) return -12;
        if (gread(cpu, args[1], buf, (size_t)args[2]) != 0) {
            free(buf); return -14;
        }
        regs[1] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && cap && (args[0] == 1 || args[0] == 2)) {
            if (args[0] == 1)
                cap_append(&cap->out, &cap->out_len, &cap->out_cap, buf, (size_t)rv);
            else
                cap_append(&cap->err, &cap->err_len, &cap->err_cap, buf, (size_t)rv);
        }
        free(buf);
        return rv;
    }
    case 0: { /* read(fd, buf, count): bounce out */
        if (args[2] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[2] ? (size_t)args[2] : 1);
        if (!buf) return -12;
        regs[1] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[1], buf, (size_t)rv) != 0) rv = -14;
        free(buf);
        return rv;
    }
    case 20: case 19: { /* writev / readv: marshal the iov array */
        uint64_t niov = args[2];
        if (niov > 1024) return -22;
        struct iovec *hiov = calloc((size_t)niov ? (size_t)niov : 1,
                                    sizeof *hiov);
        if (!hiov) return -12;
        size_t total = 0;
        int bad = 0;
        for (uint64_t i = 0; i < niov; i++) {
            uint64_t gv[2];
            if (gread(cpu, args[1] + i * 16, gv, 16) != 0) { bad = 1; break; }
            if (gv[1] > BOUNCE_MAX || total + gv[1] > BOUNCE_MAX) { bad = 1; break; }
            hiov[i].iov_len = (size_t)gv[1];
            hiov[i].iov_base = malloc(gv[1] ? (size_t)gv[1] : 1);
            if (!hiov[i].iov_base) { bad = 1; break; }
            if (nr == 20 && gv[1] &&
                gread(cpu, gv[0], hiov[i].iov_base, (size_t)gv[1]) != 0) {
                bad = 1; break;
            }
            total += gv[1];
        }
        if (bad) {
            for (uint64_t i = 0; i < niov; i++) free(hiov[i].iov_base);
            free(hiov);
            return -14;
        }
        regs[1] = (uint64_t)(uintptr_t)hiov;
        rv = vsl_syscall_dispatch(nr, regs);
        if (nr == 19 && rv > 0) {
            int64_t left = rv;
            for (uint64_t i = 0; i < niov && left > 0; i++) {
                uint64_t gv[2];
                gread(cpu, args[1] + i * 16, gv, 16);
                size_t n = (size_t)left < hiov[i].iov_len
                         ? (size_t)left : hiov[i].iov_len;
                if (n && gwrite(cpu, gv[0], hiov[i].iov_base, n) != 0) {
                    rv = -14; break;
                }
                left -= (int64_t)n;
            }
        }
        if (nr == 20 && rv > 0 && cap && (args[0] == 1 || args[0] == 2)) {
            int64_t left = rv;
            for (uint64_t i = 0; i < niov && left > 0; i++) {
                size_t n = (size_t)left < hiov[i].iov_len
                         ? (size_t)left : hiov[i].iov_len;
                if (args[0] == 1)
                    cap_append(&cap->out, &cap->out_len, &cap->out_cap,
                               hiov[i].iov_base, n);
                else
                    cap_append(&cap->err, &cap->err_len, &cap->err_cap,
                               hiov[i].iov_base, n);
                left -= (int64_t)n;
            }
        }
        for (uint64_t i = 0; i < niov; i++) free(hiov[i].iov_base);
        free(hiov);
        return rv;
    }
    case 2: case 85: case 257: /* open / creat / openat: path in */
        if (nr == 257) {
            if (gstr(cpu, args[1], path, sizeof path) != 0) return -14;
            regs[1] = (uint64_t)(uintptr_t)path;
        } else {
            if (gstr(cpu, args[0], path, sizeof path) != 0) return -14;
            regs[0] = (uint64_t)(uintptr_t)path;
        }
        return vsl_syscall_dispatch(nr, regs);
    case 4: case 6: { /* stat / lstat: path in, struct stat out */
        if (gstr(cpu, args[0], path, sizeof path) != 0) return -14;
        struct stat st;
        regs[0] = (uint64_t)(uintptr_t)path;
        regs[1] = (uint64_t)(uintptr_t)&st;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[1] && gwrite(cpu, args[1], &st, sizeof st) != 0)
            return -14;
        return rv;
    }
    case 5: { /* fstat: struct stat out */
        struct stat st;
        regs[1] = (uint64_t)(uintptr_t)&st;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[1] && gwrite(cpu, args[1], &st, sizeof st) != 0)
            return -14;
        return rv;
    }
    case 262: { /* newfstatat(dirfd, path, statbuf, flags) */
        if (gstr(cpu, args[1], path, sizeof path) != 0) return -14;
        struct stat st;
        regs[1] = (uint64_t)(uintptr_t)path;
        regs[2] = (uint64_t)(uintptr_t)&st;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[2] && gwrite(cpu, args[2], &st, sizeof st) != 0)
            return -14;
        return rv;
    }
    case 332: { /* statx(dirfd, path, flags, mask, buf 256) */
        if (gstr(cpu, args[1], path, sizeof path) != 0) return -14;
        uint8_t sx[256];
        memset(sx, 0, sizeof sx);
        regs[1] = (uint64_t)(uintptr_t)path;
        regs[4] = (uint64_t)(uintptr_t)sx;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[4] && gwrite(cpu, args[4], sx, sizeof sx) != 0)
            return -14;
        return rv;
    }
    case 217: { /* getdents64(fd, buf, count): bounce out */
        if (args[2] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[2] ? (size_t)args[2] : 1);
        if (!buf) return -12;
        regs[1] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[1], buf, (size_t)rv) != 0) rv = -14;
        free(buf);
        return rv;
    }
    case 63: { /* uname(buf) */
        struct utsname u;
        regs[0] = (uint64_t)(uintptr_t)&u;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[0] && gwrite(cpu, args[0], &u, sizeof u) != 0)
            return -14;
        return rv;
    }
    case 99: { /* sysinfo */
        struct sysinfo si;
        regs[0] = (uint64_t)(uintptr_t)&si;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[0] && gwrite(cpu, args[0], &si, sizeof si) != 0)
            return -14;
        return rv;
    }
    case 16: { /* ioctl(fd, req, arg): bounce a small struct in+out */
        uint8_t io[256];
        memset(io, 0, sizeof io);
        if (args[2] && gread(cpu, args[2], io, sizeof io) != 0) return -14;
        regs[2] = (uint64_t)(uintptr_t)io;
        rv = vsl_syscall_dispatch(nr, regs);
        if (args[2] && gwrite(cpu, args[2], io, sizeof io) != 0) return -14;
        return rv;
    }
    case 318: { /* getrandom(buf, len, flags) */
        if (args[1] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[1] ? (size_t)args[1] : 1);
        if (!buf) return -12;
        regs[0] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[0], buf, (size_t)rv) != 0) rv = -14;
        free(buf);
        return rv;
    }
    case 96: { /* gettimeofday(tv, tz) */
        uint8_t tv[16], tz[8];
        regs[0] = args[0] ? (uint64_t)(uintptr_t)tv : 0;
        regs[1] = args[1] ? (uint64_t)(uintptr_t)tz : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0) {
            if (args[0] && gwrite(cpu, args[0], tv, sizeof tv) != 0) return -14;
            if (args[1] && gwrite(cpu, args[1], tz, sizeof tz) != 0) return -14;
        }
        return rv;
    }
    case 201: { /* time(tloc): returns seconds; tloc optional out */
        int64_t t = time(NULL);
        if (args[0] && gwrite(cpu, args[0], &t, 8) != 0) return -14;
        return t;
    }
    case 228: { /* clock_gettime(clk, tp) */
        uint8_t tp[16];
        regs[1] = (uint64_t)(uintptr_t)tp;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[1] && gwrite(cpu, args[1], tp, sizeof tp) != 0)
            return -14;
        return rv;
    }
    case 35: { /* nanosleep(req, rem) */
        uint8_t req[16], rem[16];
        if (gread(cpu, args[0], req, sizeof req) != 0) return -14;
        regs[0] = (uint64_t)(uintptr_t)req;
        regs[1] = args[1] ? (uint64_t)(uintptr_t)rem : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (args[1] && gwrite(cpu, args[1], rem, sizeof rem) != 0) return -14;
        return rv;
    }
    case 97: case 160: { /* getrlimit / setrlimit: 16-byte rlimit.
        * x86-64: 97=getrlimit, 160=setrlimit (the old code treated 98
        * as setrlimit — 98 is getrusage, an i386 relic). */
        uint8_t rl[16];
        if (nr == 160 && gread(cpu, args[1], rl, sizeof rl) != 0) return -14;
        regs[1] = (uint64_t)(uintptr_t)rl;
        rv = vsl_syscall_dispatch(nr, regs);
        if (nr == 97 && rv == 0 && args[1] &&
            gwrite(cpu, args[1], rl, sizeof rl) != 0) return -14;
        return rv;
    }
    case 98: { /* getrusage(who, usage): 144-byte struct out */
        uint8_t ru[144];
        regs[1] = (uint64_t)(uintptr_t)ru;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[1] && gwrite(cpu, args[1], ru, sizeof ru) != 0)
            return -14;
        return rv;
    }
    case 17: case 18: { /* pread64 / pwrite64: bounce like read/write */
        if (args[2] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[2] ? (size_t)args[2] : 1);
        if (!buf) return -12;
        if (nr == 18 && gread(cpu, args[1], buf, (size_t)args[2]) != 0) {
            free(buf); return -14;
        }
        regs[1] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (nr == 17 && rv > 0 && gwrite(cpu, args[1], buf, (size_t)rv) != 0)
            rv = -14;
        free(buf);
        return rv;
    }
    case 22: case 293: { /* pipe / pipe2: int[2] out */
        int fds[2];
        regs[0] = (uint64_t)(uintptr_t)fds;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && gwrite(cpu, args[0], fds, sizeof fds) != 0) return -14;
        return rv;
    }
    case 100: { /* times(tbuf): 4x clock_t out */
        uint8_t tms[32];
        regs[0] = (uint64_t)(uintptr_t)tms;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv != -1 && args[0] && gwrite(cpu, args[0], tms, sizeof tms) != 0)
            return -14;
        return rv;
    }
    case 302: { /* prlimit64(pid, res, new, old) */
        uint8_t nw[16], od[16];
        memset(nw, 0, sizeof nw); memset(od, 0, sizeof od);
        if (args[2] && gread(cpu, args[2], nw, sizeof nw) != 0) return -14;
        regs[2] = args[2] ? (uint64_t)(uintptr_t)nw : 0;
        regs[3] = args[3] ? (uint64_t)(uintptr_t)od : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0 && args[3] && gwrite(cpu, args[3], od, sizeof od) != 0)
            return -14;
        return rv;
    }
    case 13: { /* rt_sigaction(sig, act, oldact, sigsetsize) */
        uint8_t act[32], old[32];
        memset(act, 0, sizeof act); memset(old, 0, sizeof old);
        if (args[1] && gread(cpu, args[1], act, sizeof act) != 0) return -14;
        regs[1] = args[1] ? (uint64_t)(uintptr_t)act : 0;
        regs[2] = args[2] ? (uint64_t)(uintptr_t)old : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (args[2] && gwrite(cpu, args[2], old, sizeof old) != 0) return -14;
        return rv;
    }
    case 14: { /* rt_sigprocmask(how, set, oldset, sigsetsize) */
        uint8_t set[8], old[8];
        memset(set, 0, sizeof set); memset(old, 0, sizeof old);
        if (args[1] && gread(cpu, args[1], set, sizeof set) != 0) return -14;
        regs[1] = args[1] ? (uint64_t)(uintptr_t)set : 0;
        regs[2] = args[2] ? (uint64_t)(uintptr_t)old : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (args[2] && gwrite(cpu, args[2], old, sizeof old) != 0) return -14;
        return rv;
    }
    case 131: { /* sigaltstack(ss, old_ss): 24-byte stack_t */
        uint8_t ss[24], old[24];
        memset(ss, 0, sizeof ss); memset(old, 0, sizeof old);
        if (args[0] && gread(cpu, args[0], ss, sizeof ss) != 0) return -14;
        regs[0] = args[0] ? (uint64_t)(uintptr_t)ss : 0;
        regs[1] = args[1] ? (uint64_t)(uintptr_t)old : 0;
        rv = vsl_syscall_dispatch(nr, regs);
        if (args[1] && gwrite(cpu, args[1], old, sizeof old) != 0) return -14;
        return rv;
    }
    case 274: { /* get_robust_list(pid, head_ptr, len_ptr) */
        uint64_t head = 0, len = 0;
        regs[1] = (uint64_t)(uintptr_t)&head;
        regs[2] = (uint64_t)(uintptr_t)&len;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == 0) {
            if (args[1] && gwrite(cpu, args[1], &head, 8) != 0) return -14;
            if (args[2] && gwrite(cpu, args[2], &len, 8) != 0) return -14;
        }
        return rv;
    }
    case 115: { /* getgroups(size, list): bounce the gid array out.
        * size==0 asks for the count only (no pointer). */
        if (args[0] == 0) return vsl_syscall_dispatch(nr, regs);
        if (args[0] > 65536) return -22;
        gid_t *gl = malloc((size_t)args[0] * sizeof *gl);
        if (!gl) return -12;
        regs[1] = (uint64_t)(uintptr_t)gl;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[1], gl, (size_t)rv * sizeof *gl) != 0)
            rv = -14;
        free(gl);
        return rv;
    }
    case 79: { /* getcwd(buf, size) */
        if (args[1] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[1] ? (size_t)args[1] : 1);
        if (!buf) return -12;
        regs[0] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[0], buf, (size_t)rv) != 0) rv = -14;
        free(buf);
        return rv;
    }
    case 89: case 267: { /* readlink / readlinkat */
        uint64_t pidx = nr == 89 ? 0 : 1;
        uint64_t bidx = nr == 89 ? 1 : 2;
        uint64_t sidx = nr == 89 ? 2 : 3;
        if (gstr(cpu, args[pidx], path, sizeof path) != 0) return -14;
        if (args[sidx] > BOUNCE_MAX) return -22;
        char *buf = malloc((size_t)args[sidx] ? (size_t)args[sidx] : 1);
        if (!buf) return -12;
        regs[pidx] = (uint64_t)(uintptr_t)path;
        regs[bidx] = (uint64_t)(uintptr_t)buf;
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv > 0 && gwrite(cpu, args[bidx], buf, (size_t)rv) != 0) rv = -14;
        free(buf);
        return rv;
    }
    case 21: case 269: /* access / faccessat */
    case 80:           /* chdir */
    case 83: case 258: /* mkdir / mkdirat */
    case 87: case 263: /* unlink / unlinkat */
    case 90: case 270: /* chmod / fchmodat */
    case 92: case 260: /* chown / fchownat */
    case 76:           /* truncate */
    case 84:           /* rmdir */
    case 94:           /* lchown */
    {
        uint64_t pidx = (nr == 269 || nr == 258 || nr == 263 ||
                         nr == 270 || nr == 260) ? 1 : 0;
        if (gstr(cpu, args[pidx], path, sizeof path) != 0) return -14;
        regs[pidx] = (uint64_t)(uintptr_t)path;
        return vsl_syscall_dispatch(nr, regs);
    }
    case 82: case 264: case 316: case 86: { /* rename family + link */
        uint64_t a = (nr == 82 || nr == 86) ? 0 : 1,
                 b = (nr == 82 || nr == 86) ? 1 : 3;
        if (gstr(cpu, args[a], path, sizeof path) != 0) return -14;
        if (gstr(cpu, args[b], path2, sizeof path2) != 0) return -14;
        regs[a] = (uint64_t)(uintptr_t)path;
        regs[b] = (uint64_t)(uintptr_t)path2;
        return vsl_syscall_dispatch(nr, regs);
    }
    case 88: case 266: { /* symlink / symlinkat */
        uint64_t a = 0, b = nr == 88 ? 1 : 2;
        if (gstr(cpu, args[a], path, sizeof path) != 0) return -14;
        if (gstr(cpu, args[b], path2, sizeof path2) != 0) return -14;
        regs[a] = (uint64_t)(uintptr_t)path;
        regs[b] = (uint64_t)(uintptr_t)path2;
        return vsl_syscall_dispatch(nr, regs);
    }
    case 280: { /* utimensat(dirfd, path, times[2], flags): touch/cp -p.
        * path may be NULL (futimens semantics on dirfd); times NULL
        * means "now". Bounce the 32-byte timespec array guest->host. */
        if (args[1]) {
            if (gstr(cpu, args[1], path, sizeof path) != 0) return -14;
            regs[1] = (uint64_t)(uintptr_t)path;
        }
        struct timespec ts[2];
        if (args[2]) {
            if (gread(cpu, args[2], ts, sizeof ts) != 0) return -14;
            regs[2] = (uint64_t)(uintptr_t)ts;
        }
        return vsl_syscall_dispatch(nr, regs);
    }
    case 235: case 261: { /* utimes(path, tv[2]) / futimesat(dirfd,path,tv) */
        uint64_t pidx = nr == 235 ? 0 : 1;
        if (gstr(cpu, args[pidx], path, sizeof path) != 0) return -14;
        regs[pidx] = (uint64_t)(uintptr_t)path;
        struct timeval tv[2];
        if (args[pidx + 1]) {
            if (gread(cpu, args[pidx + 1], tv, sizeof tv) != 0) return -14;
            regs[pidx + 1] = (uint64_t)(uintptr_t)tv;
        }
        return vsl_syscall_dispatch(nr, regs);
    }
    default: {
        /* FAIL CLOSED (security audit 2026-11-19): the old default
         * passed EVERY unlisted syscall straight to the host handler
         * with GUEST pointers intact — any pointer-taking syscall not
         * explicitly marshalled above would read/write guest addresses
         * as host pointers (silent corruption where they happen to be
         * mapped). Only scalar-arg syscalls (and the two that STORE a
         * guest pointer without dereferencing it) pass through now;
         * everything else is honest -ENOSYS until marshalled + gated. */
        static const uint16_t scalar_ok[] = {
            3, 8, 9, 10, 11, 12, 24, 28, 32, 33, 39, 41, 48, 50,
            62, 72, 74, 75, 77, 80, 81, 91, 93, 95, 102, 104, 105,
            106, 107, 108, 110, 111, 113, 114, 117, 119, 122, 123,
            124, 146, 147, 149, 150, 151, 152, 162, 186, 200, 213,
            218, 219, 234, 253, 272, 273, 276, 277, 283, 284, 285,
            290, 291, 292, 294, 306, 308, 312, 323, 324, 325, 330,
            331, 436, 438, 446, 447, 448, 450,
        };
        int ok = 0;
        for (size_t i = 0; i < sizeof scalar_ok / sizeof scalar_ok[0]; i++)
            if (scalar_ok[i] == nr) { ok = 1; break; }
        if (!ok) { record_missing(ctx, nr); return -38; }
        rv = vsl_syscall_dispatch(nr, regs);
        if (rv == -38) record_missing(ctx, nr);
        return rv;
    }
    }
}

/* ---- public entry -------------------------------------------------- */

/* Wrapper: a marshalling failure inside the hook sets cpu->fault via
 * wemu_mem_read/write; that is a bad GUEST POINTER translated to
 * -EFAULT, not an exception the guest architecturally took — clear it
 * so the guest continues with the errno (real user-mode semantics). */
static int64_t bridge_syscall(WemuCpu *cpu, uint64_t nr,
                              const uint64_t args[6], void *ud) {
    if (getenv("WEMU_BRIDGE_TRACE"))
        fprintf(stderr, "TRACE syscall nr=%llu args=%llx %llx %llx %llx %llx %llx rip=%llx\n",
                (unsigned long long)nr,
                (unsigned long long)args[0], (unsigned long long)args[1],
                (unsigned long long)args[2], (unsigned long long)args[3],
                (unsigned long long)args[4], (unsigned long long)args[5],
                (unsigned long long)wemu_cpu_rip(cpu));
    int f0 = wemu_cpu_fault(cpu);
    int64_t rv = bridge_syscall_inner(cpu, nr, args, ud);
    if (!f0 && wemu_cpu_fault(cpu)) wemu_cpu_clear_fault(cpu);
    return rv;
}

int wubu_wemu_run_elf(const char *path, char *const argv[],
                      wubu_wemu_capture *cap) {
    if (cap) wubu_wemu_capture_init(cap);
    if (vsl_init() != 0) return -1;
    wubu_kernel_select("linux");   /* the bare-number range is Linux's */

    WemuCpu *cpu = wemu_cpu_create(BRIDGE_RAM);
    if (!cpu) return -1;
    wemu_set_mem_perm_hook(wubu_wemu_prot_hook, NULL);
    wemu_set_fetch_perm_hook(wubu_wemu_fetch_hook, NULL);
    if (wemu_cpu_load_elf(cpu, path, NULL, 0, argv) != 0) {
        wemu_cpu_destroy(cpu);
        return -1;
    }
    BridgeCtx ctx = { cpu, cap };
    wemu_cpu_set_syscall_hook(cpu, bridge_syscall, &ctx);
    int64_t n;
    const char *tr = getenv("WEMU_BRIDGE_TRACE");
    if (tr && tr[0] == '2') {
        /* ring of recent rips, dumped at exit for loop analysis */
        enum { RING = 400000 };
        uint64_t *ring = calloc(RING, 8);
        uint64_t rn = 0;
        n = 0;
        while ((uint64_t)n < MAX_INSNS && !wemu_cpu_halted(cpu) &&
               !wemu_cpu_fault(cpu)) {
            ring[rn % RING] = wemu_cpu_rip(cpu);
            rn++;
            if (wemu_cpu_step(cpu) < 0) break;
            n++;
        }
        FILE *f = fopen("/tmp/wemu_rips.txt", "w");
        if (f) {
            uint64_t start = rn > RING ? rn - RING : 0;
            for (uint64_t i = start; i < rn; i++)
                fprintf(f, "%llx\n", (unsigned long long)ring[i % RING]);
            fclose(f);
        }
        free(ring);
    } else if (tr) {
        n = 0;
        while ((uint64_t)n < MAX_INSNS && !wemu_cpu_halted(cpu) &&
               !wemu_cpu_fault(cpu)) {
            uint64_t rip = wemu_cpu_rip(cpu);
            if (wemu_cpu_step(cpu) < 0) {
                uint8_t b[16] = {0};
                wemu_mem_read(cpu, rip, b, 16);
                fprintf(stderr, "TRACE fault at rip=%llx insn#%lld bytes:",
                        (unsigned long long)rip, (long long)n);
                for (int k = 0; k < 16; k++) fprintf(stderr, " %02x", b[k]);
                fprintf(stderr, "\nTRACE regs:");
                for (int k = 0; k < 16; k++)
                    fprintf(stderr, " r%d=%llx", k,
                            (unsigned long long)wemu_cpu_reg(cpu, k));
                fprintf(stderr, "\n");
                break;
            }
            n++;
        }
    } else {
        n = wemu_cpu_run(cpu, MAX_INSNS);
    }
    if (cap) {
        cap->insns = (uint64_t)(n < 0 ? 0 : n);
        cap->fault = wemu_cpu_fault(cpu);
        cap->fault_rip = wemu_cpu_rip(cpu);
    }
    if (getenv("WEMU_BRIDGE_GOT")) {
        for (uint64_t a = 0x603e00; a <= 0x603f28; a += 8) {
            uint64_t v = 0;
            wemu_mem_read(cpu, a, &v, 8);
            fprintf(stderr, "GOT %llx -> %llx\n",
                    (unsigned long long)a, (unsigned long long)v);
        }
    }
    int rc = (cap && cap->exited) ? 0 : -1;
    wemu_cpu_destroy(cpu);
    return rc;
}