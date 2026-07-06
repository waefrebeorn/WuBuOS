/*
 * vsl.c  --  VSL Core Lifecycle & Diagnostics
 * This file contains the main VSL state and lifecycle functions.
 * All syscall implementations are in vsl_syscall.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "wubu_vsl.h"
#include "wubu_container.h"
#include "vsl/vsl_internal.h"
#include "vsl/vsl_syscall.h"
#include "vsl/vsl_gpu_vulkan.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/select.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <dlfcn.h>
#include <linux/if.h>
#include <linux/if_tun.h>

/* For new namespace/security syscalls */
#include <linux/landlock.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <sys/fanotify.h>
#include <sched.h>

/* -- Global State ------------------------------------------------ */

VSL_STATE g_vsl;

/* -- Lifecycle --------------------------------------------------- */

int vsl_init(void) {
    if (g_vsl.active) return 0;

    memset(&g_vsl, 0, sizeof(g_vsl));
    g_vsl.version_major = VSL_VERSION_MAJOR;
    g_vsl.version_minor = VSL_VERSION_MINOR;
    g_vsl.kernel_base = VSL_KERNEL_BASE;
    g_vsl.kernel_size = VSL_KERNEL_SIZE;
    g_vsl.user_base = VSL_USER_BASE;
    g_vsl.user_size = VSL_USER_SIZE;
    g_vsl.shared_base = VSL_SHARED_BASE;
    g_vsl.shared_size = VSL_SHARED_SIZE;

    /* Set up shared memory region  --  use heap for hosted tests */
    uint64_t *shared = (uint64_t *)calloc(4, sizeof(uint64_t));
    g_vsl.shared_cmd    = &shared[0];
    g_vsl.shared_arg    = &shared[1];
    g_vsl.shared_ret    = &shared[2];
    g_vsl.shared_status = &shared[3];

    /* Initialize shared memory */
    *g_vsl.shared_cmd = 0;
    *g_vsl.shared_arg = 0;
    *g_vsl.shared_ret = 0;
    *g_vsl.shared_status = 0;

    /* Create init process (PID 1) */
    VSL_PROC *init = &g_vsl.procs[0];
    memset(init, 0, sizeof(*init));
    init->pid = 1;
    init->ppid = 0;
    init->state = VSL_PROC_READY;
    init->entry_point = 0;
    init->stack_pointer = VSL_USER_BASE + VSL_USER_SIZE - 0x1000ULL;
    init->brk = VSL_USER_BASE + 0x100000; /* 1MB into user space */
    init->mmap_base = VSL_USER_BASE + 0x1000000; /* 16MB into user space */
    init->uid = (uint32_t)getuid();
    init->gid = (uint32_t)getgid();
    init->euid = (uint32_t)geteuid();
    init->egid = (uint32_t)getegid();
    init->pgid = -1;
    init->sesid = -1;
    init->umask = 0022;
    g_vsl.n_procs = 1;
    g_vsl.current_pid = 1;

    /* Initialize GPU Vulkan driver */
    vsl_vulkan_driver_init();

    g_vsl.active = true;
    return 0;
}

void vsl_shutdown(void) {
    if (!g_vsl.active) return;
    memset(&g_vsl, 0, sizeof(g_vsl));
}

bool vsl_active(void) {
    return g_vsl.active;
}

/* -- Diagnostics -------------------------------------------------- */

void vsl_info(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return;
    snprintf(buf, buf_size,
        "VSL v%u.%u: %u procs, %u drivers, %lu syscalls (%lu errors)\n"
        "  Kernel: 0x%08llX (%zu bytes)\n"
        "  User:   0x%08llX (%zu bytes)\n"
        "  Shared: 0x%08llX (%zu bytes)\n",
        g_vsl.version_major, g_vsl.version_minor,
        g_vsl.n_procs, g_vsl.n_drivers,
        (unsigned long)g_vsl.syscall_count,
        (unsigned long)g_vsl.syscall_errors,
        (unsigned long long)g_vsl.kernel_base, g_vsl.kernel_size,
        (unsigned long long)g_vsl.user_base, g_vsl.user_size,
        (unsigned long long)g_vsl.shared_base, g_vsl.shared_size);
}

void vsl_dump_state(void) {
    char buf[512];
    vsl_info(buf, sizeof(buf));
    printf("%s", buf);

    printf("  Processes:\n");
    for (uint32_t i = 0; i < g_vsl.n_procs; i++) {
        VSL_PROC *p = &g_vsl.procs[i];
        const char *state_str[] = {"UNUSED","READY","RUNNING","BLOCKED","ZOMBIE","DEAD"};
        printf("    PID %u: state=%s entry=0x%llX stack=0x%llX brk=0x%llX\n",
               p->pid, state_str[p->state],
               (unsigned long long)p->entry_point,
               (unsigned long long)p->stack_pointer,
               (unsigned long long)p->brk);
    }

    printf("  Drivers:\n");
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        VSL_DRV *d = &g_vsl.drivers[i];
        const char *type_str[] = {"NONE","VULKAN","CUDA","NET","BLOCK","INPUT","DISPLAY","AUDIO","USB","PCI"};
        printf("    [%u] type=%s active=%s io=0x%llX mem=0x%llX size=%zu irq=%u\n",
               i, type_str[d->type], d->active ? "yes" : "no",
               (unsigned long long)d->io_base,
               (unsigned long long)d->mem_base,
               d->mem_size, d->irq);
    }

    printf("  Files:\n");
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        VSL_FD *f = &g_vsl.fds[i];
        printf("    FD %d: host_fd=%d flags=0x%X path=%s\n",
               f->fd, f->vsl_fd, f->flags, f->path);
    }
}

void vsl_get_stats(uint64_t *out_syscalls, uint64_t *out_errors,
                   uint32_t *out_procs, uint32_t *out_drivers) {
    if (out_syscalls) *out_syscalls = g_vsl.syscall_count;
    if (out_errors) *out_errors = g_vsl.syscall_errors;
    if (out_procs) *out_procs = g_vsl.n_procs;
    if (out_drivers) *out_drivers = g_vsl.n_drivers;
}