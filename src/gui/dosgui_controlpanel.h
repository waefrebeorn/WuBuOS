/*
 * dosgui_controlpanel.h  --  WuBuOS Control Panel
 *
 * Phase 4.5: Win98/XP style Control Panel with applet system.
 * Applets: Display, Network, Sound, Theme/Personalization
 */

#ifndef WUBU_DOSGUI_CONTROLPANEL_H
#define WUBU_DOSGUI_CONTROLPANEL_H

#include <stdint.h>
#include <stdbool.h>

/* -- Applet System ------------------------------------------------- */

#define CP_MAX_APPLETS        8
#define CP_MAX_RECENT_ENTRIES 16
#define CP_SIDEBAR_W          180
#define CP_CONTENT_MARGIN     16

typedef enum {
    CP_APPLET_DISPLAY     = 0,
    CP_APPLET_NETWORK     = 1,
    CP_APPLET_SOUND       = 2,
    CP_APPLET_THEME       = 3,
    CP_APPLET_KEYBOARD    = 4,
    CP_APPLET_MOUSE       = 5,
    CP_APPLET_PROGRAMS    = 6,  /* Add/Remove Programs */
    CP_APPLET_SYSTEM      = 7,  /* System properties */
} CpAppletId;

typedef struct CpApplet CpApplet;

/* Applet lifecycle callbacks */
typedef void  (*CpAppletInitFn)    (CpApplet *applet);
typedef void  (*CpAppletRenderFn)  (CpApplet *applet, uint32_t *fb, int x, int y, int w, int h);
typedef void  (*CpAppletMouseFn)   (CpApplet *applet, int x, int y, int btn, int kind);
typedef void  (*CpAppletKeyFn)     (CpApplet *applet, uint32_t key, uint32_t mods);
typedef void  (*CpAppletCleanupFn) (CpApplet *applet);

struct CpApplet {
    CpAppletId          id;
    char                name[48];
    char                desc[128];
    uint32_t            icon_color;
    CpAppletInitFn      init;
    CpAppletRenderFn    render;
    CpAppletMouseFn     mouse;
    CpAppletKeyFn       key;
    CpAppletCleanupFn   cleanup;
    void               *user_data;      /* Applet-specific state */
    bool                initialized;
};

/* -- Control Panel Manager ---------------------------------------- */

typedef struct {
    CpApplet            applets[CP_MAX_APPLETS];
    int                 applet_count;
    int                 active_applet;    /* Currently selected in sidebar */
    bool                open;
    int                 win_id;           /* Window ID if open in window */
    int                 sidebar_scroll;   /* For many applets */
} ControlPanel;

/* Global instance */
extern ControlPanel g_controlpanel;

/* -- Public API ---------------------------------------------------- */

/* Lifecycle */
void dosgui_controlpanel_init(void);
void dosgui_controlpanel_shutdown(void);

/* Applet registration */
int  dosgui_controlpanel_register_applet(const CpApplet *applet);
void dosgui_controlpanel_unregister_applet(CpAppletId id);
CpApplet *dosgui_controlpanel_get_applet(CpAppletId id);

/* Window management */
void dosgui_controlpanel_show(void);
void dosgui_controlpanel_hide(void);
bool dosgui_controlpanel_is_open(void);
void dosgui_controlpanel_toggle(void);

/* Input handling */
void dosgui_controlpanel_handle_key(uint32_t key, uint32_t mods);
void dosgui_controlpanel_handle_mouse(int x, int y, int btn, int kind);

/* Rendering (called from WM) */
void dosgui_controlpanel_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Built-in Applet Factories ------------------------------------ */
/* These create pre-configured applets for standard categories */

CpApplet dosgui_cp_create_display_applet(void);
CpApplet dosgui_cp_create_network_applet(void);
CpApplet dosgui_cp_create_sound_applet(void);
CpApplet dosgui_cp_create_theme_applet(void);

/* -- Display Applet Types ------------------------------------------ */

#define CP_MAX_RESOLUTIONS  16
#define CP_MAX_REFRESH_RATES 8

typedef enum {
    CP_ORIENT_LANDSCAPE      = 0,
    CP_ORIENT_PORTRAIT       = 1,
    CP_ORIENT_LANDSCAPE_FLIP = 2,
    CP_ORIENT_PORTRAIT_FLIP  = 3,
} CpOrientation;

typedef struct {
    int width;
    int height;
    int refresh_rates[CP_MAX_REFRESH_RATES];
    int refresh_count;
    int current_refresh_idx;
} CpResolution;

typedef struct {
    CpResolution  resolutions[CP_MAX_RESOLUTIONS];
    int           resolution_count;
    int           current_res_idx;
    int           current_dpi;      /* 96, 120, 144, 192 (100%, 125%, 150%, 200%) */
    CpOrientation orientation;
    int           monitor_count;
    struct {
        int x, y;
        int width, height;
        bool primary;
        bool enabled;
    } monitors[4];
    bool          pending_changes;
} CpDisplayState;

/* -- Network Applet Types ------------------------------------------ */

#define CP_MAX_INTERFACES  8
#define CP_MAX_WIFI_NETWORKS 32
#define CP_MAX_IP_ADDRS    4

typedef enum {
    CP_IF_TYPE_ETHERNET = 0,
    CP_IF_TYPE_WIFI     = 1,
    CP_IF_TYPE_VPN      = 2,
    CP_IF_TYPE_TETHER   = 3,
    CP_IF_TYPE_LOOPBACK = 4,
    CP_IF_TYPE_UNKNOWN  = 5,
} CpInterfaceType;

typedef enum {
    CP_IF_STATE_UP       = 0,
    CP_IF_STATE_DOWN     = 1,
    CP_IF_STATE_CONNECTING = 2,
    CP_IF_STATE_DISCONNECTED = 3,
} CpInterfaceState;

typedef enum {
    CP_WIFI_SEC_OPEN     = 0,
    CP_WIFI_SEC_WEP      = 1,
    CP_WIFI_SEC_WPA      = 2,
    CP_WIFI_SEC_WPA2     = 3,
    CP_WIFI_SEC_WPA3     = 4,
    CP_WIFI_SEC_ENTERPRISE = 5,
} CpWifiSecurity;

typedef struct {
    char            name[32];       /* e.g., "eth0", "wlan0" */
    char            friendly[64];   /* e.g., "Intel Wi-Fi 6E AX210" */
    CpInterfaceType type;
    CpInterfaceState state;
    char            mac[18];
    char            ipv4[CP_MAX_IP_ADDRS][16];
    int             ipv4_count;
    char            ipv6[CP_MAX_IP_ADDRS][40];
    int             ipv6_count;
    char            gateway[16];
    char            dns[2][16];
    int             signal_bars;    /* 0-4 for wifi */
    bool            is_default;
    bool            enabled;
} CpNetworkInterface;

typedef struct {
    char            ssid[64];
    CpWifiSecurity  security;
    int             signal_bars;    /* 0-4 */
    int             frequency;      /* MHz */
    bool            connected;
} CpWifiNetwork;

typedef struct {
    CpNetworkInterface interfaces[CP_MAX_INTERFACES];
    int                interface_count;
    CpWifiNetwork      wifi_networks[CP_MAX_WIFI_NETWORKS];
    int                wifi_count;
    int                selected_interface;
    int                selected_wifi;
    bool               scanning;
    char               passphrase[64];
    int                passphrase_len;
    bool               show_passphrase;
} CpNetworkState;

/* -- Sound Applet Types -------------------------------------------- */

#define CP_MAX_AUDIO_DEVICES 16

typedef enum {
    CP_AUDIO_OUTPUT = 0,
    CP_AUDIO_INPUT  = 1,
} CpAudioDirection;

typedef struct {
    char          name[64];
    char          description[128];
    CpAudioDirection direction;
    int           volume;         /* 0-100 */
    bool          mute;
    bool          is_default;
    int           channels;
    int           sample_rate;
    bool          active;         /* Currently playing/recording */
} CpAudioDevice;

typedef struct {
    CpAudioDevice output_devices[CP_MAX_AUDIO_DEVICES];
    int           output_count;
    CpAudioDevice input_devices[CP_MAX_AUDIO_DEVICES];
    int           input_count;
    int           master_volume;  /* 0-100 */
    bool          master_mute;
    int           selected_output;
    int           selected_input;
    bool          test_tone_playing;
} CpSoundState;

/* -- Theme/Personalization Applet Types ---------------------------- */

#define CP_MAX_THEMES        16
#define CP_MAX_CUSTOM_THEMES 8
#define CP_COLOR_ROLES       16

typedef enum {
    CP_COLOR_BG              = 0,
    CP_COLOR_FG              = 1,
    CP_COLOR_TITLE_ACTIVE    = 2,
    CP_COLOR_TITLE_INACTIVE  = 3,
    CP_COLOR_BUTTON_FACE     = 4,
    CP_COLOR_BUTTON_HOVER    = 5,
    CP_COLOR_BUTTON_PRESSED  = 6,
    CP_COLOR_BORDER_LIGHT    = 7,
    CP_COLOR_BORDER_DARK     = 8,
    CP_COLOR_BORDER_DARKEST  = 9,
    CP_COLOR_SELECT_BG       = 10,
    CP_COLOR_SELECT_TEXT     = 11,
    CP_COLOR_MENU_BG         = 12,
    CP_COLOR_MENU_TEXT       = 13,
    CP_COLOR_TOOLTIP_BG      = 14,
    CP_COLOR_DISABLED        = 15,
} CpColorRole;

typedef struct {
    char        name[48];
    uint32_t    colors[CP_COLOR_ROLES];
    bool        is_builtin;
    bool        rounded_buttons;
    bool        gradient_titles;
    bool        luna_start_button;
    char        wallpaper_path[256];
} CpTheme;

typedef struct {
    CpTheme       themes[CP_MAX_THEMES];
    int           theme_count;
    int           current_theme_idx;
    CpTheme       custom_themes[CP_MAX_CUSTOM_THEMES];
    int           custom_count;
    int           preview_theme_idx;    /* For live preview */
    char          wallpaper_path[256];
    int           wallpaper_mode;       /* 0=center, 1=tile, 2=stretch */
} CpThemeState;

/* -- Applet State Accessors ---------------------------------------- */

CpDisplayState *dosgui_cp_display_state(void);
CpNetworkState *dosgui_cp_network_state(void);
CpSoundState   *dosgui_cp_sound_state(void);
CpThemeState   *dosgui_cp_theme_state(void);

/* -- Helpers ------------------------------------------------------- */

const char *dosgui_cp_color_role_name(CpColorRole role);
const char *dosgui_cp_orientation_name(CpOrientation o);
const char *dosgui_cp_wifi_sec_name(CpWifiSecurity s);
const char *dosgui_cp_if_type_name(CpInterfaceType t);
const char *dosgui_cp_if_state_name(CpInterfaceState s);

#endif /* WUBU_DOSGUI_CONTROLPANEL_H */