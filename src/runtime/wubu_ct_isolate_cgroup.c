/* wubu_ct_isolate_cgroup.c -- Container cgroup operations subsystem.
 *
 * Self-contained: cgroup file read/write helpers + the wubu_ct_cgroup_*
 * API (create/set memory/cpu/pids/io/attach/destroy). Uses the cgroupfs
 * layout and standard POSIX I/O. Minimal includes.
 */

#include "wubu_ct_isolate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

static int cgroup_write_file(const char *cgroup_path, const char *file, const char *value) {
    char path[WUBU_CGROUP_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", cgroup_path, file);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    ssize_t n = write(fd, value, strlen(value));
    close(fd);
    return n > 0 ? 0 : -1;
}

static int cgroup_read_file(const char *cgroup_path, const char *file, char *buf, size_t bufsize) {
    char path[WUBU_CGROUP_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", cgroup_path, file);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t n = read(fd, buf, bufsize - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
}

int wubu_ct_cgroup_create(const char *container_name, char *out_path, size_t path_size) {
    char path[WUBU_CGROUP_MAX_PATH];

    /* Ensure base exists */
    struct stat st;
    if (stat(WUBU_CGROUP_BASE, &st) != 0) {
        if (mkdir(WUBU_CGROUP_BASE, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    /* Enable controllers we need */
    char controllers[256];
    cgroup_read_file(WUBU_CGROUP_BASE, "cgroup.controllers", controllers, sizeof(controllers));
    if (strstr(controllers, "memory") && strstr(controllers, "cpu") && strstr(controllers, "pids")) {
        /* Controllers available, enable them in subtree_control */
        cgroup_write_file(WUBU_CGROUP_BASE, "cgroup.subtree_control", "+memory +cpu +pids");
    }

    /* Create container cgroup */
    snprintf(path, sizeof(path), "%s/%s", WUBU_CGROUP_BASE, container_name);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    /* Enable controllers for this cgroup */
    cgroup_write_file(path, "cgroup.subtree_control", "+memory +cpu +pids");

    if (out_path) {
        strncpy(out_path, path, path_size - 1);
        out_path[path_size - 1] = '\0';
    }
    return 0;
}

int wubu_ct_cgroup_set_memory(const char *cgroup_path, uint64_t mem_mb) {
    if (mem_mb == 0) {
        /* Unlimited: write "max" */
        return cgroup_write_file(cgroup_path, "memory.max", "max");
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(mem_mb * 1024 * 1024));
    return cgroup_write_file(cgroup_path, "memory.max", buf);
}

int wubu_ct_cgroup_set_cpu(const char *cgroup_path, int cpu_count) {
    if (cpu_count <= 0) {
        return cgroup_write_file(cgroup_path, "cpu.max", "max 100000");
    }
    /* cpu.max format: "quota period" -- quota in microseconds, period=100000 (100ms) */
    char buf[64];
    snprintf(buf, sizeof(buf), "%d 100000", cpu_count * 100000);
    return cgroup_write_file(cgroup_path, "cpu.max", buf);
}

int wubu_ct_cgroup_set_pids(const char *cgroup_path, int max_pids) {
    if (max_pids <= 0) {
        return cgroup_write_file(cgroup_path, "pids.max", "max");
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", max_pids);
    return cgroup_write_file(cgroup_path, "pids.max", buf);
}

int wubu_ct_cgroup_attach(const char *cgroup_path, pid_t pid) {
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    return cgroup_write_file(cgroup_path, "cgroup.procs", pid_str);
}

void wubu_ct_cgroup_destroy(const char *cgroup_path) {
    /* Remove the cgroup directory (will fail if processes still attached) */
    rmdir(cgroup_path);
}

int wubu_ct_cgroup_set_io_max(const char *cgroup_path, uint64_t read_bps, uint64_t write_bps) {
    char buf[128];
    if (read_bps == 0 && write_bps == 0) {
        return cgroup_write_file(cgroup_path, "io.max", "max");
    }
    snprintf(buf, sizeof(buf), "8:0 rbps=%lu wbps=%lu", (unsigned long)read_bps, (unsigned long)write_bps);
    return cgroup_write_file(cgroup_path, "io.max", buf);
}

int wubu_ct_cgroup_set_io_weight(const char *cgroup_path, uint32_t weight) {
    if (weight == 0) weight = 100; /* Default weight */
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", weight);
    return cgroup_write_file(cgroup_path, "io.weight", buf);
}
