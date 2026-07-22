/*
 * ct_iso_cgroup.c  --  WuBuOS container cgroups v2 write helper (Cell 420 split).
 * The cgroup create/set/attach ops live in wubu_ct_isolate_cgroup.c.
 */

#define _GNU_SOURCE
#include "ct_iso_cgroup.h"
#include "wubu_ct_isolate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int wubu_cgroup_write(const char *path, const char *value) {
    if (!path || !value) return -1;
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t n = write(fd, value, strlen(value));
    close(fd);
    return n > 0 ? 0 : -1;
}

/* Install seccomp filter in current process */
