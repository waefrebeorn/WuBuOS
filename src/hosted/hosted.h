/*
 * hosted.h — WuBuOS Hosted Mode (Inferno emu-style launcher)
 *
 * WuBuOS runs as a Linux program via Wayland window, or standalone
 * on bare-metal. This is the hosted "blob" — the OS as a regular
 * executable that sets up the full WuBuOS environment.
 *
 * Architecture:
 *   Wayland surface → VBE/DRM framebuffer → WuBuOS GUI/Temple REPL
 *   Styx server → /dev, /net, /wubu namespace (Unix socket / 9P)
 *   .wubu containers → loaded from host filesystem or embedded
 *   Input events → WuBuOS input queue
 */
#ifndef WUBU_HOSTED_H
#define WUBU_HOSTED_H

#include <stdint.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════
 * Public Enums/Constants
 * ══════════════════════════════════════════════════════════════════ */

#define HOSTED_DEFAULT_W 1024
#define HOSTED_DEFAULT_H 768
#define HOSTED_WIN_TITLE "WuBuOS"

typedef enum {
    HMODE_NONE     = 0,
    HMODE_GUI      = 1,     /* Win98 desktop GUI */
    HMODE_TEMPLE   = 2,     /* HolyC REPL full-screen */
    HMODE_CONSOLE  = 4,     /* Text console (no GUI) */
    HMODE_HEADLESS = 8,     /* No window (Styx-only server) */
} hosted_mode_t;

/* ══════════════════════════════════════════════════════════════════
 * HOSTED_STATE — public state
 * ══════════════════════════════════════════════════════════════════ */

struct HOSTED_STATE {
    int              width;
    int              height;
    int              depth;
    int              fb_pitch;
    uint32_t        *framebuffer;
    bool             running;
    bool             fullscreen;
    hosted_mode_t    mode;
    void            *display_ptr;
    void            *window_ptr;
    void            *gc_ptr;
    void            *ximage_ptr;
    int              styx_fd;
    int              mouse_x;
    int              mouse_y;
    int              mouse_buttons;
    uint8_t          key_map[256];
};

typedef struct HOSTED_STATE hosted_state_t;

/* ══════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════ */

int  hosted_init(hosted_state_t *state, int argc, char **argv);
int  hosted_run(hosted_state_t *state);
void hosted_shutdown(hosted_state_t *state);

void hosted_set_mode(hosted_state_t *state, hosted_mode_t mode);

const char *hosted_mode_name(hosted_mode_t mode);

/* Styx namespace servers */
int hosted_styx_init(hosted_state_t *state, const char *socket_path);
int hosted_styx_register_wubu(hosted_state_t *state,
                               const char *name,
                               const uint8_t *data, uint32_t size);

/* Behavioral Test API */
int  hosted_kernel_ready(void);
int  hosted_wm_has_windows(void);
void hosted_fs_reset(void);

void hosted_blit(hosted_state_t *state);

#endif /* WUBU_HOSTED_H */
