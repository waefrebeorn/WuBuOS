/* dosgui_explorer_drives.c -- Drive / volume enumeration subsystem.
 *
 * Self-contained: enumerates mounted volumes via the 9P/Styx backend
 * (ex_9p_opendir/ex_9p_stat, declared in dosgui_explorer_internal.h).
 * Minimal includes; shared helpers come from the internal header.
 */

#include "dosgui_explorer_internal.h"
#include <stdio.h>
#include <string.h>

int dosgui_explorer_enumerate_drives(char paths[][EX_MAX_PATH], char labels[][64], int max) {
    int count = 0;

    /* Add root filesystem */
    if (count < max) {
        strcpy(paths[count], "/");
        strcpy(labels[count], "Root Filesystem");
        count++;
    }

    /* On Linux, check /mnt for mounted volumes */
    DIR *mnt = ex_9p_opendir("/mnt");
    if (mnt) {
        struct dirent *ent;
        while ((ent = readdir(mnt)) && count < max) {
            if (ent->d_name[0] == '.') continue;

            char path[EX_MAX_PATH];
            snprintf(path, sizeof(path), "/mnt/%s", ent->d_name);

            struct stat st;
            if (ex_9p_stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncpy(paths[count], path, EX_MAX_PATH - 1);
                strncpy(labels[count], ent->d_name, 63);
                count++;
            }
        }
        closedir(mnt);
    }

    /* Check /media for removable media */
    DIR *media = ex_9p_opendir("/media");
    if (media) {
        struct dirent *ent;
        while ((ent = readdir(media)) && count < max) {
            if (ent->d_name[0] == '.') continue;

            char path[EX_MAX_PATH];
            snprintf(path, sizeof(path), "/media/%s", ent->d_name);

            struct stat st;
            if (ex_9p_stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncpy(paths[count], path, EX_MAX_PATH - 1);
                strncpy(labels[count], ent->d_name, 63);
                count++;
            }
        }
        closedir(media);
    }

    return count;
}
