/*
 * wubu_syscall_test.c  --  WuBuOS Syscall Bridge Test Suite
 *
 * Tests the 26 TempleOS/ZealOS-compatible syscall handlers.
 * Tests the C handler functions directly (not via kernel syscall).
 */

#include "wubu_syscall.h"
#include "../kernel/vbe.h"
#include "../gui/dosgui_wm.h"
#include <stdio.h>
#include <string.h>

/* Weak stubs for VBE/WM functions when running unit test without GUI */
__attribute__((weak)) void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
__attribute__((weak)) void vbe_fill_circle(int cx, int cy, int r, uint32_t color) { (void)cx; (void)cy; (void)r; (void)color; }
__attribute__((weak)) void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) { (void)x; (void)y; (void)s; (void)color; (void)scale; }
__attribute__((weak)) void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale) { (void)x; (void)y; (void)ch; (void)color; (void)scale; }
__attribute__((weak)) void vbe_vline(int x, int y1, int y2, uint32_t color) { (void)x; (void)y1; (void)y2; (void)color; }
__attribute__((weak)) void vbe_hline(int x1, int x2, int y, uint32_t color) { (void)x1; (void)x2; (void)y; (void)color; }
__attribute__((weak)) int vbe_text_width(const char *s, int scale) { (void)s; (void)scale; return 0; }
__attribute__((weak)) void vbe_swap(void) {}
__attribute__((weak)) VBEState *vbe_state(void) { return NULL; }
__attribute__((weak)) DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title) { (void)x; (void)y; (void)w; (void)h; (void)title; return NULL; }
__attribute__((weak)) void dosgui_wm_destroy(DosGuiWindow *win) { (void)win; }
__attribute__((weak)) void dosgui_wm_set_focus(DosGuiWindow *win) { (void)win; }
__attribute__((weak)) DosGuiWindow *dosgui_wm_get_focused(void) { return NULL; }
__attribute__((weak)) DosGuiWindow *dosgui_wm_find_by_id(int id) { (void)id; return NULL; }
__attribute__((weak)) void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) { (void)fb; (void)fb_w; (void)fb_h; }

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("\u2705\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("\u274C %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Tests ------------------------------------------------------- */

static void test_syscall_constants(void) {
    TEST("wubu_syscall constants have expected values");
    CHECK(SYS_MAX_DEFINED == 26, "should have 26 syscalls defined");
    CHECK(SYS_VBE_FILL_RECT == 0, "VBEFillRect is 0");
    CHECK(SYS_VBE_SWAP == 7, "VBESwap is 7");
    CHECK(SYS_WM_CREATE_WIN == 8, "WMCreateWin is 8");
    CHECK(SYS_WM_RENDER == 12, "WMRender is 12");
    CHECK(SYS_FILE_OPEN == 13, "FileOpen is 13");
    CHECK(SYS_WUBU_EXEC == 23, "WubuExec is 23");
    CHECK(SYS_GET_TIME == 24, "GetTime is 24");
    CHECK(SYS_SLEEP == 25, "Sleep is 25");
    PASS();
}

static void test_syscall_trampolines(void) {
    TEST("wubu_syscall_trampoline returns valid pointers");
    for (int i = 0; i < 26; i++) {
        void *ptr = wubu_syscall_trampoline(i);
        CHECK(ptr != NULL, "trampoline should not be NULL");
    }
    /* Out of bounds should return NULL */
    CHECK(wubu_syscall_trampoline(26) == NULL, "out of bounds should return NULL");
    PASS();
}

static void test_syscall_names(void) {
    TEST("wubu_syscall_name returns correct names");
    CHECK(strcmp((const char*)wubu_syscall_name(0), "VBEFillRect") == 0, "syscall 0 name");
    CHECK(strcmp((const char*)wubu_syscall_name(7), "VBESwap") == 0, "syscall 7 name");
    CHECK(strcmp((const char*)wubu_syscall_name(8), "WMCreateWin") == 0, "syscall 8 name");
    CHECK(strcmp((const char*)wubu_syscall_name(23), "WubuExec") == 0, "syscall 23 name");
    CHECK(strcmp((const char*)wubu_syscall_name(24), "GetTime") == 0, "syscall 24 name");
    CHECK(strcmp((const char*)wubu_syscall_name(25), "Sleep") == 0, "syscall 25 name");
    PASS();
}

static void test_utility_syscalls(void) {
    TEST("Utility syscalls execute without crash");
    /* Test GetTime */
    int64_t t1 = sys_get_time(0, 0, 0, 0, 0, 0);
    CHECK(t1 > 0, "GetTime should return positive TSC");
    
    /* Test Sleep (1ms) */
    int64_t t2 = sys_get_time(0, 0, 0, 0, 0, 0);
    sys_sleep(1, 0, 0, 0, 0, 0);
    int64_t t3 = sys_get_time(0, 0, 0, 0, 0, 0);
    CHECK(t3 >= t2, "time should advance after sleep");
    PASS();
}

static void test_file_syscalls(void) {
    TEST("File syscalls execute without crash");
    /* Use a temp file for testing since /dev/null with fwrite is special */
    int64_t fd = sys_file_open((int64_t)"/tmp/wubu_test_file", 2, 0, 0, 0, 0);
    CHECK(fd != -1, "should open temp file for write");
    
    /* Test FileWrite */
    int64_t w = sys_file_write(fd, (int64_t)"test", 4, 0, 0, 0);
    CHECK(w == 4, "write should return 4 bytes");
    
    /* Test FileClose */
    int64_t c = sys_file_close(fd, 0, 0, 0, 0, 0);
    CHECK(c == 0, "close should succeed");
    
    /* Clean up */
    unlink("/tmp/wubu_test_file");
    PASS();
}

/* -- Main -------------------------------------------------------- */

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS Syscall Bridge Test Suite                      |\n");
    printf("|  26 TempleOS/ZealOS-compatible syscalls                |\n");
    printf("+========================================================+\n\n");

    test_syscall_constants();
    test_syscall_trampolines();
    test_syscall_names();
    test_utility_syscalls();
    test_file_syscalls();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}