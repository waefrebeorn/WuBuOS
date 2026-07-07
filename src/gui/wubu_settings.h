/*
 * wubu_settings.h  --  WuBuOS Settings Daemon
 *
 * Phase 2: GNOME-standard desktop services.
 * Persistent settings: theme, fonts, keyboard, mouse, display, a11y.
 * Stored in Styx namespace at /wubu/config/settings.json
 */

#ifndef WUBU_SETTINGS_H
#define WUBU_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

/* -- Desktop Icon Layout (persistent, ReactOS NTUSER.DAT-style) --- */
#define WUBU_ICON_LAYOUT_MAX 16
typedef struct {
    char name[32];      /* Icon display name (match key) */
    int  grid_x;        /* Persisted grid column */
    int  grid_y;        /* Persisted grid row */
    bool alive;         /* Entry in use */
} IconLayoutEntry;

/* -- Setting Categories ------------------------------------------- */

typedef enum {
    SETTINGS_CAT_THEME     = 0,  /* Theme, colors, wallpaper */
    SETTINGS_CAT_FONTS     = 1,  /* Font family, size, hinting, antialiasing */
    SETTINGS_CAT_KEYBOARD  = 2,  /* Repeat rate, delay, layout, shortcuts */
    SETTINGS_CAT_MOUSE     = 3,  /* Speed, acceleration, left-handed, double-click */
    SETTINGS_CAT_DISPLAY   = 4,  /* Resolution, scale, refresh, multi-monitor */
    SETTINGS_CAT_A11Y      = 5,  /* High contrast, screen reader, font scaling */
    SETTINGS_CAT_PRIVACY   = 6,  /* Telemetry, history, recent files */
    SETTINGS_CAT_COUNT
} SettingsCategory;

/* -- Theme Settings ----------------------------------------------- */

typedef struct {
    int theme_id;                    /* WubuThemeId */
    char wallpaper_path[256];        /* Path to wallpaper image */
    int wallpaper_mode;              /* 0=center, 1=tile, 2=stretch */
    bool use_custom_colors;          /* Override theme colors */
    uint32_t custom_colors[16];      /* Custom color overrides */
    IconLayoutEntry icon_layout[WUBU_ICON_LAYOUT_MAX];  /* Persisted desktop icon grid */
    int            icon_layout_count;
} ThemeSettings;

/* -- Font Settings ------------------------------------------------ */

typedef struct {
    char font_family[64];            /* e.g. "DejaVu Sans", "Tamsyn" */
    int font_size;                   /* Point size (8-72) */
    bool antialiasing;               /* Subpixel AA */
    bool hinting;                    /* Full/slight/none */
    int dpi;                         /* 96, 120, 144, etc. */
    char monospace_family[64];       /* For terminal/code */
    int monospace_size;
} FontSettings;

/* -- Keyboard Settings -------------------------------------------- */

typedef struct {
    int repeat_delay;                /* ms before repeat starts (200-1000) */
    int repeat_rate;                 /* repeats per second (10-50) */
    char layout[8];                  /* "us", "de", "fr", etc. */
    char variant[16];                /* "dvorak", "colemak", etc. */
    bool numlock_on;                 /* NumLock at startup */
    bool capslock_ctrl;              /* CapsLock as extra Ctrl */
    bool compose_key;                /* Right Alt as compose */
    int scroll_tick_lines;           /* Lines per scroll tick */
} KeyboardSettings;

/* -- Mouse Settings ----------------------------------------------- */

typedef struct {
    double speed;                    /* 0.5 - 3.0 */
    double acceleration;             /* 0.0 - 1.0 */
    bool left_handed;                /* Swap buttons */
    int double_click_time;           /* ms (200-500) */
    bool natural_scroll;             /* Content moves with fingers */
    int scroll_lines;                /* Lines per wheel click */
} MouseSettings;

/* -- Display Settings --------------------------------------------- */

typedef struct {
    int width;                       /* 800-7680 */
    int height;                      /* 600-4320 */
    int refresh_rate;                /* 30-240 Hz */
    double scale_factor;             /* 1.0, 1.25, 1.5, 1.75, 2.0 */
    bool fractional_scaling;         /* Allow non-integer scales */
    int primary_monitor;             /* Monitor index */
    bool mirror_displays;            /* Clone mode */
    int rotation[4];                 /* Per-monitor: 0, 90, 180, 270 */
} DisplaySettings;

/* -- Accessibility Settings --------------------------------------- */

typedef struct {
    bool high_contrast;              /* WCAG AA theme override */
    bool screen_reader;              /* speech-dispatcher integration */
    bool screen_magnifier;           /* Zoom follows focus */
    bool sticky_keys;                /* Modifiers latch */
    bool slow_keys;                  /* Key hold delay */
    bool bounce_keys;                /* Ignore rapid repeats */
    bool mouse_keys;                 /* Keypad moves pointer */
    double font_scale;               /* 1.0 - 2.0 UI scale */
    bool reduce_animations;          /* Disable transitions */
} A11ySettings;

/* -- Privacy Settings --------------------------------------------- */

typedef struct {
    bool remember_recent;            /* Track recent files/apps */
    bool remember_history;           /* Shell command history */
    bool telemetry;                  /* Anonymous usage stats */
    bool location_services;          /* App location access */
    int history_retention_days;      /* 0 = forever */
    bool auto_clear_temp;            /* Clean /tmp on boot */
} PrivacySettings;

/* -- Complete Settings Structure ---------------------------------- */

typedef struct {
    ThemeSettings   theme;
    FontSettings    fonts;
    KeyboardSettings keyboard;
    MouseSettings   mouse;
    DisplaySettings display;
    A11ySettings    a11y;
    PrivacySettings privacy;
    uint32_t version;                /* Config schema version */
    uint64_t last_modified;          /* Unix timestamp */
} WubuSettings;

/* -- Settings Daemon API ------------------------------------------ */

/* Initialize settings subsystem (loads from disk) */
int  wubu_settings_init(void);
void wubu_settings_shutdown(void);

/* Get current settings (read-only) */
const WubuSettings *wubu_settings_get(void);

/* Modify settings (caller must call save) */
WubuSettings *wubu_settings_mut(void);

/* Save to persistent storage */
int  wubu_settings_save(void);

/* Reset category to defaults */
void wubu_settings_reset_category(SettingsCategory cat);

/* Reset all to defaults */
void wubu_settings_reset_all(void);

/* Apply live changes (theme, font scale, etc.) */
void wubu_settings_apply_live(void);

/* Get config file path */
const char *wubu_settings_path(void);

/* -- Specific Helpers --------------------------------------------- */

uint32_t wubu_settings_get_font_scale(void);       /* Returns a11y.font_scale * 100 */
uint32_t wubu_settings_get_cursor_size(void);      /* From a11y */
bool     wubu_settings_high_contrast(void);        /* a11y.high_contrast */
bool     wubu_settings_reduce_motion(void);        /* a11y.reduce_animations */
const char *wubu_settings_font_family(void);       /* fonts.font_family */
int      wubu_settings_font_size(void);            /* fonts.font_size */

#endif /* WUBU_SETTINGS_H */