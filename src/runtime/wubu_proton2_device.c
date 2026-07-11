/* wubu_proton2_device.c -- WuBuOS proton2: HID/USB/MIDI device enumeration.
 * Extracted from wubu_proton2.c (separable leaf). Self-contained: scans /dev + /sys
 * for input/USB/MIDI devices. No ProtonManager coupling. C11, minimal includes.
 */
#include "wubu_proton2.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>

int wubu_hid_enumerate(char names[][64], int *types, int max) {
    DIR *d = opendir("/dev/input");
    if (!d) return 0;

    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL && count < max) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        /* Get device name */
        char name[64] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        strncpy(names[count], name, 63);

        /* Determine type from evdev capabilities */
        unsigned long evbit[EV_MAX/8 + 1] = {0};
        ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit);
        if (types) {
            int has_key = (evbit[EV_KEY/8] & (1 << (EV_KEY%8))) != 0;
            int has_abs = (evbit[EV_ABS/8] & (1 << (EV_ABS%8))) != 0;
            int has_rel = (evbit[EV_REL/8] & (1 << (EV_REL%8))) != 0;
            if (has_key && has_abs) types[count] = 1; /* Gamepad */
            else if (has_key)    types[count] = 2; /* Keyboard */
            else if (has_rel)    types[count] = 3; /* Mouse */
            else                 types[count] = 0; /* Unknown */
        }
        close(fd);
        count++;
    }
    closedir(d);
    return count;
}

int wubu_hid_open(const char *path) {
    return open(path, O_RDWR | O_NONBLOCK);
}

int wubu_usb_enumerate(char paths[][256], char names[][64], int max) {
    DIR *d = opendir("/dev/bus/usb");
    if (!d) return 0;

    struct dirent *bus_ent;
    int count = 0;
    while ((bus_ent = readdir(d)) != NULL && count < max) {
        char bus_path[256];
        snprintf(bus_path, sizeof(bus_path), "/dev/bus/usb/%s", bus_ent->d_name);
        DIR *bus = opendir(bus_path);
        if (!bus) continue;

        struct dirent *dev_ent;
        while ((dev_ent = readdir(bus)) != NULL && count < max) {
            if (dev_ent->d_name[0] == '.') continue;
            snprintf(paths[count], 256, "%s/%s", bus_path, dev_ent->d_name);

            /* Try to get device name from uevent */
            char uevent_path[512], name[64] = "USB Device";
            snprintf(uevent_path, sizeof(uevent_path),
                     "/sys/bus/usb/devices/%s/%s/uevent",
                     bus_ent->d_name, dev_ent->d_name);
            FILE *f = fopen(uevent_path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "PRODUCT=", 8) == 0) {
                        int v, p;
                        if (sscanf(line + 8, "%x/%x", &v, &p) == 2)
                            snprintf(name, sizeof(name), "USB %04x:%04x", v, p);
                        break;
                    }
                }
                fclose(f);
            }
            strncpy(names[count], name, 63);
            count++;
        }
        closedir(bus);
    }
    closedir(d);
    return count;
}

int wubu_usb_open(const char *path) {
    return open(path, O_RDWR);
}

int wubu_midi_enumerate(char names[][64], int max) {
    int count = 0;

    /* Check /dev/snd/seq */
    if (count < max && access("/dev/snd/seq", F_OK) == 0) {
        strncpy(names[count], "ALSA MIDI Sequencer", 63);
        count++;
    }

    /* Check /dev/midi* */
    DIR *d = opendir("/dev");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && count < max) {
            if (strncmp(ent->d_name, "midi", 4) == 0) {
                snprintf(names[count], 63, "MIDI %s", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }

    /* Check /dev/snd/midi* */
    d = opendir("/dev/snd");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && count < max) {
            if (strncmp(ent->d_name, "midi", 4) == 0) {
                snprintf(names[count], 63, "ALSA %s", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }

    return count;
}

int wubu_midi_open(const char *path) {
    return open(path, O_RDWR | O_NONBLOCK);
}

/* -- Proton Manager Create/Destroy -------------------------------- */

