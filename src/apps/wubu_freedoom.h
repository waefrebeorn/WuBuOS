/*
 * wubu_freedoom.h  --  WuBuOS FreeDoom Launcher
 *
 * Cell 391: FreeDoom running inside Arch container.
 *
 * "Why Not Both"  --  Doom runs in the Arch container.
 * WuBuOS provides the desktop icon. Arch provides the drivers.
 * prboom-plus provides the engine. freedoom provides the WADs.
 *
 * Flow:
 *   1. User clicks Doom icon on Win98 desktop
 *   2. WuBuOS creates Arch container (if not running)
 *   3. WuBuOS installs freedoom + prboom-plus (if not installed)
 *   4. WuBuOS launches prboom-plus inside Arch container
 *   5. Game renders via DRM/KMS passthrough
 *
 * This is the "por qué no los dos" in action:
 *   - TempleOS soul: raw game engine, no abstraction theater
 *   - Arch kernel: real drivers, real GPU, real audio
 *   - Win98 shell: the icon you click to start it
 *   - Container: safety rail so game crash doesn't take OS down
 */
#ifndef WUBU_FREEDOOM_H
#define WUBU_FREEDOOM_H

#include <stdint.h>
#include <stdbool.h>
#include "wubu_host_exec.h"

/* -- FreeDoom Configuration --------------------------------------- */

#define WUBU_FREEDOOM_WAD_DIR      "/usr/share/doom"
#define WUBU_FREEDOOM_PWAD_PHASE1 "freedoom1.wad"
#define WUBU_FREEDOOM_PWAD_PHASE2 "freedoom2.wad"
#define WUBU_FREEDOOM_PWAD_FDM    "freedm.wad"

typedef enum {
    WUBU_DOOM_IDLE       = 0,   /* Not launched */
    WUBU_DOOM_INSTALLING = 1,   /* Installing freedoom + prboom-plus */
    WUBU_DOOM_LAUNCHING  = 2,   /* Container starting */
    WUBU_DOOM_RUNNING    = 3,   /* Game running */
    WUBU_DOOM_EXITED     = 4,   /* Game exited */
    WUBU_DOOM_FAILED     = 5,   /* Launch/install failed */
} WubuDoomState;

typedef enum {
    WUBU_DOOM_PHASE1 = 0,   /* FreeDoom: Phase 1 (E1M1-E4M9) */
    WUBU_DOOM_PHASE2 = 1,   /* FreeDoom: Phase 2 (MAP01-MAP32) */
    WUBU_DOOM_DM     = 2,   /* FreeDM (deathmatch) */
} WubuDoomWad;

typedef struct {
    WubuDoomState state;
    WubuDoomWad   wad;          /* Which WAD to use */
    WubuCt       *container;    /* Arch container running prboom-plus */
    char          arch_root[512]; /* Arch root path */
    int           width;        /* Window width */
    int           height;       /* Window height */
    bool          fullscreen;   /* Fullscreen mode */
    bool          sound;        /* Sound enabled */
    bool          music;        /* Music enabled */
    int           skill;        /* Skill level 1-5 */
} WubuDoom;

/* -- FreeDoom Lifecycle ------------------------------------------- */

/*
 * Initialize FreeDoom launcher.
 * Checks if Arch root exists, if freedoom is installed.
 * Returns: launcher handle (NULL on failure)
 */
WubuDoom *wubu_doom_init(const char *arch_root);

/*
 * Destroy FreeDoom launcher.
 * Stops game if running.
 */
void wubu_doom_destroy(WubuDoom *doom);

/*
 * Install freedoom + prboom-plus inside Arch root.
 * Runs: pacstrap <root> freedoom prboom-plus
 * Returns: 0 on success, -1 on failure
 */
int wubu_doom_install(WubuDoom *doom);

/*
 * Check if freedoom is installed in Arch root.
 * Looks for: /usr/bin/prboom-plus and WAD files
 */
bool wubu_doom_installed(WubuDoom *doom);

/*
 * Launch FreeDoom inside Arch container.
 * Creates container → sets GPU binds → execs prboom-plus
 *
 * Returns: 0 on launch, -1 on failure
 */
int wubu_doom_launch(WubuDoom *doom);

/*
 * Stop FreeDoom (sends SIGTERM to container).
 */
int wubu_doom_stop(WubuDoom *doom);

/*
 * Get FreeDoom state.
 */
WubuDoomState wubu_doom_state(WubuDoom *doom);

/* -- FreeDoom Configuration --------------------------------------- */

/*
 * Set game resolution.
 */
void wubu_doom_set_resolution(WubuDoom *doom, int w, int h);

/*
 * Set fullscreen mode.
 */
void wubu_doom_set_fullscreen(WubuDoom *doom, bool fs);

/*
 * Set which WAD to play.
 */
void wubu_doom_set_wad(WubuDoom *doom, WubuDoomWad wad);

/*
 * Set skill level (1=ITYTD, 2=HEY, 3=HURT, 4=ULTRA, 5=NIGHTMARE).
 */
void wubu_doom_set_skill(WubuDoom *doom, int skill);

/*
 * Enable/disable sound and music.
 */
void wubu_doom_set_audio(WubuDoom *doom, bool sound, bool music);

#endif /* WUBU_FREEDOOM_H */
