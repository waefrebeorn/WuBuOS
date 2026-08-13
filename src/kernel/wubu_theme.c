/*
 * wubu_theme.c  --  WuBuOS Metal Theme Engine + /theme Namespace
 *
 * The graphic set as a writable node tree. Presets seed the nodes;
 * writes go through the node table (EDR-counted); apply() re-derives
 * the draw struct. Renderers (vbe chrome, bonzi, the WM) read only
 * the applied struct -- so a node write re-skins the next frame.
 *
 * Freestanding: fixed tables, no malloc, no hosted APIs.
 */

#include "wubu_theme.h"

#include <stddef.h>
#include <stdio.h>   /* snprintf (kernel libc) */

/* -- The node tree -------------------------------------------------- */

typedef struct {
    const char *path;      /* e.g. "/theme/win/title_active" (rodata) */
    uint32_t    value;
} WubuThemeNode;

static WubuThemeNode g_nodes[WUBU_THEME_NODES];
static int           g_node_count;
static uint32_t      g_writes;    /* EDR audit counter */
static WubuKTheme    g_theme;

/* /theme Styx/9P node write-through observer (NULL = unlinked). The AGF
 * density planner (wubu_density_plan) registers here so a kernel theme change
 * triggers the planner's absorb/keep/prune cycle. NULL-checked on every
 * write so the engine stays freestanding without the planner linked. */
static wubu_theme_write_observer_fn g_theme_write_observer;

void wubu_theme_set_write_observer(wubu_theme_write_observer_fn fn)
{
    g_theme_write_observer = fn;
}

/* kernel-local string compare (no libc dependency) */
static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int node_find(const char *path)
{
    for (int i = 0; i < g_node_count; i++)
        if (g_nodes[i].path && str_eq(g_nodes[i].path, path))
            return i;
    return -1;
}

static void node_set(const char *path, uint32_t value)
{
    int i = node_find(path);
    if (i < 0 && g_node_count < WUBU_THEME_NODES) {
        i = g_node_count++;
        g_nodes[i].path = path;
    }
    if (i >= 0) {
        g_nodes[i].value = value;
        g_writes++;
    }
}

int wubu_theme_node_set(const char *path, uint32_t value)
{
    if (!path || node_find(path) < 0) return -1;
    node_set(path, value);
    /* /theme Styx/9P node write-through: a successful node write flows
     * through to any registered observer (e.g. the AGF density planner),
     * so a kernel theme change triggers the planner's absorb/keep/prune
     * cycle. */
    if (g_theme_write_observer)
        g_theme_write_observer(path, value);
    return 0;
}

int wubu_theme_node_get(const char *path, uint32_t *out)
{
    int i = node_find(path);
    if (i < 0) return -1;
    if (out) *out = g_nodes[i].value;
    return 0;
}

int wubu_theme_node_list(char *buf, int bufsz)
{
    int off = 0;
    for (int i = 0; i < g_node_count && off < bufsz - 1; i++) {
        int n = snprintf(buf + off, (size_t)(bufsz - off), "%s = %08x\n",
                         g_nodes[i].path, (unsigned)g_nodes[i].value);
        if (n < 0) break;
        off += n;
    }
    if (bufsz > 0) buf[off < bufsz ? off : bufsz - 1] = '\0';
    return g_node_count;
}

uint32_t wubu_theme_write_count(void) { return g_writes; }

/* -- Presets -------------------------------------------------------- */

static void preset_win98(void)
{
    node_set("/theme/desktop/bg",          0x00808080u);
    node_set("/theme/win/face",            0x00C0C0C0u);
    node_set("/theme/win/title_active",    0x00000080u);
    node_set("/theme/win/title_text",      0x00FFFFFFu);
    node_set("/theme/border/light",        0x00FFFFFFu);
    node_set("/theme/border/dark",         0x00000000u);
    node_set("/theme/btn/face",            0x00C0C0C0u);
    node_set("/theme/btn/text",            0x00000000u);
    node_set("/theme/taskbar/bg",          0x00C0C0C0u);
    node_set("/theme/select/bg",           0x00000080u);
    node_set("/theme/select/text",         0x00FFFFFFu);
    node_set("/theme/gorilla/fur",         0x00806040u);
    node_set("/theme/gorilla/belly",       0x00C0A080u);
    node_set("/theme/speech/bubble",       0x00FFFFFFu);
    node_set("/theme/speech/border",       0x00402070u);
    node_set("/theme/chrome/rounded",      0x00000000u);
    node_set("/theme/chrome/gradient",     0x00000000u);
    node_set("/theme/chrome/luna_start",   0x00000000u);
}

static void preset_luna(void)
{
    preset_win98();
    node_set("/theme/desktop/bg",          0x003A6EA5u);
    node_set("/theme/win/title_active",    0x00084992u);
    node_set("/theme/win/title_text",      0x00FFFFFFu);
    node_set("/theme/select/bg",           0x003169ACu);
    node_set("/theme/gorilla/fur",         0x00704A20u);
    node_set("/theme/speech/border",       0x001F4E79u);
    node_set("/theme/chrome/rounded",      0x00000001u);
    node_set("/theme/chrome/gradient",     0x00000001u);
    node_set("/theme/chrome/luna_start",   0x00000001u);
}

static void preset_wubu(void)
{
    preset_luna();
    node_set("/theme/desktop/bg",          0x00003020u);
    node_set("/theme/win/title_active",    0x00006040u);
    node_set("/theme/select/bg",           0x00005030u);
    node_set("/theme/gorilla/fur",         0x00806040u);
    node_set("/theme/speech/border",       0x0000A060u);
}

static void preset_orange(void)
{
    preset_win98();
    node_set("/theme/desktop/bg",          0x00101018u);
    node_set("/theme/win/title_active",    0x00A04000u);
    node_set("/theme/win/title_text",      0x00FFFFFFu);
    node_set("/theme/select/bg",           0x00A04000u);
    node_set("/theme/gorilla/fur",         0x00A05020u);
    node_set("/theme/speech/border",       0x00E07000u);
    node_set("/theme/chrome/rounded",      0x00000001u);
}

void wubu_theme_apply(void)
{
    uint32_t v;
    WubuKTheme *t = &g_theme;
#define N(path, field) \
    if (wubu_theme_node_get(path, &v) == 0) t->field = v;
    N("/theme/desktop/bg", desktop_bg);
    N("/theme/win/face", win_face);
    N("/theme/win/title_active", win_title_active);
    N("/theme/win/title_text", win_title_text);
    N("/theme/border/light", border_light);
    N("/theme/border/dark", border_dark);
    N("/theme/btn/face", btn_face);
    N("/theme/btn/text", btn_text);
    N("/theme/taskbar/bg", taskbar_bg);
    N("/theme/select/bg", select_bg);
    N("/theme/select/text", select_text);
    N("/theme/gorilla/fur", gorilla_fur);
    N("/theme/gorilla/belly", gorilla_belly);
    N("/theme/speech/bubble", speech_bubble);
    N("/theme/speech/border", speech_border);
    N("/theme/chrome/rounded", rounded_buttons);
    N("/theme/chrome/gradient", gradient_title);
    N("/theme/chrome/luna_start", luna_start_button);
#undef N
    t->rounded_buttons = t->rounded_buttons != 0;
    t->gradient_title  = t->gradient_title  != 0;
    t->luna_start_button = t->luna_start_button != 0;
}

const WubuKTheme *wubu_theme_get(void) { return &g_theme; }

int wubu_theme_preset(WubuKThemeId id)
{
    switch (id) {
    case KTHEME_WIN98:  preset_win98();  break;
    case KTHEME_LUNA:   preset_luna();   break;
    case KTHEME_WUBU:   preset_wubu();   break;
    case KTHEME_ORANGE: preset_orange(); break;
    default: return -1;
    }
    wubu_theme_apply();
    return 0;
}

const char *wubu_theme_name(WubuKThemeId id)
{
    switch (id) {
    case KTHEME_WIN98:  return "WIN98";
    case KTHEME_LUNA:   return "LUNA";
    case KTHEME_WUBU:   return "WUBU";
    case KTHEME_ORANGE: return "ORANGE";
    default:            return "?";
    }
}

void wubu_theme_cycle(void)
{
    static WubuKThemeId cur = KTHEME_WIN98;
    cur = (WubuKThemeId)((cur + 1) % KTHEME_COUNT);
    wubu_theme_preset(cur);
}

void wubu_theme_init(void)
{
    g_node_count = 0;
    g_writes = 0;
    wubu_theme_preset(KTHEME_WIN98);
}
