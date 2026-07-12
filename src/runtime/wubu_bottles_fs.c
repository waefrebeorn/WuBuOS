/* wubu_bottles_fs.c -- WuBuOS bottles: recursive filesystem delete.
 * Extracted from wubu_bottles.c (separable leaf). Self-contained: ftw only.
 * C11, minimal includes.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "wubu_bottles.h"
#include "wubu_bottles_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "wubu_fs_util.h"

int bottles_rm_rf(const char *path) {
    return wubu_fs_rm_rf(path);
}

