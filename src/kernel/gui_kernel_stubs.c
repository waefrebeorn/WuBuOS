/* gui_kernel_stubs.c -- bare-metal provisions for GUI objects that assume a
 * hosted environment.
 *
 * The window manager itself (dosgui_wm, wubu_wm, gui_dbuf, wubu_theme) is
 * freestanding and now links into the kernel. The wallpaper loader is not: it
 * uses access/fopen/fread/fseek from stdio, which the kernel does not provide.
 *
 * Rather than pull a stdio emulation into ring 0 for a background image, the
 * kernel supplies a solid-colour wallpaper. This is the same decision a real
 * OS makes when the framebuffer console comes up before the filesystem.
 */
#include <stdint.h>
#include "vbe.h"
#include "../gui/wubu_theme.h"

void load_default_wallpaper(void)
{
    /* Fill the framebuffer with the theme's desktop colour. */
    vbe_clear(0x001E3A5Fu);
    vbe_swap();
}

/* The kernel's own wubu_theme.c predates the GUI theme API (it conflicts on the
 * shared symbols), so the two accessors the window manager needs are provided
 * here with the stock Win98-ish palette. */
void wubu_theme_shutdown(void)
{
    /* nothing to tear down before the framebuffer is even visible */
}

const WubuThemeColors *wubu_theme_colors(void)
{
    static WubuThemeColors c;
    c.desktop_bg = 0x001E3A5Fu;
    c.win_face = 0x00C0C0C0u;
    c.win_title_active = 0x00008080u;
    c.win_title_inactive = 0x00808080u;
    c.win_title_text = 0x00FFFFFFu;
    c.win_title_text_ina = 0x00C0C0C0u;
    c.border_light = 0x00FFFFFFu;
    c.border_face = 0x00C0C0C0u;
    c.border_dark = 0x00808080u;
    return &c;
}
