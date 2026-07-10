/* wubu_proton2_gpu.c -- GPU detection subsystem (self-contained).
 *
 * wubu_gpu_detect / wubu_gpu_open: scan /sys/class/drm, identify vendor,
 * open /dev/dri. Minimal includes.
 */

#include "wubu_proton2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

int wubu_gpu_detect(char *name, int name_len, char *pci, int pci_len) {
    /* Scan /sys/class/drm for GPU devices */
    DIR *d = opendir("/sys/class/drm");
    if (!d) return -1;

    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "card", 4) != 0) continue;
        /* Read device info */
        char path[256], buf[256];
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(buf, sizeof(buf), f)) {
            if (name) {
                if (strstr(buf, "0x10de")) strncpy(name, "NVIDIA", name_len);
                else if (strstr(buf, "0x1002")) strncpy(name, "AMD", name_len);
                else if (strstr(buf, "0x8086")) strncpy(name, "Intel", name_len);
                else snprintf(name, name_len, "GPU(%s)", buf);
            }
            found = 1;
        }
        fclose(f);

        if (pci) {
            snprintf(path, sizeof(path), "/sys/class/drm/%s/device", ent->d_name);
            char link[256];
            ssize_t len = readlink(path, link, sizeof(link) - 1);
            if (len > 0) {
                link[len] = '\0';
                /* Extract PCI address from path */
                char *pci_addr = strrchr(link, '/');
                if (pci_addr) strncpy(pci, pci_addr + 1, pci_len);
            }
        }
        break; /* Use first GPU */
    }
    closedir(d);
    return found ? 0 : -1;
}

int wubu_gpu_open(const char *pci) {
    (void)pci;
    /* Open /dev/dri/card0 */
    int fd = open("/dev/dri/card0", O_RDWR);
    return fd;
}
