/* wubu_ramdisk_format.c -- WuBuOS ramdisk: image format sniffing.
 * Extracted from wubu_ramdisk.c (separable leaf). Self-contained: stat +
 * extension matching -> ImgFormat. C11, minimal includes.
 */
#include "wubu_ramdisk.h"
#include "wubu_ramdisk_internal.h"

#include <string.h>
#include <sys/stat.h>

ImgFormat detect_format(const char *path) {
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
