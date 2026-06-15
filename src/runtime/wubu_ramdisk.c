/*
 * wubu_ramdisk.c  --  WuBuOS Root Mount for Arch Container Roots
 *
 * Cell 392: Two-mode Arch root  --  RAM for containers, SSD for bare metal.
 *
 * Container mode:  tmpfs at /run/wubu/ramdisk → zero disk, instant teardown
 * Bare metal mode: SSD at /var/wubu/roots/arch-base → persistent, real OS
 * Cross-mode:      install_to_disk() copies RAM → SSD for persistence
 *
 * Both paths feed into the same wubu_host_exec container runtime.
 * The chroot root is just a directory  --  RAM or disk doesn't matter
 * to the fork+chroot+exec machinery.
 */
#include "wubu_ramdisk.h"
#include "wubu_arch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <errno.h>

/* -- Helper: mkdir -p --------------------------------------------- */

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* -- Helper: run command ------------------------------------------ */

static int run_cmd(const char *cmd) {
    int ret = system(cmd);
    if (WIFEXITED(ret)) return WEXITSTATUS(ret);
    return -1;
}

/* -- Helper: detect image format ---------------------------------- */

typedef enum {
    IMG_UNKNOWN,
    IMG_CPIO_GZ,     /* .cgz, .cpio.gz */
    IMG_TAR_GZ,      /* .tar.gz, .tgz */
    IMG_TAR_ZST,     /* .tar.zst */
    IMG_DIRECTORY,   /* existing directory */
} ImgFormat;

static ImgFormat detect_format(const char *path) {
    if (!path) return IMG_UNKNOWN;

    /* Check if directory */
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return IMG_DIRECTORY;

    size_t len = strlen(path);

    /* .tar.zst */
    if (len > 8 && strcmp(path + len - 8, ".tar.zst") == 0)
        return IMG_TAR_ZST;

    /* .tar.gz */
    if (len > 7 && strcmp(path + len - 7, ".tar.gz") == 0)
        return IMG_TAR_GZ;

    /* .tgz */
    if (len > 4 && strcmp(path + len - 4, ".tgz") == 0)
        return IMG_TAR_GZ;

    /* .cpio.gz */
    if (len > 8 && strcmp(path + len - 8, ".cpio.gz") == 0)
        return IMG_CPIO_GZ;

    /* .cgz */
    if (len > 4 && strcmp(path + len - 4, ".cgz") == 0)
        return IMG_CPIO_GZ;

    return IMG_UNKNOWN;
}

/* -- Create ------------------------------------------------------- */

WubuRamdisk *wubu_rd_create(WubuRdMode mode, const char *image_path) {
    WubuRamdisk *rd = (WubuRamdisk*)calloc(1, sizeof(WubuRamdisk));
    if (!rd) return NULL;

    rd->mode = mode;
    rd->state = WUBU_RD_NONE;
    rd->usage_mb = 0;

    if (mode == WUBU_RD_RAM) {
        strncpy(rd->path, WUBU_RD_RAM_PATH, sizeof(rd->path) - 1);
        strncpy(rd->ram_size, WUBU_RD_RAM_SIZE, sizeof(rd->ram_size) - 1);
        rd->limit_mb = 2048;
    } else {
        strncpy(rd->path, WUBU_RD_DISK_PATH, sizeof(rd->path) - 1);
        rd->ram_size[0] = '\0';
        rd->limit_mb = 0;  /* No limit on SSD */
    }

    strncpy(rd->image_path,
            image_path ? image_path : WUBU_RD_IMAGE_DEFAULT,
            sizeof(rd->image_path) - 1);

    strncpy(rd->disk_path, WUBU_RD_DISK_PATH, sizeof(rd->disk_path) - 1);

    return rd;
}

/* -- Destroy ------------------------------------------------------ */

void wubu_rd_destroy(WubuRamdisk *rd) {
    if (!rd) return;

    if (rd->mode == WUBU_RD_RAM && rd->state >= WUBU_RD_MOUNTED) {
        wubu_rd_unmount(rd);
    }

    free(rd);
}

/* -- Mount tmpfs (RAM mode) --------------------------------------- */

int wubu_rd_mount(WubuRamdisk *rd) {
    if (!rd || rd->mode != WUBU_RD_RAM) return -1;
    if (rd->state >= WUBU_RD_MOUNTED) return 0;  /* Already mounted */

    /* Create mount point */
    if (mkdir_p(rd->path, 0755) != 0) return -1;

    /* Mount tmpfs */
    char opts[128];
    snprintf(opts, sizeof(opts), "size=%s,mode=0755", rd->ram_size);

    if (mount("tmpfs", rd->path, "tmpfs", 0, opts) != 0) {
        /* May fail without root  --  fall back to just using the directory */
        /* This works for testing / unprivileged mode */
        struct stat st;
        if (stat(rd->path, &st) == 0 && S_ISDIR(st.st_mode)) {
            rd->state = WUBU_RD_MOUNTED;
            return 0;
        }
        rd->state = WUBU_RD_FAILED;
        return -1;
    }

    rd->state = WUBU_RD_MOUNTED;
    return 0;
}

/* -- Load rootfs image -------------------------------------------- */

int wubu_rd_load(WubuRamdisk *rd) {
    if (!rd || rd->state < WUBU_RD_MOUNTED) return -1;

    ImgFormat fmt = detect_format(rd->image_path);
    char cmd[2048];

    switch (fmt) {
        case IMG_CPIO_GZ:
            snprintf(cmd, sizeof(cmd),
                     "gunzip -c %s | (cd %s && cpio -idm 2>/dev/null)",
                     rd->image_path, rd->path);
            break;

        case IMG_TAR_GZ:
            snprintf(cmd, sizeof(cmd),
                     "tar xzf %s -C %s 2>/dev/null",
                     rd->image_path, rd->path);
            break;

        case IMG_TAR_ZST:
            snprintf(cmd, sizeof(cmd),
                     "tar --zstd -xf %s -C %s 2>/dev/null",
                     rd->image_path, rd->path);
            break;

        case IMG_DIRECTORY:
            snprintf(cmd, sizeof(cmd),
                     "cp -a %s/. %s/ 2>/dev/null",
                     rd->image_path, rd->path);
            break;

        default:
            /* Try tar.gz as default */
            snprintf(cmd, sizeof(cmd),
                     "tar xzf %s -C %s 2>/dev/null",
                     rd->image_path, rd->path);
            break;
    }

    int ret = run_cmd(cmd);
    if (ret != 0) {
        rd->state = WUBU_RD_FAILED;
        return -1;
    }

    rd->state = WUBU_RD_LOADED;
    return 0;
}

/* -- Unmount tmpfs (RAM mode) ------------------------------------- */

int wubu_rd_unmount(WubuRamdisk *rd) {
    if (!rd || rd->mode != WUBU_RD_RAM) return -1;
    if (rd->state < WUBU_RD_MOUNTED) return 0;

    /* Unmount tmpfs  --  all data in RAM is destroyed */
    if (umount(rd->path) != 0) {
        /* May fail if not actually a mount (testing) */
        /* Non-fatal */
    }

    rd->state = WUBU_RD_NONE;
    return 0;
}

/* -- Boot --------------------------------------------------------- */

int wubu_rd_boot(WubuRamdisk *rd) {
    if (!rd) return -1;

    if (rd->mode == WUBU_RD_RAM) {
        /* RAM mode: mount tmpfs + load image */
        if (wubu_rd_mount(rd) != 0) return -1;

        /* Check if image exists */
        if (access(rd->image_path, F_OK) == 0) {
            if (wubu_rd_load(rd) != 0) return -1;
        } else {
            /* No image  --  just mount empty tmpfs.
             * Caller will bootstrap Arch into it. */
            rd->state = WUBU_RD_MOUNTED;
        }
        return 0;

    } else {
        /* DISK mode: ensure directory exists */
        if (mkdir_p(rd->path, 0755) != 0) {
            rd->state = WUBU_RD_FAILED;
            return -1;
        }

        /* Check if Arch is already installed */
        if (wubu_arch_root_valid(rd->path)) {
            rd->state = WUBU_RD_LOADED;
            return 0;
        }

        /* No Arch root  --  caller needs to bootstrap */
        rd->state = WUBU_RD_MOUNTED;  /* Directory ready */
        return 0;
    }
}

/* -- Root Path ---------------------------------------------------- */

const char *wubu_rd_root_path(WubuRamdisk *rd) {
    if (!rd) return NULL;
    return rd->path;
}

/* -- State / Usage ------------------------------------------------ */

WubuRdState wubu_rd_state(WubuRamdisk *rd) {
    return rd ? rd->state : WUBU_RD_NONE;
}

uint64_t wubu_rd_usage_mb(WubuRamdisk *rd) {
    if (!rd) return 0;

    /* Query df for actual usage */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "df -m %s 2>/dev/null | tail -1 | awk '{print $3}'", rd->path);
    FILE *f = popen(cmd, "r");
    if (f) {
        unsigned long mb = 0;
        if (fscanf(f, "%lu", &mb) >= 1)
            rd->usage_mb = (uint64_t)mb;
        pclose(f);
    }

    return rd->usage_mb;
}

/* -- Disk Bootstrap ----------------------------------------------- */

int wubu_rd_bootstrap_disk(WubuRamdisk *rd, const char *mirror) {
    if (!rd || rd->mode != WUBU_RD_DISK) return -1;

    int ret = wubu_arch_bootstrap(rd->path, mirror, NULL);
    if (ret != 0) {
        rd->state = WUBU_RD_FAILED;
        return -1;
    }

    rd->state = WUBU_RD_LOADED;
    return 0;
}

/* -- Install RAM → SSD -------------------------------------------- */

int wubu_rd_install_to_disk(WubuRamdisk *rd, const char *disk_path) {
    if (!rd || rd->state < WUBU_RD_LOADED) return -1;

    const char *target = disk_path ? disk_path : rd->disk_path;
    if (!target[0]) return -1;

    /* Create target directory */
    if (mkdir_p(target, 0755) != 0) return -1;

    /* rsync RAM → SSD (preserves permissions, ownership) */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "rsync -aAX --delete %s/ %s/ 2>/dev/null || "
             "cp -a %s/. %s/ 2>/dev/null",
             rd->path, target, rd->path, target);

    int ret = run_cmd(cmd);
    return ret;
}

/* -- Snapshot ----------------------------------------------------- */

int wubu_rd_snapshot(WubuRamdisk *rd, const char *output_path) {
    if (!rd || rd->state < WUBU_RD_LOADED) return -1;

    const char *out = output_path ? output_path : WUBU_RD_IMAGE_DEFAULT;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "tar --zstd -cf %s -C %s . 2>/dev/null || "
             "tar czf %s -C %s . 2>/dev/null",
             out, rd->path, out, rd->path);

    return run_cmd(cmd);
}

/* -- Install Packages --------------------------------------------- */

int wubu_rd_install(WubuRamdisk *rd, const char *packages) {
    if (!rd || !packages) return -1;
    if (rd->state < WUBU_RD_MOUNTED) return -1;

    return wubu_arch_install(rd->path, packages);
}

/* -- Set RAM Size ------------------------------------------------- */

void wubu_rd_set_ram_size(WubuRamdisk *rd, const char *size_str) {
    if (!rd || !size_str) return;
    strncpy(rd->ram_size, size_str, sizeof(rd->ram_size) - 1);

    /* Parse MB from size string (e.g., "4096m" → 4096) */
    unsigned long val = 0;
    if (sscanf(size_str, "%lum", &val) >= 1)
        rd->limit_mb = (uint64_t)val;
}
