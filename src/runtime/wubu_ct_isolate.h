/*
 * wubu_ct_isolate.h  --  WuBuOS Container Isolation (cgroups v2 + seccomp)
 */

#ifndef WUBU_CT_ISOLATE_H
#define WUBU_CT_ISOLATE_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* -- Seccomp Profiles ---------------------------------------------- */

typedef enum {
    SECCOMP_PROFILE_NONE = 0,
    SECCOMP_PROFILE_BASIC,
    SECCOMP_PROFILE_GPU,
    SECCOMP_PROFILE_WINE,
} SeccompProfile;

/* -- Cgroups v2 API ------------------------------------------------ */

/*
 * Create a cgroup for the container under /sys/fs/cgroup/wubu/<name>
 * Returns 0 on success, -1 on failure.
 * out_path receives the cgroup path (optional).
 */
int wubu_ct_cgroup_create(const char *container_name,
                         char *out_path, size_t path_size);

/*
 * Set memory limit in cgroup (MB). 0 = unlimited.
 */
int wubu_ct_cgroup_set_memory(const char *cgroup_path, uint64_t mem_mb);

/*
 * Set CPU quota in cgroup. cpu_count = number of CPUs. 0 = unlimited.
 */
int wubu_ct_cgroup_set_cpu(const char *cgroup_path, int cpu_count);

/*
 * Set max PIDs in cgroup. 0 = unlimited.
 */
int wubu_ct_cgroup_set_pids(const char *cgroup_path, int max_pids);

/*
 * Set I/O max bandwidth in cgroup (cgroups v2 io controller).
 * read_bps/write_bps in bytes per second. 0 = unlimited.
 */
int wubu_ct_cgroup_set_io_max(const char *cgroup_path, uint64_t read_bps, uint64_t write_bps);

/*
 * Set I/O weight in cgroup (cgroups v2 io controller).
 * weight: 1-10000, 0 = default (100).
 */
int wubu_ct_cgroup_set_io_weight(const char *cgroup_path, uint32_t weight);

/*
 * Attach a process to the cgroup.
 */
int wubu_ct_cgroup_attach(const char *cgroup_path, pid_t pid);

/*
 * Remove the cgroup (must be empty).
 */
void wubu_ct_cgroup_destroy(const char *cgroup_path);

/* -- Seccomp API --------------------------------------------------- */

/*
 * Apply seccomp filter in the current process (call in child after fork).
 * Returns 0 on success, -1 on failure.
 */
int wubu_ct_apply_seccomp(void *ct);  /* WubuCt* opaque */

/*
 * Child isolation setup: attach to cgroup + apply seccomp.
 * Called in child process after fork(), before exec().
 */
int wubu_ct_child_isolation(void);

/*
 * Parent-side isolation setup: create cgroup, set limits, pass env to child.
 * Called in parent before fork().
 */
int wubu_ct_setup_isolation(void *ct_ptr);

/* -- Namespace Isolation API ---------------------------------------- */

/*
 * Unshare namespaces for container isolation.
 * flags: CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWIPC
 * Returns 0 on success, -1 on failure.
 */
int wubu_ns_unshare(int flags);

/*
 * Write to a cgroup file (generic helper).
 * path: full path to cgroup file (e.g., /sys/fs/cgroup/wubu/name/memory.max)
 * value: string value to write
 * Returns 0 on success, -1 on failure.
 */
int wubu_cgroup_write(const char *path, const char *value);

/*
 * Install seccomp filter in current process.
 * profile: SECCOMP_PROFILE_BASIC, GPU, WINE, or NONE
 * Returns 0 on success, -1 on failure.
 */
int wubu_seccomp_install(SeccompProfile profile);

#endif /* WUBU_CT_ISOLATE_H */