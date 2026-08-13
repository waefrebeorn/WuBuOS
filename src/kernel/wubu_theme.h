/*
 * wubu_theme.h  --  WuBuOS Metal Theme Engine + /theme Namespace
 *
 * The graphic set system (Kevin-Bacon pass 3): the display is a WRITABLE
 * namespace.  Every drawable color/flag is a node with a hierarchical
 * path (/theme/...); the AGI (or the human via the console) writes a
 * node and the next frame re-renders with it -- no recompile, no rebuild.
 *
 * Every node write bumps the EDR write counter (the audit trail the
 * capability model logs); presets fill the table; wubu_theme_apply()
 * re-derives the draw struct from the nodes.
 *
 * Freestanding: fixed tables, no malloc.
 */
#ifndef WUBU_THEME_H
#define WUBU_THEME_H

#include <stdint.h>
#include <stdbool.h>

/* -- Preset theme ids (metal subset) ----------------------------- */

typedef enum {
    KTHEME_WIN98  = 0,
    KTHEME_LUNA   = 1,
    KTHEME_WUBU   = 2,
    KTHEME_ORANGE = 3,
    KTHEME_COUNT  = 4,
} WubuKThemeId;

/* -- The applied draw struct (what renderers read) ---------------- */

typedef struct {
    /* Desktop */
    uint32_t desktop_bg;

    /* Window chrome */
    uint32_t win_face;
    uint32_t win_title_active;
    uint32_t win_title_text;
    uint32_t border_light;
    uint32_t border_dark;

    /* Buttons */
    uint32_t btn_face;
    uint32_t btn_text;

    /* Taskbar / selection */
    uint32_t taskbar_bg;
    uint32_t select_bg;
    uint32_t select_text;

    /* Bonzi / gorilla accents */
    uint32_t gorilla_fur;
    uint32_t gorilla_belly;
    uint32_t speech_bubble;
    uint32_t speech_border;

    /* Flags */
    bool rounded_buttons;
    bool gradient_title;
    bool luna_start_button;
} WubuKTheme;

/* -- Namespace API (the self-modifying graphic set) -------------- */

/* Max nodes + path length in the /theme tree. */
#define WUBU_THEME_NODES  32
#define WUBU_THEME_PATH   48

/* Write a node: "/theme/win/title_active" = 0xff0000.
 * Bumps the EDR write counter. Returns 0 on success, -1 unknown path. */
int  wubu_theme_node_set(const char *path, uint32_t value);

/* Register a write-through observer: invoked with (path, value) on every
 * successful node_set(). This is the "/theme" Styx/9P node write-through --
 * the AGF density planner (wubu_density_plan) registers here so a kernel
 * theme change triggers the planner's absorb/keep/prune cycle. NULL clears. */
typedef void (*wubu_theme_write_observer_fn)(const char *path, uint32_t value);
void wubu_theme_set_write_observer(wubu_theme_write_observer_fn fn);

/* Read a node. Returns 0 on success, -1 unknown. */
int  wubu_theme_node_get(const char *path, uint32_t *out);

/* List all nodes into a caller buffer (name = value\n lines).
 * Returns the number of nodes written. */
int  wubu_theme_node_list(char *buf, int bufsz);

/* Number of node writes since boot (the EDR audit counter). */
uint32_t wubu_theme_write_count(void);

/* Re-derive the applied theme from the nodes. Call after set(). */
void wubu_theme_apply(void);

/* The applied theme (renderers read this). */
const WubuKTheme *wubu_theme_get(void);

/* Load a preset into the nodes + apply. */
int  wubu_theme_preset(WubuKThemeId id);
void wubu_theme_cycle(void);
const char *wubu_theme_name(WubuKThemeId id);

/* Init: load WIN98 preset. */
void wubu_theme_init(void);

#endif /* WUBU_THEME_H */
