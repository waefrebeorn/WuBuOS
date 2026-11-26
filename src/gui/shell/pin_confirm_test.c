/*
 * pin_confirm_test.c -- a confirmation that cannot be dismissed into a pin, a cancel that pins nothing,
 * and a preference that actually stops the asking.
 *
 * C11.
 */
#include "pin_confirm.h"

#include "../../kernel/vbe.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

static const WubuThemeColors kTheme = {
    .win_face = 0xFF202020u, .border_dark = 0xFF3A3A3A, .win_title_text = 0xFFFFFFFFu,
    .body_text = 0xFFE0E0E0u, .body_text_ina = 0xFF9A9A9Au, .select_bg = 0xFF0078D4u,
    .select_text = 0xFFFFFFFFu, .btn_face = 0xFF2D2D2Du, .btn_text = 0xFFE0E0E0u,
    .win_shadow = 0xFF000000u,
};
const WubuThemeColors *wubu_theme_colors(void) { return &kTheme; }

/* The pin the dialog applies -- recorded, so "cancel pins nothing" is a measurement. */
static int g_pins;
static char g_pinned_key[128];
void wubu_taskbar_pin(const char *key, const char *title, const char *exec)
{
    (void)title; (void)exec;
    g_pins++;
    snprintf(g_pinned_key, sizeof g_pinned_key, "%s", key ? key : "");
}

int main(void)
{
    vbe_init(1600, 900);
    char root[] = "/tmp/pintestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }
    char conf[400];
    snprintf(conf, sizeof conf, "%s/pin.conf", root);
    setenv("WUBU_PIN_CONF", conf, 1);
    pin_confirm_reset();

    printf("=== Test: asking is the default, and the card says what it will do ===\n");
    CHECK(pin_confirm_should_ask(), "a fresh profile asks before pinning");
    CHECK(!pin_confirm_is_open(), "nothing is open before it is opened");
    CHECK(pin_confirm_ask("app.term", "Terminal", "term", 1600, 900), "the card opens");

    const PinConfirmState *st = pin_confirm_state();
    CHECK(st->open, "the state reports open");
    CHECK(strstr(st->card.title, "Terminal") != NULL, "the heading names the app");
    CHECK(strcmp(st->card.footer, "Don't ask me again") == 0, "the card offers the preference");
    CHECK(!st->card.footer_checked, "the preference starts unticked");

    printf("\n=== Test: a cancel pins nothing ===\n");
    int bx, by, bw, bh;
    CHECK(cd_cancel_rect(&pin_confirm_state()->card, &bx, &by, &bw, &bh), "Cancel has a rect");
    g_pins = 0;
    CHECK(pin_confirm_click(bx + bw / 2, by + bh / 2, 1600, 900) == 2, "Cancel reports a cancel");
    CHECK(g_pins == 0, "cancelling applied no pin");
    CHECK(!pin_confirm_is_open(), "and the card closed");
    CHECK(pin_confirm_should_ask(), "cancelling did not change the preference");

    printf("\n=== Test: a press outside the card does nothing at all ===\n");
    CHECK(pin_confirm_ask("app.term", "Terminal", "term", 1600, 900), "the card reopens");
    g_pins = 0;
    CHECK(pin_confirm_click(5, 5, 1600, 900) == 0, "a stray press is not an answer");
    CHECK(pin_confirm_is_open(), "the card is still up");
    CHECK(g_pins == 0, "and nothing was pinned");

    printf("\n=== Test: accepting pins exactly once, with the right key ===\n");
    int okx, oky, okw, okh;
    CHECK(cd_ok_rect(&pin_confirm_state()->card, &okx, &oky, &okw, &okh), "Pin has a rect");
    CHECK(pin_confirm_click(okx + okw / 2, oky + okh / 2, 1600, 900) == 1, "Pin reports an accept");
    CHECK(g_pins == 1, "the pin was applied once");
    CHECK(strcmp(g_pinned_key, "app.term") == 0, "the pin carries the key the caller gave");
    CHECK(!pin_confirm_is_open(), "the card closed");

    printf("\n=== Test: the keyboard answers the same question ===\n");
    CHECK(pin_confirm_ask("app.paint", "Paint", "paint", 1600, 900), "the card opens");
    g_pins = 0;
    CHECK(pin_confirm_key(27, 1600, 900), "Esc is consumed");
    CHECK(g_pins == 0 && !pin_confirm_is_open(), "Esc cancels without pinning");
    CHECK(pin_confirm_ask("app.paint", "Paint", "paint", 1600, 900), "the card opens again");
    CHECK(pin_confirm_key(0x1C, 1600, 900), "Enter is consumed");
    CHECK(g_pins == 1 && !pin_confirm_is_open(), "Enter pins");

    printf("\n=== Test: 'don't ask me again' is remembered across restarts ===\n");
    CHECK(pin_confirm_ask("app.calc", "Calculator", "calc", 1600, 900), "the card opens");
    CHECK(cd_footer_rect(&pin_confirm_state()->card, &bx, &by, &bw, &bh), "the tick has a rect");
    CHECK(pin_confirm_click(bx + 6, by + 6, 1600, 900) == 0, "ticking is not an answer");
    CHECK(pin_confirm_state()->card.footer_checked, "the tick is on");
    CHECK(pin_confirm_click(okx + okw / 2, oky + okh / 2, 1600, 900) == 1, "Pin accepts");
    CHECK(!pin_confirm_should_ask(), "the preference now says do not ask");

    FILE *f = fopen(conf, "r");
    CHECK(f != NULL, "the preference was written");
    if (f) {
        char buf[256];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
        CHECK(strstr(buf, "ask=0") != NULL, "the file records ask=0");
    }

    pin_confirm_reset();                     /* a new session reads the file, not memory */
    CHECK(!pin_confirm_should_ask(), "the preference survives a restart");
    g_pins = 0;
    CHECK(!pin_confirm_ask("app.calc", "Calculator", "calc", 1600, 900),
          "when the answer is known, the card does not open");
    CHECK(!pin_confirm_is_open(), "and nothing is left open");
    CHECK(g_pins == 0, "the caller is expected to pin directly -- this module pinned nothing");

    printf("\n=== Test: a surface too small refuses to ask, and does not block the pin ===\n");
    pin_confirm_reset();
    remove(conf);
    CHECK(pin_confirm_ask("app.term", "Terminal", "term", 200, 100) == false,
          "the card cannot open on a surface that cannot hold it");
    CHECK(!pin_confirm_is_open(), "and it did not leave a half-open card");

    printf("\n=== Test: the card paints ===\n");
    remove(conf);
    pin_confirm_reset();
    CHECK(pin_confirm_ask("app.term", "Terminal", "term", 1600, 900), "the card opens for the paint test");
    vbe_init(1600, 900);
    pin_confirm_render(NULL, 1600, 900);
    int rx = 0, ry = 0, rw = 0, rh = 0;
    cd_rect(&pin_confirm_state()->card, &rx, &ry, &rw, &rh);
    CHECK(vbe_get_pixel(rx + 8, ry + rw / 2) != 0, "the card's surface is painted");

    char rm[400];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  pin_confirm: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
