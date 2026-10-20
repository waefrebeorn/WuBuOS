/* wubu_theme_file_test.c -- parser checks for the single-source theme file. */
#include <stdio.h>
#include <string.h>
#include "wubu_theme_file.h"

static int checks, failures;
static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("  FAIL %s\n", what); }
}

int main(void) {
    WubuThemeColors c;

    printf("== parse basics\n");
    memset(&c, 0xEE, sizeof c);
    int n = wubu_theme_parse("desktop_bg = #008080\nwin_face = C0C0C0\n", &c);
    check(n == 2, "two keys applied");
    check(c.desktop_bg == 0x008080, "desktop_bg parsed with '#'");
    check(c.win_face == 0xC0C0C0, "win_face parsed without '#'");
    check(c.btn_face == 0xEEEEEEEE, "unspecified key keeps its previous value");

    printf("== tolerance\n");
    memset(&c, 0, sizeof c);
    n = wubu_theme_parse("# a comment\n\n  \tdesktop_bg=#0a246a  \r\n", &c);
    check(n == 1, "comments, blank lines and CRLF are ignored");
    check(c.desktop_bg == 0x0A246A, "lowercase hex and surrounding spaces");

    printf("== rejection\n");
    memset(&c, 0, sizeof c);
    check(wubu_theme_parse("nonsense = #FFFFFF\n", &c) == -1, "unknown key rejected");
    check(wubu_theme_parse("desktop_bg = #FFFFFFF\n", &c) == -1, "7 hex digits rejected");
    check(wubu_theme_parse("desktop_bg = #GGGGGG\n", &c) == -1, "non-hex rejected");
    check(wubu_theme_parse("desktop_bg #FFFFFF\n", &c) == -1, "missing '=' rejected");
    check(wubu_theme_parse("desktop_bg = #FFFFFF junk\n", &c) == -1, "trailing junk rejected");
    check(wubu_theme_parse(NULL, &c) == -1, "NULL text rejected");

    printf("== key table\n");
    check(wubu_theme_key_name(0) != NULL, "first key has a name");
    check(wubu_theme_key_name(-1) == NULL, "negative index is NULL");
    check(wubu_theme_key_name(9999) == NULL, "out-of-range index is NULL");
    int found_title = 0;
    for (int i = 0; wubu_theme_key_name(i); i++)
        if (strcmp(wubu_theme_key_name(i), "win_title_active") == 0) found_title = 1;
    check(found_title, "win_title_active is in the table");

    printf("== file load\n");
    const char *p = "/tmp/wubu_theme_test.conf";
    FILE *f = fopen(p, "wb");
    check(f != NULL, "temp theme file created");
    if (f) {
        fputs("win_title_active = #0A246A\nbtn_hover = #D4D4D4\n", f);
        fclose(f);
        memset(&c, 0, sizeof c);
        n = wubu_theme_load_file(p, &c);
        check(n == 2, "file yields two keys");
        check(c.win_title_active == 0x0A246A && c.btn_hover == 0xD4D4D4,
              "file values land in the right fields");
        remove(p);
    }
    memset(&c, 0, sizeof c);
    check(wubu_theme_load_file("/tmp/definitely-not-here.conf", &c) == -2,
          "missing file returns -2");

    printf("=== wubu_theme_file: %d checks, %d failures ===\n", checks, failures);
    return failures ? 1 : 0;
}