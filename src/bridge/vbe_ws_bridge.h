/*
 * vbe_ws_bridge.h — VBE ↔ WorldSim Render Bridge
 *
 * Wires WorldSim's software renderer to WuBuOS's VBE framebuffer.
 * This is Cell 070: the connection between:
 *   - ws_render_ctx_t.fb → VBE back-buffer
 *   - Game loop: sim step → render → vbe_swap
 *   - Camera/viewport management
 *   - HUD overlay drawing
 *
 * Architecture:
 *   WorldSim renders to ws_render_ctx_t.fb
 *   Bridge sets that fb pointer to VBE's back-buffer
 *   After rendering, vbe_swap() flips to front
 *
 * All C11, no external deps.
 */

#ifndef WUBU_VBE_WS_BRIDGE_H
#define WUBU_VBE_WS_BRIDGE_H

#include <stdint.h>
#include "../kernel/vbe.h"
#include "../worldsim/worldsim.h"

/* ── Bridge State ──────────────────────────────────────────── */

typedef enum {
    BRIDGE_STOPPED = 0,
    BRIDGE_RUNNING = 1,
    BRIDGE_PAUSED  = 2
} bridge_state_t;

typedef struct {
    /* Connection status */
    bridge_state_t state;
    int            wired;       /* 1 if VBE↔WorldSim connection is active */

    /* Viewport / Camera */
    int    view_x, view_y;     /* camera position (world coords) */
    float  view_zoom;          /* zoom level (1.0 = 1:1 pixel) */
    int    view_w, view_h;     /* viewport dimensions (screen pixels) */

    /* Simulation */
    ws_simulation_t *sim;      /* pointer to active simulation */
    uint64_t         frame_count;
    uint64_t         tick_accum; /* fractional tick accumulator */
    float            sim_speed;  /* simulation speed multiplier (1.0 = real-time) */

    /* HUD configuration */
    uint8_t show_hud;          /* 1 = draw HUD overlay */
    uint8_t show_minimap;      /* 1 = draw minimap */
    uint8_t show_fps;          /* 1 = show FPS counter */
    uint8_t show_entity_count; /* 1 = show entity count */
    int     minimap_size;      /* minimap pixel size (default 128) */
    int     minimap_x, minimap_y; /* minimap position */

    /* Performance stats */
    uint64_t last_frame_ns;    /* nanoseconds for last frame */
    float    fps;              /* computed FPS */

    /* VBE reference (set on wire) */
    VBEState *vbe;
} vbe_ws_bridge_t;

/* ── Lifecycle ─────────────────────────────────────────────── */

/* Initialize bridge (does NOT wire — call wire after vbe_init + sim_init) */
void vbe_ws_bridge_init(vbe_ws_bridge_t *br);

/* Wire VBE back-buffer to WorldSim render context.
 * After this call, sim->render.fb points to VBE back-buffer.
 * Requires: vbe_init() already called, sim already initialized. */
int vbe_ws_bridge_wire(vbe_ws_bridge_t *br, ws_simulation_t *sim);

/* Unwire — disconnect WorldSim render from VBE */
void vbe_ws_bridge_unwire(vbe_ws_bridge_t *br);

/* Start the render loop */
void vbe_ws_bridge_start(vbe_ws_bridge_t *br);

/* Pause/resume */
void vbe_ws_bridge_pause(vbe_ws_bridge_t *br);
void vbe_ws_bridge_resume(vbe_ws_bridge_t *br);

/* Stop render loop */
void vbe_ws_bridge_stop(vbe_ws_bridge_t *br);

/* ── Per-Frame Operations ──────────────────────────────────── */

/* Advance simulation by one tick and render to VBE.
 * Called from main loop or timer interrupt.
 * Steps the sim, renders terrain + entities + minimap + HUD,
 * then calls vbe_swap(). */
void vbe_ws_bridge_frame(vbe_ws_bridge_t *br);

/* Render only — no sim step. Use for paused mode or screenshots. */
void vbe_ws_bridge_render_only(vbe_ws_bridge_t *br);

/* ── Camera / Viewport ─────────────────────────────────────── */

/* Move camera by deltas (pixels) */
void vbe_ws_bridge_pan(vbe_ws_bridge_t *br, int dx, int dy);

/* Set camera position directly (world coords) */
void vbe_ws_bridge_set_view(vbe_ws_bridge_t *br, int x, int y);

/* Zoom in/out. zoom_delta: positive = zoom in, negative = zoom out. */
void vbe_ws_bridge_zoom(vbe_ws_bridge_t *br, float zoom_delta);

/* Center camera on a world position */
void vbe_ws_bridge_center_on(vbe_ws_bridge_t *br, int wx, int wy);

/* ── HUD Overlay Drawing ───────────────────────────────────── */

/* Draw HUD elements to VBE back-buffer:
 * - FPS counter (top-left)
 * - Entity count (top-left, below FPS)
 * - Minimap (top-right)
 * - Camera position (bottom-left) */
void vbe_ws_bridge_draw_hud(vbe_ws_bridge_t *br);

/* Draw a text string to VBE at (x,y) using a simple 8x16 bitmap font.
 * Color is XRGB8888. Returns x + len*8 (next character position). */
int vbe_ws_bridge_text(vbe_ws_bridge_t *br, int x, int y,
                        const char *text, uint32_t color);

/* Draw an integer value as text */
int vbe_ws_bridge_text_int(vbe_ws_bridge_t *br, int x, int y,
                            int value, uint32_t color);

/* ── Query ──────────────────────────────────────────────────── */

/* Is the bridge wired and running? */
int vbe_ws_bridge_is_active(const vbe_ws_bridge_t *br);

/* Get current frame count */
uint64_t vbe_ws_bridge_frame_count(const vbe_ws_bridge_t *br);

/* Get current FPS */
float vbe_ws_bridge_fps(const vbe_ws_bridge_t *br);

#endif /* WUBU_VBE_WS_BRIDGE_H */
