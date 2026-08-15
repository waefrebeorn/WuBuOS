/* wubu_gnu_compat.h — WuBu-native replacements for _GNU_SOURCE symbols
 *
 * The user's directive: WuBu compliance means WE define the feature surface,
 * not glibc's _GNU_SOURCE macro. This header provides the tiny set of
 * symbols that test code needs WITHOUT reaching for _GNU_SOURCE:
 *
 *   - CPU_ZERO / CPU_SET / CPU_ISSET / CPU_COUNT  (sched.h)
 *   - CLONE_NEWNS / CLONE_NEWPID / CLONE_NEWNET / CLONE_NEWUSER /
 *     CLONE_NEWUTS / CLONE_NEWIPC                  (sched.h)
 *
 * FTW_* and DT_* constants live in wubu_ftw.h (include AFTER <ftw.h>/<dirent.h>).
 *
 * Include this header BEFORE any system header that might guard these
 * behind __USE_MISC / _GNU_SOURCE, or just include it first thing.
 *
 * Values are from the Linux kernel UAPI (asm-generic, linux/sched.h) —
 * they are OS ABI constants, not glibc inventions.
 */

#ifndef WUBU_GNU_COMPAT_H
#define WUBU_GNU_COMPAT_H

/* --- CPU affinity macros (from linux/sched.h / glibc sched.h) --- */
#ifndef CPU_SETSIZE
#  define CPU_SETSIZE 1024
#endif

/* CPU affinity macros — the type cpu_set_t comes from <sched.h> (system).
 * We only provide the macros here; if <sched.h> is not included, these
 * are harmless and the file won't be using CPU affinity anyway. */
#ifndef __CPU_SETSIZE
#  define __CPU_SETSIZE 1024
#endif
#ifndef __NCPUBITS
#  define __NCPUBITS (8 * sizeof(unsigned long))
#endif
#ifndef CPU_ZERO
#  define CPU_ZERO(cpusetp) \
    do { unsigned int __i; for (__i = 0; __i < __CPU_SETSIZE / __NCPUBITS; __i++) \
        (cpusetp)->__bits[__i] = 0; } while(0)
#endif
#ifndef CPU_SET
#  define CPU_SET(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu) / __NCPUBITS] |= (1UL << ((cpu) % __NCPUBITS)))
#endif
#ifndef CPU_CLR
#  define CPU_CLR(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu) / __NCPUBITS] &= ~(1UL << ((cpu) % __NCPUBITS)))
#endif
#ifndef CPU_ISSET
#  define CPU_ISSET(cpu, cpusetp) \
    !!((cpusetp)->__bits[(cpu) / __NCPUBITS] & (1UL << ((cpu) % __NCPUBITS)))
#endif
/* CPU_COUNT: count set bits in a cpuset using __builtin_popcountll.
 * No type dependency — works on any array of unsigned long. */
#ifndef CPU_COUNT
#  define CPU_COUNT(cpusetp) __extension__({ \
    int _n = 0; \
    for (unsigned int _i = 0; _i < (unsigned int)(__CPU_SETSIZE / __NCPUBITS); _i++) \
        _n += __builtin_popcountll((cpusetp)->__bits[_i]); \
    _n; })
#endif

/* --- Clone namespace flags (from linux/sched.h) --- */
#ifndef CLONE_NEWNS
#  define CLONE_NEWNS     0x00020000  /* New mount namespace */
#endif
#ifndef CLONE_NEWCGROUP
#  define CLONE_NEWCGROUP 0x02000000  /* New cgroup namespace */
#endif
#ifndef CLONE_NEWUTS
#  define CLONE_NEWUTS    0x04000000  /* New utsname namespace */
#endif
#ifndef CLONE_NEWIPC
#  define CLONE_NEWIPC    0x08000000  /* New ipc namespace */
#endif
#ifndef CLONE_NEWUSER
#  define CLONE_NEWUSER   0x10000000  /* New user namespace */
#endif
#ifndef CLONE_NEWPID
#  define CLONE_NEWPID    0x20000000  /* New pid namespace */
#endif
#ifndef CLONE_NEWNET
#  define CLONE_NEWNET    0x40000000  /* New network namespace */
#endif
#ifndef CLONE_NEWTIME
#  define CLONE_NEWTIME   0x00000080  /* New time namespace */
#endif

#endif /* WUBU_GNU_COMPAT_H */
