/* wubu_metal_evdev.c -- WuBuOS evdev input backend (extracted from wubu_metal.c).
 * Mirror of the original wubu_metal.c include set (proven to compile) plus the
 * extern globals this backend touches. C11, no god headers. */

#include "wubu_metal.h"
#include "wubu_metal_audio.h"
#include "../audio/wubu_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

extern WubuInput g_input;
int wubu_evdev_find_device(const char *type, int *out_fd) {
    DIR *d = opendir("/dev/input");
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) && *out_fd < 0) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        uint8_t evbit[EV_MAX / 8 + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
            close(fd);
            continue;
        }

        bool match = false;
        if (strcmp(type, "keyboard") == 0) {
            if (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8))) {
                /* Check for keyboard keys */
                uint8_t keybit[KEY_MAX / 8 + 1] = {0};
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
                if (keybit[KEY_A / 8] & (1 << (KEY_A % 8))) match = true;
            }
        } else if (strcmp(type, "mouse") == 0) {
            if ((evbit[EV_REL / 8] & (1 << (EV_REL % 8))) &&
                (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
                uint8_t relbit[REL_MAX / 8 + 1] = {0};
                ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit);
                if ((relbit[REL_X / 8] & (1 << (REL_X % 8))) &&
                    (relbit[REL_Y / 8] & (1 << (REL_Y % 8)))) match = true;
            }
        } else if (strcmp(type, "touch") == 0) {
            if (evbit[EV_ABS / 8] & (1 << (EV_ABS % 8))) match = true;
        } else if (strcmp(type, "gamepad") == 0) {
            uint8_t keybit[KEY_MAX / 8 + 1] = {0};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
            if (keybit[BTN_GAMEPAD / 8] & (1 << (BTN_GAMEPAD % 8))) match = true;
        } else if (strcmp(type, "midi") == 0) {
            /* MIDI devices often appear as HID */
            uint8_t keybit[KEY_MAX / 8 + 1] = {0};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
            /* Check for generic MIDI/HID keys - BTN_MISC is more common */
            if (keybit[BTN_MISC / 8] & (1 << (BTN_MISC % 8))) match = true;
        }

        if (match) {
            *out_fd = fd;
        } else {
            close(fd);
        }
    }

    closedir(d);
    return *out_fd >= 0 ? 0 : -1;
}

void wubu_evdev_init_all(void) {
    g_input.backend = INPUT_EVDEV;

    /* Keyboard */
    g_input.kbd_fd = -1;
    wubu_evdev_find_device("keyboard", &g_input.kbd_fd);
    if (g_input.kbd_fd >= 0) printf("[metal] Keyboard: /dev/input/event%d\n", g_input.kbd_fd);

    /* Mouse */
    g_input.mouse_fd = -1;
    wubu_evdev_find_device("mouse", &g_input.mouse_fd);
    if (g_input.mouse_fd >= 0) printf("[metal] Mouse: /dev/input/event%d\n", g_input.mouse_fd);

    /* Touch */
    g_input.touch_fd = -1;
    wubu_evdev_find_device("touch", &g_input.touch_fd);

    /* Gamepads */
    g_input.n_gamepads = 0;
    for (int i = 0; i < 4; i++) {
        g_input.gamepad_fds[i] = -1;
        if (wubu_evdev_find_device("gamepad", &g_input.gamepad_fds[i]) == 0) {
            g_input.n_gamepads++;
        }
    }

    /* MIDI HID */
    g_input.n_midi = 0;
    for (int i = 0; i < 4; i++) {
        g_input.midi_fds[i] = -1;
        if (wubu_evdev_find_device("midi", &g_input.midi_fds[i]) == 0) {
            g_input.n_midi++;
        }
    }

    /* USB HID Raw */
    g_input.n_hidraw = 0;
    DIR *d = opendir("/dev/hidraw");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && g_input.n_hidraw < 8) {
            if (strncmp(ent->d_name, "hidraw", 6) != 0) continue;
            char path[256];
            snprintf(path, sizeof(path), "/dev/hidraw/%s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                g_input.hidraw_fds[g_input.n_hidraw++] = fd;
            }
        }
        closedir(d);
    }
}

void wubu_evdev_shutdown(void) {
    if (g_input.kbd_fd >= 0) close(g_input.kbd_fd);
    if (g_input.mouse_fd >= 0) close(g_input.mouse_fd);
    if (g_input.touch_fd >= 0) close(g_input.touch_fd);
    for (int i = 0; i < g_input.n_gamepads; i++) if (g_input.gamepad_fds[i] >= 0) close(g_input.gamepad_fds[i]);
    for (int i = 0; i < g_input.n_midi; i++) if (g_input.midi_fds[i] >= 0) close(g_input.midi_fds[i]);
    for (int i = 0; i < g_input.n_hidraw; i++) if (g_input.hidraw_fds[i] >= 0) close(g_input.hidraw_fds[i]);
    memset(&g_input, 0, sizeof(g_input));
}

int wubu_evdev_poll(void) {
    int events = 0;
    struct input_event ev[64];

    /* Keyboard */
    if (g_input.kbd_fd >= 0) {
        int n = read(g_input.kbd_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Mouse */
    if (g_input.mouse_fd >= 0) {
        int n = read(g_input.mouse_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Touch */
    if (g_input.touch_fd >= 0) {
        int n = read(g_input.touch_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Gamepads */
    for (int i = 0; i < g_input.n_gamepads; i++) {
        if (g_input.gamepad_fds[i] >= 0) {
            int n = read(g_input.gamepad_fds[i], ev, sizeof(ev));
            if (n > 0) events += n / sizeof(struct input_event);
        }
    }

    return events;
}

int wubu_evdev_key_down(uint32_t key) {
    /* Simple state tracking - in production would maintain key state array */
    struct input_event ev[32];
    if (g_input.kbd_fd >= 0) {
        int n = read(g_input.kbd_fd, ev, sizeof(ev));
        for (int i = 0; i < n / (int)sizeof(struct input_event); i++) {
            if (ev[i].type == EV_KEY && ev[i].code == key && ev[i].value == 1) return 1;
        }
    }
    return 0;
}

void wubu_evdev_mouse_pos(int *x, int *y) {
    static int mx = 0, my = 0;
    struct input_event ev[32];
    if (g_input.mouse_fd >= 0) {
        int n = read(g_input.mouse_fd, ev, sizeof(ev));
        for (int i = 0; i < n / (int)sizeof(struct input_event); i++) {
            if (ev[i].type == EV_REL) {
                if (ev[i].code == REL_X) mx += ev[i].value;
                else if (ev[i].code == REL_Y) my += ev[i].value;
            }
        }
    }
    *x = mx;
    *y = my;
}
