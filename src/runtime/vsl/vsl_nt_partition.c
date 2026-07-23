/*
 * vsl_nt_partition.c -- Windows 11 Partition/CpuPartition syscalls.
 *
 * Partitions are lightweight isolation units in modern NT, used for
 * containers and VBS. In WuBuOS we back partitions with cgroup
 * directories (already available from our container subsystem).
 *
 * 8 syscalls (Windows 11 24H2 ordinals 168-421).
 */

#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define NT_PART_MAX  16

typedef struct {
    int      used;
    uint32_t handle;
    char     name[256];
    char     cgroup_path[512];
    int      is_cpu_partition;
} nt_partition_t;

static nt_partition_t g_nt_partitions[NT_PART_MAX];

static nt_partition_t *nt_part_find(uint32_t h) {
    for (int i = 0; i < NT_PART_MAX; i++)
        if (g_nt_partitions[i].used && g_nt_partitions[i].handle == h)
            return &g_nt_partitions[i];
    return NULL;
}

static nt_partition_t *nt_part_alloc(uint32_t *out_h) {
    for (int i = 0; i < NT_PART_MAX; i++) {
        if (!g_nt_partitions[i].used) {
            uint32_t h = 0xD000 + (uint32_t)i;
            g_nt_partitions[i].used = 1;
            g_nt_partitions[i].handle = h;
            g_nt_partitions[i].is_cpu_partition = 0;
            *out_h = h;
            return &g_nt_partitions[i];
        }
    }
    return NULL;
}

/* 168: NtCreateCpuPartition */
int64_t vsl_nt_create_cpu_partition(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_partition_t *p = nt_part_alloc(&h);
    if (!p) return NT_STATUS_INSUFFICIENT_RESOURCES;
    p->is_cpu_partition = 1;
    snprintf(p->name, sizeof(p->name), "cpupart_%u", h);
    snprintf(p->cgroup_path, sizeof(p->cgroup_path),
             "/sys/fs/cgroup/wubu_cpu_%u", h);
    mkdir(p->cgroup_path, 0755);
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 189: NtCreatePartition */
int64_t vsl_nt_create_partition(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_partition_t *p = nt_part_alloc(&h);
    if (!p) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (b) {
        const char *name = (const char *)(void *)b;
        snprintf(p->name, sizeof(p->name), "%s", name);
    } else {
        snprintf(p->name, sizeof(p->name), "part_%u", h);
    }
    snprintf(p->cgroup_path, sizeof(p->cgroup_path),
             "/sys/fs/cgroup/wubu_part_%u", h);
    mkdir(p->cgroup_path, 0755);
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 282: NtManagePartition */
int64_t vsl_nt_manage_partition(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t part_h = (uint32_t)a;
    if (!nt_part_find(part_h)) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}

/* 294: NtOpenCpuPartition */
int64_t vsl_nt_open_cpu_partition(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_partition_t *p = nt_part_alloc(&h);
    if (!p) return NT_STATUS_INSUFFICIENT_RESOURCES;
    p->is_cpu_partition = 1;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 305: NtOpenPartition */
int64_t vsl_nt_open_partition(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_partition_t *p = nt_part_alloc(&h);
    if (!p) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 340: NtQueryInformationCpuPartition */
int64_t vsl_nt_query_information_cpu_partition(uint64_t a, uint64_t b, uint64_t c,
                                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t part_h = (uint32_t)a;
    nt_partition_t *p = nt_part_find(part_h);
    if (!p) return NT_STATUS_INVALID_HANDLE;
    if (c && d >= 4) *(uint32_t *)c = p->is_cpu_partition;
    if (e) *(uint32_t *)e = 4;
    return NT_STATUS_SUCCESS;
}

/* 388: NtReplacePartitionUnit */
int64_t vsl_nt_replace_partition_unit(uint64_t a, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    /* Replace a hardware partition unit (hot-replace) */
    return NT_STATUS_SUCCESS;
}

/* 421: NtSetInformationCpuPartition */
int64_t vsl_nt_set_information_cpu_partition(uint64_t a, uint64_t b, uint64_t c,
                                              uint64_t d, uint64_t e, uint64_t f) {
    uint32_t part_h = (uint32_t)a;
    if (!nt_part_find(part_h)) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}

void vsl_nt_partition_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
tbl[344-1] = vsl_nt_create_cpu_partition;
tbl[345-1] = vsl_nt_create_partition;
tbl[346-1] = vsl_nt_manage_partition;
tbl[347-1] = vsl_nt_open_cpu_partition;
tbl[348-1] = vsl_nt_open_partition;
tbl[349-1] = vsl_nt_query_information_cpu_partition;
tbl[350-1] = vsl_nt_replace_partition_unit;
tbl[351-1] = vsl_nt_set_information_cpu_partition;
}
