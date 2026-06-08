/*
 * wubu_freedoom.c — WuBuOS FreeDoom Launcher
 *
 * Cell 391: FreeDoom running inside Arch container.
 *
 * This is "por qué no los dos" executing at runtime:
 *   - Win98 desktop gives you the Doom icon
 *   - Arch container gives you the Linux drivers + GPU
 *   - prboom-plus gives you the Doom engine (enhanced port)
 *   - freedoom gives you the free WADs
 *   - Container gives you the safety rail
 *
 * The game runs as a HOST PROCESS inside an Arch chroot.
 * The chroot root comes from the ramdisk module:
 *   - Container mode: /run/wubu/ramdisk (tmpfs, RAM)
 *   - Bare metal:     /var/wubu/roots/arch-base (SSD)
 *
 * It sees /dev/dri for GPU, /dev/snd for audio.
 * If it crashes, the container dies. WuBuOS lives.
 * That's the whole point of containers as guardrails.
 */
#include "wubu_freedoom.h"
#include "wubu_arch.h"
#include "wubu_ramdisk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

/* ── Init ───────────────────────────────────────────────────────── */

WubuDoom *wubu_doom_init(const char *arch_root) {
    WubuDoom *doom = (WubuDoom*)calloc(1, sizeof(WubuDoom));
    if (!doom) return NULL;

    const char *root = arch_root ? arch_root : WUBU_ARCH_ROOT_DEFAULT;
    strncpy(doom->arch_root, root, sizeof(doom->arch_root) - 1);

    doom->state = WUBU_DOOM_IDLE;
    doom->wad = WUBU_DOOM_PHASE1;
    doom->width = 1280;
    doom->height = 720;
    doom->fullscreen = false;
    doom->sound = true;
    doom->music = true;
    doom->skill = 3;  /* Hurt Me Plenty */
    doom->container = NULL;

    return doom;
}

void wubu_doom_destroy(WubuDoom *doom) {
    if (!doom) return;

    if (doom->state == WUBU_DOOM_RUNNING) {
        wubu_doom_stop(doom);
    }

    if (doom->container) {
        wubu_ct_destroy(doom->container);
        doom->container = NULL;
    }

    free(doom);
}

/* ── Install ────────────────────────────────────────────────────── */

int wubu_doom_install(WubuDoom *doom) {
    if (!doom) return -1;
    doom->state = WUBU_DOOM_INSTALLING;

    /* Install freedoom + prboom-plus into Arch root */
    const char *pkgs = "freedoom prboom-plus";
    int ret = wubu_arch_install(doom->arch_root, pkgs);
    if (ret != 0) {
        doom->state = WUBU_DOOM_FAILED;
        return -1;
    }

    /* Also ensure audio packages */
    ret = wubu_arch_install(doom->arch_root,
                            "pulseaudio pulseaudio-alsa alsa-utils sdl_mixer");
    if (ret != 0) {
        /* Non-fatal — game may still work with basic audio */
    }

    doom->state = WUBU_DOOM_IDLE;
    return 0;
}

/* ── Check Installed ────────────────────────────────────────────── */

bool wubu_doom_installed(WubuDoom *doom) {
    if (!doom) return false;

    char path[1024];

    /* Check for prboom-plus binary */
    snprintf(path, sizeof(path), "%s/usr/bin/prboom-plus", doom->arch_root);
    if (access(path, X_OK) != 0) return false;

    /* Check for at least one WAD */
    snprintf(path, sizeof(path), "%s%s/%s", doom->arch_root,
             WUBU_FREEDOOM_WAD_DIR, WUBU_FREEDOOM_PWAD_PHASE1);
    if (access(path, F_OK) != 0) return false;

    return true;
}

/* ── Launch ─────────────────────────────────────────────────────── */

int wubu_doom_launch(WubuDoom *doom) {
    if (!doom) return -1;
    if (doom->state == WUBU_DOOM_RUNNING) return -1;

    /* Install if not yet installed */
    if (!wubu_doom_installed(doom)) {
        if (wubu_doom_install(doom) != 0)
            return -1;
    }

    doom->state = WUBU_DOOM_LAUNCHING;

    /* Create Arch container for the game */
    doom->container = wubu_ct_native("freedoom", doom->arch_root);
    if (!doom->container) {
        doom->state = WUBU_DOOM_FAILED;
        return -1;
    }

    /* GPU passthrough — Doom needs the GPU for rendering */
    wubu_ct_set_gpu(doom->container, true);

    /* Audio — bind /dev/snd for sound */
    wubu_ct_add_bind(doom->container, "/dev/snd", "/dev/snd", false);

    /* PulseAudio socket */
    wubu_ct_add_bind(doom->container, "/run/user", "/run/user", false);

    /* Add DISPLAY for X11 (if not fullscreen DRM) */
    char display_env[32];
    snprintf(display_env, sizeof(display_env), "DISPLAY=%s",
             getenv("DISPLAY") ? getenv("DISPLAY") : ":0");
    wubu_ct_add_env(doom->container, display_env);

    /* SDL audio */
    wubu_ct_add_env(doom->container, "SDL_AUDIODRIVER=pulseaudio");

    /* Build prboom-plus command with args */
    const char *wad_file;
    switch (doom->wad) {
        case WUBU_DOOM_PHASE1: wad_file = WUBU_FREEDOOM_PWAD_PHASE1; break;
        case WUBU_DOOM_PHASE2: wad_file = WUBU_FREEDOOM_PWAD_PHASE2; break;
        case WUBU_DOOM_DM:     wad_file = WUBU_FREEDOOM_PWAD_FDM;    break;
        default:               wad_file = WUBU_FREEDOOM_PWAD_PHASE1; break;
    }

    char wad_path[512];
    snprintf(wad_path, sizeof(wad_path), "%s/%s",
             WUBU_FREEDOOM_WAD_DIR, wad_file);

    /* Skill names for prboom-plus */
    const char *skill_names[] = {
        NULL, "-skill", "-skill", "-skill", "-skill", "-skill"
    };
    char skill_arg[16] = "";
    if (doom->skill >= 1 && doom->skill <= 5) {
        snprintf(skill_arg, sizeof(skill_arg), "%d", doom->skill);
    }

    /* Build full command line:
     * prboom-plus -iwad <wad> -width W -height H [-fullscreen] [-skill N] */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "/usr/bin/prboom-plus -iwad %s -width %d -height %d %s %s %s %s",
             wad_path,
             doom->width, doom->height,
             doom->fullscreen ? "-fullscreen" : "",
             doom->sound ? "" : "-nosound",
             doom->music ? "" : "-nomusic",
             skill_arg[0] ? skill_names[doom->skill] : "",
             skill_arg[0] ? skill_arg : "");

    /* Parse command into argv */
    char *argv[32];
    int argc = 0;
    char cmd_copy[1024];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char *token = strtok(cmd_copy, " ");
    while (token && argc < 30) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    /* Set container command */
    wubu_ct_set_cmd(doom->container, argc, argv);

    /* Set uid to wubu user (uid 1000 typically) */
    doom->container->uid = 1000;
    doom->container->gid = 1000;

    /* Start container */
    int ret = wubu_ct_start(doom->container);
    if (ret != 0) {
        wubu_ct_destroy(doom->container);
        doom->container = NULL;
        doom->state = WUBU_DOOM_FAILED;
        return -1;
    }

    doom->state = WUBU_DOOM_RUNNING;
    return 0;
}

/* ── Stop ───────────────────────────────────────────────────────── */

int wubu_doom_stop(WubuDoom *doom) {
    if (!doom || !doom->container) return -1;

    int ret = wubu_ct_kill(doom->container, SIGTERM);
    if (ret == 0) {
        wubu_ct_wait(doom->container);
    }

    doom->state = WUBU_DOOM_EXITED;
    return ret;
}

/* ── State Query ────────────────────────────────────────────────── */

WubuDoomState wubu_doom_state(WubuDoom *doom) {
    if (!doom) return WUBU_DOOM_FAILED;

    /* Check if container process is still running */
    if (doom->state == WUBU_DOOM_RUNNING && doom->container) {
        CtState ct_state = wubu_ct_state(doom->container);
        if (ct_state == CT_EXITED || ct_state == CT_FAILED) {
            doom->state = WUBU_DOOM_EXITED;
        }
    }

    return doom->state;
}

/* ── Configuration ──────────────────────────────────────────────── */

void wubu_doom_set_resolution(WubuDoom *doom, int w, int h) {
    if (!doom) return;
    doom->width = w > 0 ? w : 640;
    doom->height = h > 0 ? h : 480;
}

void wubu_doom_set_fullscreen(WubuDoom *doom, bool fs) {
    if (doom) doom->fullscreen = fs;
}

void wubu_doom_set_wad(WubuDoom *doom, WubuDoomWad wad) {
    if (doom) doom->wad = wad;
}

void wubu_doom_set_skill(WubuDoom *doom, int skill) {
    if (doom && skill >= 1 && skill <= 5) doom->skill = skill;
}

void wubu_doom_set_audio(WubuDoom *doom, bool sound, bool music) {
    if (!doom) return;
    doom->sound = sound;
    doom->music = music;
}
