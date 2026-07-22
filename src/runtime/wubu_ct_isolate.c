/*
 * wubu_ct_isolate.c  --  WuBuOS Container Isolation (cgroups v2 + seccomp)
 *
 * Cell 420: Security hardening for container execution.
 * Not syscall emulation -- real Linux kernel isolation primitives.
 *
 * cgroups v2: resource limits (memory, CPU, I/O) via /sys/fs/cgroup
 * seccomp-bpf: syscall filtering via seccomp(2) syscall
 *
 * Design:
 *   - Called from wubu_ct_start() before fork()
 *   - Creates cgroup under /sys/fs/cgroup/wubu/<container_name>
 *   - Applies memory.max, cpu.max, pids.max
 *   - Installs seccomp filter in child after fork(), before exec()
 *   - Filter is configurable per-container runtime (native/steamos/proton/holyc)
 */

#define _GNU_SOURCE
#include "wubu_ct_isolate.h"
#include "wubu_host_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sched.h>
#include <stdint.h>

#include "ct_iso_seccomp.h"
#include "ct_iso_cgroup.h"
#include "ct_iso_ns.h"

int wubu_ct_setup_isolation(void *ct_ptr) {
    WubuCt *ct = (WubuCt *)ct_ptr;
    if (!ct) return -1;

    char cgroup_path[WUBU_CGROUP_MAX_PATH];

    /* Create cgroup */
    if (wubu_ct_cgroup_create(ct->name, cgroup_path, sizeof(cgroup_path)) != 0) {
        /* Non-fatal: cgroups may not be available */
        return 0;
    }

    /* Store path for later cleanup/attach */
    /* We'll pass it via environment variable to the child */
    char env[WUBU_CGROUP_MAX_PATH + 32];
    snprintf(env, sizeof(env), "WUBU_CGROUP_PATH=%s", cgroup_path);
    wubu_ct_add_env(ct, env);

    /* Apply resource limits */
    if (ct->mem_limit_mb > 0) {
        wubu_ct_cgroup_set_memory(cgroup_path, ct->mem_limit_mb);
    }
    if (ct->cpu_limit > 0) {
        wubu_ct_cgroup_set_cpu(cgroup_path, ct->cpu_limit);
    }
    /* Default pids limit */
    wubu_ct_cgroup_set_pids(cgroup_path, 1024);

    /* I/O controller limits (procmem/io) */
    if (ct->io_read_bps > 0 || ct->io_write_bps > 0) {
        wubu_ct_cgroup_set_io_max(cgroup_path, ct->io_read_bps, ct->io_write_bps);
    }
    if (ct->io_weight > 0) {
        wubu_ct_cgroup_set_io_weight(cgroup_path, ct->io_weight);
    }

    /* Pass namespace flags to child via environment */
    char env_ns[32];
    snprintf(env_ns, sizeof(env_ns), "WUBU_NS_FLAGS=%d", WUBU_NS_FLAGS);
    wubu_ct_add_env(ct, env_ns);

    /* Set seccomp profile env for child */
    SeccompProfile sp = runtime_to_seccomp(ct->runtime);
    const char *sp_str = "basic";
    if (sp == SECCOMP_PROFILE_GPU) sp_str = "gpu";
    else if (sp == SECCOMP_PROFILE_WINE) sp_str = "wine";
    else if (sp == SECCOMP_PROFILE_NONE) sp_str = "none";
    char env2[64];
    snprintf(env2, sizeof(env2), "WUBU_SECCOMP_PROFILE=%s", sp_str);
    wubu_ct_add_env(ct, env2);

    return 0;
}

/* Called in child after fork() to attach to cgroup and apply seccomp */
