/*
 * wubu_ns_de.c -- the /n/background/de DE-daemon framework surface (P3 §154).
 * Tray-slot ownership (XApp.StatusIcon) + desktop ownership (nemo-desktop),
 * published as /n/background/de files. C11, single-threaded.
 */
#include "wubu_deskenv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char slot[WUBU_DE_MAX_STR];
    char owner[WUBU_DE_MAX_STR];
    char used;
} wubu_deskenv_slot_t;

struct wubu_deskenv {
    wubu_deskenv_slot_t tray[WUBU_DE_MAX_SLOTS];
    int n_tray;
    char desktop[WUBU_DE_MAX_STR];
    int has_desktop;
};

wubu_deskenv_t *wubu_deskenv_create(void) {
    return (wubu_deskenv_t *)calloc(1, sizeof(wubu_deskenv_t));
}

void wubu_deskenv_destroy(wubu_deskenv_t *de) {
    if (de) free(de);
}

int wubu_deskenv_tray_claim(wubu_deskenv_t *de, const char *slot, const char *owner) {
    if (!de || !slot || !slot[0] || !owner || !owner[0]) return -1;
    for (int i = 0; i < de->n_tray; i++) {
        if (strcmp(de->tray[i].slot, slot) == 0) {
            if (strcmp(de->tray[i].owner, owner) == 0) return 0;  /* idempotent */
            return -1;                                            /* taken */
        }
    }
    if (de->n_tray >= WUBU_DE_MAX_SLOTS) return -1;
    wubu_deskenv_slot_t *s = &de->tray[de->n_tray++];
    s->used = 1;
    snprintf(s->slot, sizeof(s->slot), "%s", slot);
    snprintf(s->owner, sizeof(s->owner), "%s", owner);
    return 0;
}

int wubu_deskenv_tray_release(wubu_deskenv_t *de, const char *slot, const char *owner) {
    for (int i = 0; i < de->n_tray; i++) {
        if (strcmp(de->tray[i].slot, slot) == 0) {
            if (strcmp(de->tray[i].owner, owner) == 0) {
                /* shift the tail down to keep the array dense */
                de->tray[i] = de->tray[de->n_tray - 1];
                de->n_tray--;
                return 0;
            }
            return -1;   /* another owner holds it */
        }
    }
    return -1;
}

const char *wubu_deskenv_tray_owner(const wubu_deskenv_t *de, const char *slot) {
    for (int i = 0; i < de->n_tray; i++)
        if (strcmp(de->tray[i].slot, slot) == 0) return de->tray[i].owner;
    return NULL;
}

int wubu_deskenv_tray_count(const wubu_deskenv_t *de) {
    return de ? de->n_tray : 0;
}

int wubu_deskenv_desktop_set(wubu_deskenv_t *de, const char *owner) {
    if (!de || !owner) return -1;
    snprintf(de->desktop, sizeof(de->desktop), "%s", owner);
    de->has_desktop = 1;
    return 0;
}

void wubu_deskenv_desktop_clear(wubu_deskenv_t *de) {
    if (!de) return;
    de->desktop[0] = '\0';
    de->has_desktop = 0;
}

const char *wubu_deskenv_desktop_owner(const wubu_deskenv_t *de) {
    return (de && de->has_desktop) ? de->desktop : NULL;
}

int wubu_ns_publish_deskenv(wubu_deskenv_t *de) {
    if (!de) return -1;
    extern int ns_mkdir(const char *sub);
    extern int ns_write(const char *sub, const char *buf);
    if (ns_mkdir("background/de") != 0) return -1;

    /* tray slots: "<slot> <owner>" per line */
    char buf[4096];
    int off = 0;
    if (de->n_tray > 0) {
        for (int i = 0; i < de->n_tray && off < (int)sizeof(buf) - 4; i++)
            off += snprintf(buf + off, sizeof(buf) - (size_t)off, "%s %s\n",
                            de->tray[i].slot, de->tray[i].owner);
        if (ns_write("background/de/tray", buf) != 0) return -1;
    }
    char small[16];
    snprintf(small, sizeof(small), "%d\n", de->n_tray);
    if (ns_write("background/de/traycount", small) != 0) return -1;

    if (de->has_desktop)
        return ns_write("background/de/desktop", de->desktop);
    return ns_write("background/de/desktop", "none\n");
}
