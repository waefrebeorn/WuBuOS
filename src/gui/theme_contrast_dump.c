/* Dump the theme's text/background pairs as hex; the audit's arithmetic lives in Python (WCAG's
 * formula implemented once, in the tool, so the product does not carry a second copy of it). */
#include <stdio.h>

#include "wubu_theme.h"

#define PAIR(lhs, rhs) printf("%d %s %06X %06X\n", (int)i, #lhs "/" #rhs, c->lhs & 0xFFFFFFu, c->rhs & 0xFFFFFFu)

int main(void)
{
    for (int i = 0; i < THEME_COUNT; i++) {
        wubu_theme_set((WubuThemeId)i);
        const WubuThemeColors *c = wubu_theme_colors();
        if (!c) continue;
        PAIR(win_title_text, win_title_active);
        PAIR(win_title_text_ina, win_title_inactive);
        PAIR(body_text, win_face);
        PAIR(body_text_ina, win_face);
        PAIR(select_text, select_bg);
        PAIR(btn_text, btn_face);
        PAIR(start_btn_text, start_btn_face);
        PAIR(startmenu_text, startmenu_bg);
        PAIR(icon_text, desktop_bg);
    }
    return 0;
}
