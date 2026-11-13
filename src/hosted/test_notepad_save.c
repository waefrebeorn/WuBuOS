/*
 * test_notepad_save.c -- does a document actually survive a restart?
 *
 * This is the acceptance test for laundry-list items 1 and 2.  Item 1 made the
 * namespace durable; item 2 gave Notepad a way to reach it.  Neither means
 * anything until a document can be typed, saved, the process state thrown away,
 * and read back byte for byte.
 *
 * The GUI layer is stubbed rather than linked, because this test is about the
 * bytes on their way to disk, not about pixels.  Everything below the stub line
 * is the real Notepad and the real guest filesystem.
 */

#include "hosted.h"
#include "hosted_internal.h"
#include "hosted_fs_persist.h"
#include "../apps/guest_file.h"
#include "../apps/notepad.h"
#include "../gui/shell/window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* -- GUI stubs ----------------------------------------------------
 * DosGuiWindow is deliberately opaque, so the stub cannot fabricate one.  It
 * returns NULL instead; this test never draws, and notepad_open tolerates a
 * NULL window (it only skips wiring the callbacks).
 */

DosGuiWindow *wm_create(int x, int y, int w, int h, const char *title)
{
    (void)x; (void)y; (void)w; (void)h; (void)title;
    return NULL;
}

ChromeContentRect chrome_draw_window(DosGuiWindow *win, uint32_t *fb,
                                     int fw, int fh)
{
    (void)win; (void)fb; (void)fw; (void)fh;
    ChromeContentRect r = { 0, 0, 800, 600 };
    return r;
}

void vbe_fill_rect(int x, int y, int w, int h, uint32_t c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
void vbe_rect(int x, int y, int w, int h, uint32_t c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
void vbe_draw_text(int x, int y, const char *s, uint32_t c, int scale)
{ (void)x; (void)y; (void)s; (void)c; (void)scale; }
void vbe_set_pixel(int x, int y, uint32_t c)
{ (void)x; (void)y; (void)c; }
VBEState *vbe_state(void) { return NULL; }

WubuThemeColors *tc(void)
{
    static WubuThemeColors t;
    memset(&t, 0, sizeof(t));
    return &t;
}

/* -- test harness ------------------------------------------------- */

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

static const char *kStore = "/tmp/wubu_notepad_roundtrip.img";

/* Type a string through the real key handler. */
static void type(const char *s)
{
    for (const char *p = s; *p; p++)
        notepad_key(NULL, (uint32_t)(unsigned char)*p, 0);
}

int main(void)
{
    setenv("WUBU_FS_STORE", kStore, 1);
    unlink(kStore);

    /* ---- 1. type, save, verify on disk ---------------------------- */
    notepad_reset();
    type("hello\nworld");
    TEST(notepad_modified(), "typing marks the document modified");

    TEST(notepad_save("/n/notes/todo.txt"), "save succeeds");
    TEST(!notepad_modified(), "saving clears the modified flag");
    TEST(strcmp(notepad_filename(), "/n/notes/todo.txt") == 0,
          "filename recorded");
    TEST(guest_file_exists("/n/notes/todo.txt"), "file exists in the namespace");
    /* Durability is a separate step: the write marks the namespace dirty and
     * the host flushes it.  Assert both halves explicitly. */
    TEST(hosted_fs_flush() == 0, "flush succeeds");
    TEST(fs_persist_size() > 0, "image written");

    /* ---- 2. forget everything, read it back ---------------------- */
    notepad_reset();
    TEST(notepad_line_count() == 1, "reset empties the buffer");

    TEST(notepad_load("/n/notes/todo.txt"), "load succeeds");
    TEST(notepad_line_count() == 2, "two lines restored");
    TEST(strcmp(notepad_line_text(0), "hello") == 0, "line one exact");
    TEST(strcmp(notepad_line_text(1), "world") == 0, "line two exact");

    /* ---- 3. a trailing newline must not grow a phantom line ------ */
    notepad_reset();
    type("one");
    notepad_save("/n/one.txt");
    notepad_reset();
    notepad_load("/n/one.txt");
    TEST(notepad_line_count() == 1, "no phantom trailing line");

    /* ---- 4. overwrite in place keeps working --------------------- */
    notepad_reset();
    type("second version");
    TEST(notepad_save("/n/notes/todo.txt"), "overwrite succeeds");
    notepad_reset();
    notepad_load("/n/notes/todo.txt");
    TEST(notepad_line_count() == 1, "overwritten file has one line");
    TEST(strcmp(notepad_line_text(0), "second version") == 0, "new content won");

    /* ---- 5. Ctrl+S / Ctrl+O drive the same path ------------------ */
    /* With no filename yet, Ctrl+S asks where to put the document.  That is
     * the Windows behaviour too: the first save names the file. */
    notepad_reset();
    type("via hotkey");
    notepad_key(NULL, 's', 0x04);
    type("untitled.txt\r");                       /* answer the prompt */
    TEST(strcmp(notepad_filename(), "untitled.txt") == 0,
          "Ctrl+S names the file on first save");
    TEST(guest_file_exists("untitled.txt"), "named file landed");
    TEST(hosted_fs_flush() == 0, "flush after hotkey save");

    notepad_reset();
    type("scratch");
    notepad_key(NULL, 'o', 0x04);                 /* Ctrl+O: prompt mode */
    type("untitled.txt\r");                       /* confirm the prompt */
    TEST(strcmp(notepad_line_text(0), "via hotkey") == 0,
          "Ctrl+O prompt loads the saved file");

    /* ---- 6. refusal, never silent truncation --------------------- */
    notepad_reset();
    TEST(!notepad_save(""), "empty name refused");
    TEST(!notepad_load("/n/does-not-exist.txt"), "missing file refused");
    TEST(notepad_line_count() == 1, "refused load left the buffer alone");

    /* ---- 7. durability across a "restart" ------------------------ */
    notepad_reset();
    type("persist me");
    notepad_save("/n/persist.txt");
    TEST(hosted_fs_flush() == 0, "flush succeeds");
    fs_reset();                                    /* the process went away */
    TEST(fs_persist_load(1) > 0, "namespace restored");
    notepad_reset();
    TEST(notepad_load("/n/persist.txt"), "reload after restart");
    TEST(strcmp(notepad_line_text(0), "persist me") == 0,
          "CONTENT SURVIVED THE RESTART");

    printf("=== notepad_save: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
