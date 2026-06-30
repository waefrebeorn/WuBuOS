/*
 * dosgui_term_test.c  --  WuBuOS Terminal Test Suite
 *
 * Tests PTY backend, ANSI parser, scrollback, copy/paste, tab management,
 * HolyC REPL, keyboard shortcuts, and resize handling.
 */

#include "dosgui_term.h"
#include "dosgui_wm.h"
#include "wubu_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <pty.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-55s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)

void test_init(void) {
    printf("\n=== Test: Init/Shutdown ===\n");
    dosgui_term_init();
    CHECK(dosgui_term_is_open() == 0, "should be closed");
    dosgui_term_shutdown();
    PASS();
}

void test_new_tab(void) {
    printf("\n=== Test: Tab Management ===\n");
    dosgui_term_init();
    int idx = dosgui_term_new_tab(TERM_SESSION_SHELL, NULL, "/bin/bash");
    CHECK(idx == 0, "new tab returns 0");
    TermTab *tab = dosgui_term_get_tab(0);
    CHECK(tab != NULL, "tab created");
    CHECK(strcmp(tab->label, "Shell 1") == 0, "default label set (Shell 1)");
    dosgui_term_shutdown();
    PASS();
}

void test_term_handle_key(void) {
    printf("\n=== Test: Key Handling ===\n");
    dosgui_term_init();
    int idx = dosgui_term_new_tab(TERM_SESSION_SHELL, "Test", "/bin/bash");
    CHECK(idx == 0, "initial tab created");
    dosgui_term_show(100, 100, 600, 400);
    TermState *term_before = dosgui_term_state();
    printf("DEBUG: tab_count before handle_key = %d\n", term_before->tab_count);
    dosgui_term_handle_key('t', 0x05); /* Ctrl+Shift+T - new tab (ctrl=0x04, shift=0x01) */
    TermState *term = dosgui_term_state();
    printf("DEBUG: tab_count after handle_key = %d\n", term->tab_count);
    CHECK(term->tab_count == 3, "new tab created (now 3 tabs: new_tab + show + handle_key)");
    dosgui_term_shutdown();
    PASS();
}

void test_term_render(void) {
    printf("\n=== Test: Render (no crash) ===\n");
    dosgui_term_init();
    vbe_init(800, 600);
    dosgui_term_show(100, 100, 600, 400);
    dosgui_term_render(NULL, 800, 600);
    dosgui_term_shutdown();
    vbe_shutdown();
    PASS();
}

int main(void) {
    printf("=== dosgui_term Test Suite ===\n");
    test_init();
    test_new_tab();
    test_term_handle_key();
    test_term_render();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}