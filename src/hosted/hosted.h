/*
 * hosted.h — WuBuOS Hosted Mode (Linux emu-style launcher)
 *
 * WuBuOS can run as a Linux program via X11 window, or standalone
 * on bare-metal. This is the hosted "blob" — the OS as a regular
 * executable that sets up the full WuBuOS environment.
 *
 * Architecture:
 *   X11 window → VBE framebuffer → WuBuOS GUI/Temple REPL
 *   Styx server → /dev, /net, /wubu namespace (Unix socket)
 *   .wubu containers → loaded from host filesystem or embedded
 *   Input events → WuBuOS input queue
 */
#ifndef WUBU_HOSTED_H
#define WUBU_HOSTED_H

#include <stdint.h>
#include <stdbool.h>

/* ── Display Configuration ──────────────────────────────────────── */

#define HOSTED_DEFAULT_W 1024
#define HOSTED_DEFAULT_H 768
#define HOSTED_WIN_TITLE "WuBuOS — TempleOS Soul · Win98 Chrome"

/* ── Hosted Mode Flags ──────────────────────────────────────────── */

typedef enum {
    HMODE_NONE    = 0,
    HMODE_GUI     = 1,     /* Win98 desktop GUI */
    HMODE_TEMPLE  = 2,     /* HolyC REPL full-screen */
    HMODE_CONSOLE = 4,     /* Text console (no GUI) */
    HMODE_HEADLESS = 8,    /* No window (Styx-only server) */
} hosted_mode_t;

/* ── Hosted State ───────────────────────────────────────────────── */

typedef struct {
    /* Display */
    int      width, height;
    int      depth;           /* Bits per pixel */
    uint32_t *framebuffer;    /* VBE-compatible framebuffer */
    int      fb_pitch;        /* Bytes per row */
    
    /* X11 (stored as void* to avoid X11 dependency in header) */
    void    *display_ptr;     /* Display* */
    void    *window_ptr;      /* Window (uintptr_t cast) */
    void    *gc_ptr;          /* GC (uintptr_t cast) */
    void    *ximage_ptr;      /* XImage* */
    int      x11_fd;          /* X11 connection fd for select() */
    
    /* Mode */
    hosted_mode_t mode;
    bool     running;
    bool     fullscreen;
    
    /* Styx namespace */
    int      styx_fd;         /* Unix socket for Styx connections */
    
    /* Input state */
    int      mouse_x, mouse_y;
    int      mouse_buttons;
    char     key_map[256];    /* Current key states */
} hosted_state_t;

/* ── API ────────────────────────────────────────────────────────── */

/* Initialize hosted mode: create window, init VBE, start Styx */
int  hosted_init(hosted_state_t *state, int argc, char **argv);

/* Run the main event loop (blocks until exit) */
int  hosted_run(hosted_state_t *state);

/* Shutdown and cleanup */
void hosted_shutdown(hosted_state_t *state);

/* Copy VBE framebuffer to X11 window */
void hosted_blit(hosted_state_t *state);

/* Set GUI/Temple mode */
void hosted_set_mode(hosted_state_t *state, hosted_mode_t mode);

/* ── Styx Namespace Servers ─────────────────────────────────────── */

/* Initialize the Styx namespace for hosted mode:
 *   /wubu     — .wubu container filesystem root
 *   /dev/cons — console I/O
 *   /dev/env  — environment variables
 *   /dev/time — system clock
 *   /prog     — process info
 *   /net      — networking stubs
 */
int hosted_styx_init(hosted_state_t *state, const char *socket_path);

/* Register a .wubu container file in the namespace */
int hosted_styx_register_wubu(hosted_state_t *state,
                               const char *name, 
                               const uint8_t *data, uint32_t size);

#endif /* WUBU_HOSTED_H */
