/* wubu_ramdisk_internal.h -- Internal helpers shared by wubu_ramdisk sub-modules. */
#ifndef WUBU_RAMDISK_INTERNAL_H
#define WUBU_RAMDISK_INTERNAL_H

#include "wubu_ramdisk.h"

/* Image format enum (was local to wubu_ramdisk.c; shared with format submodule) */
typedef enum {
    IMG_UNKNOWN,
    IMG_CPIO_GZ,     /* .cgz, .cpio.gz */
    IMG_TAR_GZ,      /* .tar.gz, .tgz */
    IMG_TAR_ZST,     /* .tar.zst */
    IMG_DIRECTORY,   /* existing directory */
} ImgFormat;

/* Image format sniffing (wubu_ramdisk_format.c) */
ImgFormat detect_format(const char *path);

#endif /* WUBU_RAMDISK_INTERNAL_H */
