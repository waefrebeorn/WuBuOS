/*
 * dosgui_explorer_test.c  --  Test suite for dosgui_explorer
 */

#include "dosgui_explorer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* -- Test Helpers ------------------------------------------------- */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("  ✅ %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("  ❌ %s\n", msg); \
            tests_failed++; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) == 0) { \
            printf("  ✅ %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("  ❌ %s: expected '%s', got '%s'\n", msg, (b), (a)); \
            tests_failed++; \
        } \
    } while (0)

/* -- External symbols for stub linking ----------------------------- */
/* VBE functions are provided by vbe.c (linked in test) */
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale);
int vbe_text_width(const char *s, int scale);
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color);
void vbe_3d_raised_colors(int x, int y, int w, int h,
                          uint32_t light, uint32_t face,
                          uint32_t dark, uint32_t darkest);
void vbe_3d_sunken_colors(int x, int y, int w, int h,
                          uint32_t light, uint32_t face,
                          uint32_t dark, uint32_t darkest);
void vbe_hline(int x1, int x2, int y, uint32_t color);
void vbe_vline(int x, int y1, int y2, uint32_t color);
void vbe_rect(int x, int y, int w, int h, uint32_t color);
void vbe_shade_rect(int x, int y, int w, int h);
void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale);
int vbe_fill_rect_clip(int x, int y, int w, int h, uint32_t color);

/* -- Tests -------------------------------------------------------- */

static void test_init_shutdown(void) {
    printf("\n=== Test: Init/Shutdown ===\n");
    
    int ret = dosgui_explorer_init();
    TEST_ASSERT(ret == 0, "dosgui_explorer_init returns 0");
    
    ExExplorerState *ex = dosgui_explorer_state();
    TEST_ASSERT(ex != NULL, "dosgui_explorer_state returns valid pointer");
    TEST_ASSERT(ex->entry_count >= 0, "entry_count initialized");
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "default view mode is DETAILS");
    TEST_ASSERT(ex->sort_column == EX_SORT_NAME, "default sort is NAME");
    TEST_ASSERT(ex->sort_ascending == true, "default sort is ascending");
    TEST_ASSERT(ex->toolbar_visible == true, "toolbar visible by default");
    TEST_ASSERT(ex->preview_visible == true, "preview visible by default");
    
    dosgui_explorer_shutdown();
    TEST_ASSERT(!dosgui_explorer_is_open(), "explorer not open after shutdown");
}

static void test_navigation(void) {
    printf("\n=== Test: Navigation ===\n");
    
    dosgui_explorer_init();
    
    const char *initial = dosgui_explorer_current_path();
    TEST_ASSERT(strlen(initial) > 0, "current path not empty");
    
    /* Test navigate to root */
    dosgui_explorer_navigate("/");
    TEST_ASSERT(strcmp(dosgui_explorer_current_path(), "/") == 0, "navigate to root");
    
    /* Test go_up from root stays at root */
    dosgui_explorer_go_up();
    TEST_ASSERT(strcmp(dosgui_explorer_current_path(), "/") == 0, "go_up from root stays at root");
    
    dosgui_explorer_navigate("/tmp");
    const char *tmp_path = dosgui_explorer_current_path();
    TEST_ASSERT(strcmp(tmp_path, "/tmp") == 0 || strcmp(tmp_path, "/private/tmp") == 0, "navigate to /tmp");
    
    dosgui_explorer_go_up();
    TEST_ASSERT(strcmp(dosgui_explorer_current_path(), "/") == 0, "go_up from /tmp goes to /");
    
    dosgui_explorer_shutdown();
}

static void test_view_modes(void) {
    printf("\n=== Test: View Modes ===\n");
    
    dosgui_explorer_init();
    
    dosgui_explorer_set_view_mode(EX_VIEW_ICONS);
    ExExplorerState *ex = dosgui_explorer_state();
    TEST_ASSERT(ex->view_mode == EX_VIEW_ICONS, "set view mode to ICONS");
    
    dosgui_explorer_set_view_mode(EX_VIEW_LIST);
    TEST_ASSERT(ex->view_mode == EX_VIEW_LIST, "set view mode to LIST");
    
    dosgui_explorer_set_view_mode(EX_VIEW_TILES);
    TEST_ASSERT(ex->view_mode == EX_VIEW_TILES, "set view mode to TILES");
    
    dosgui_explorer_set_view_mode(EX_VIEW_DETAILS);
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "set view mode to DETAILS");
    
    /* Invalid mode should be ignored */
    dosgui_explorer_set_view_mode(99);
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "invalid view mode ignored");
    
    dosgui_explorer_shutdown();
}

static void test_sort(void) {
    printf("\n=== Test: Sort ===\n");
    
    dosgui_explorer_init();
    
    dosgui_explorer_set_sort(EX_SORT_SIZE, true);
    ExExplorerState *ex = dosgui_explorer_state();
    TEST_ASSERT(ex->sort_column == EX_SORT_SIZE, "sort column SIZE");
    TEST_ASSERT(ex->sort_ascending == true, "sort ascending");
    
    dosgui_explorer_set_sort(EX_SORT_DATE, false);
    TEST_ASSERT(ex->sort_column == EX_SORT_DATE, "sort column DATE");
    TEST_ASSERT(ex->sort_ascending == false, "sort descending");
    
    dosgui_explorer_set_sort(EX_SORT_TYPE, true);
    TEST_ASSERT(ex->sort_column == EX_SORT_TYPE, "sort column TYPE");
    
    dosgui_explorer_set_sort(EX_SORT_NAME, true);
    TEST_ASSERT(ex->sort_column == EX_SORT_NAME, "sort column NAME");
    
    dosgui_explorer_shutdown();
}

static void test_toggles(void) {
    printf("\n=== Test: Toggles ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *ex = dosgui_explorer_state();
    
    bool hidden_before = ex->show_hidden;
    dosgui_explorer_toggle_hidden();
    TEST_ASSERT(ex->show_hidden != hidden_before, "toggle hidden works");
    
    bool ext_before = ex->show_extensions;
    dosgui_explorer_toggle_extensions();
    TEST_ASSERT(ex->show_extensions != ext_before, "toggle extensions works");
    
    bool preview_before = ex->preview_visible;
    dosgui_explorer_toggle_preview();
    TEST_ASSERT(ex->preview_visible != preview_before, "toggle preview works");
    
    bool toolbar_before = ex->toolbar_visible;
    dosgui_explorer_toggle_toolbar();
    TEST_ASSERT(ex->toolbar_visible != toolbar_before, "toggle toolbar works");
    
    dosgui_explorer_shutdown();
}

static void test_selection(void) {
    printf("\n=== Test: Selection ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *ex = dosgui_explorer_state();
    
    /* Initially no selection */
    dosgui_explorer_clear_selection();
    TEST_ASSERT(ex->selection_count == 0, "clear selection -> 0");
    
    /* Test toggle selection on invalid index */
    dosgui_explorer_toggle_selection(-1);
    TEST_ASSERT(ex->selection_count == 0, "toggle invalid idx doesn't crash");
    dosgui_explorer_toggle_selection(9999);
    TEST_ASSERT(ex->selection_count == 0, "toggle out of bounds doesn't crash");
    
    dosgui_explorer_select_all();
    /* After select_all, depends on entry count */
    printf("  ℹ selection_count after select_all: %d\n", ex->selection_count);
    
    dosgui_explorer_clear_selection();
    TEST_ASSERT(ex->selection_count == 0, "clear selection again -> 0");
    
    dosgui_explorer_shutdown();
}

static void test_file_ops(void) {
    printf("\n=== Test: File Operations (Clipboard) ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *ex = dosgui_explorer_state();
    
    /* Copy with no selection */
    ex->selection_count = 0;
    dosgui_explorer_copy();
    TEST_ASSERT(ex->clipboard_count == 0, "copy with no selection -> empty clipboard");
    
    /* Cut with no selection */
    dosgui_explorer_cut();
    TEST_ASSERT(ex->clipboard_count == 0, "cut with no selection -> empty clipboard");
    
    /* Paste with empty clipboard */
    dosgui_explorer_paste();
    TEST_ASSERT(!ex->file_op.in_progress, "paste with empty clipboard -> no op");
    
    /* Delete with no selection */
    dosgui_explorer_delete(false);
    TEST_ASSERT(!ex->file_op.in_progress, "delete with no selection -> no op");
    
    dosgui_explorer_shutdown();
}

static void test_helpers(void) {
    printf("\n=== Test: Helper Functions ===\n");
    
    dosgui_explorer_init();
    
    char buf[64];
    
    /* Test type strings */
    TEST_ASSERT_STR_EQ(dosgui_explorer_type_str(EX_ENTRY_FILE), "File", "type_str FILE");
    TEST_ASSERT_STR_EQ(dosgui_explorer_type_str(EX_ENTRY_DIR), "Folder", "type_str DIR");
    TEST_ASSERT_STR_EQ(dosgui_explorer_type_str(EX_ENTRY_DRIVE), "Drive", "type_str DRIVE");
    TEST_ASSERT_STR_EQ(dosgui_explorer_type_str(EX_ENTRY_ZIP), "Archive", "type_str ZIP");
    
    /* Test view mode names */
    TEST_ASSERT_STR_EQ(dosgui_explorer_view_mode_name(EX_VIEW_DETAILS), "Details", "view_mode DETAILS");
    TEST_ASSERT_STR_EQ(dosgui_explorer_view_mode_name(EX_VIEW_ICONS), "Icons", "view_mode ICONS");
    TEST_ASSERT_STR_EQ(dosgui_explorer_view_mode_name(EX_VIEW_LIST), "List", "view_mode LIST");
    TEST_ASSERT_STR_EQ(dosgui_explorer_view_mode_name(EX_VIEW_TILES), "Tiles", "view_mode TILES");
    
    /* Test type colors */
    uint32_t file_color = dosgui_explorer_type_color(EX_ENTRY_FILE);
    uint32_t dir_color = dosgui_explorer_type_color(EX_ENTRY_DIR);
    TEST_ASSERT(file_color == 0xFFFFFF, "file color white");
    TEST_ASSERT(dir_color == 0xFFD700, "dir color gold");
    
    /* Test format_size */
    dosgui_explorer_format_size(512, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "512 B", "format_size 512");
    
    dosgui_explorer_format_size(1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 KB", "format_size 1KB");
    
    dosgui_explorer_format_size(1024 * 1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 MB", "format_size 1MB");
    
    dosgui_explorer_format_size(1024 * 1024 * 1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 GB", "format_size 1GB");
    
    /* Test format_time (just check it doesn't crash) */
    dosgui_explorer_format_time(time(NULL), buf, sizeof(buf));
    TEST_ASSERT(strlen(buf) > 0, "format_time produces output");
    
    dosgui_explorer_shutdown();
}

static void test_drive_enumeration(void) {
    printf("\n=== Test: Drive Enumeration ===\n");
    
    dosgui_explorer_init();
    
    char paths[16][EX_MAX_PATH];
    char labels[16][64];
    int count = dosgui_explorer_enumerate_drives(paths, labels, 16);
    
    TEST_ASSERT(count > 0, "enumerate_drives returns at least one drive");
    TEST_ASSERT(strcmp(paths[0], "/") == 0, "first drive is root");
    TEST_ASSERT(strlen(labels[0]) > 0, "first drive has label");
    
    printf("  ℹ Found %d drives:\n", count);
    for (int i = 0; i < count; i++) {
        printf("    %d: %s -> %s\n", i, labels[i], paths[i]);
    }
    
    dosgui_explorer_shutdown();
}

static void test_history(void) {
    printf("\n=== Test: Navigation History ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *ex = dosgui_explorer_state();
    
    dosgui_explorer_navigate("/tmp");
    const char *path1 = dosgui_explorer_current_path();
    
    dosgui_explorer_navigate("/home");
    const char *path2 = dosgui_explorer_current_path();
    
    /* Go back */
    dosgui_explorer_go_back();
    TEST_ASSERT(strcmp(dosgui_explorer_current_path(), path1) == 0, "go_back works");
    
    /* Go forward */
    dosgui_explorer_go_forward();
    TEST_ASSERT(strcmp(dosgui_explorer_current_path(), path2) == 0, "go_forward works");
    
    dosgui_explorer_shutdown();
}

static void test_zip(void) {
    printf("\n=== Test: Zip Archive (Stub) ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *ex = dosgui_explorer_state();
    
    TEST_ASSERT(!dosgui_explorer_is_in_zip(), "initially not in zip");

        /* Test with real zip file */
        bool ret = dosgui_explorer_mount_zip("/tmp/test.zip");
        TEST_ASSERT(ret, "mount_zip returns true for real zip");
        TEST_ASSERT(dosgui_explorer_is_in_zip(), "in_zip_archive is true after mount");
        TEST_ASSERT(ex->entry_count > 0, "entries populated from zip");

        dosgui_explorer_unmount_zip();
        TEST_ASSERT(!ex->in_zip_archive, "unmount_zip clears flag");
    
        /* Test with non-existent file */
        ret = dosgui_explorer_mount_zip("/fake/archive.zip");
        TEST_ASSERT(!ret, "mount_zip returns false for missing file");
    
    dosgui_explorer_shutdown();
}

static void test_state_accessors(void) {
    printf("\n=== Test: State Accessors ===\n");
    
    dosgui_explorer_init();
    
    ExExplorerState *s1 = dosgui_explorer_state();
    ExExplorerState *s2 = dosgui_explorer_state();
    TEST_ASSERT(s1 == s2, "dosgui_explorer_state returns same pointer");
    
    const char *path = dosgui_explorer_current_path();
    TEST_ASSERT(path != NULL, "dosgui_explorer_current_path returns non-NULL");
    TEST_ASSERT(strlen(path) > 0, "current path not empty");
    
    dosgui_explorer_shutdown();
}

/* -- New Feature Tests ------------------------------------------- */

static void test_find(void) {
    printf("\n=== Test: Find (Ctrl+F) ===\n");

    dosgui_explorer_init();

    /* Shift+Ctrl+F activates find mode - simulate by calling handle_key with Ctrl+F */
    dosgui_explorer_handle_key('f', 0x04); /* Ctrl+F */
    /* After activation, the find state should be accessible via status text */
    ExExplorerState *ex = dosgui_explorer_state();
    TEST_ASSERT(strlen(ex->status_text) > 0, "Ctrl+F sets status text");

    /* Test escape cancels find */
    dosgui_explorer_handle_key(0x01, 0); /* Escape */
    TEST_ASSERT(ex->status_text[0] != '\0', "Escape doesn't crash (status set)");

    /* Test typing in find mode */
    dosgui_explorer_handle_key('f', 0x04); /* Ctrl+F - reactivate */
    dosgui_explorer_handle_key('t', 0);    /* Type 't' */
    TEST_ASSERT(ex->status_text[0] != '\0', "Typing in find mode sets status text");

    dosgui_explorer_handle_key(0x01, 0);   /* Cancel */
    dosgui_explorer_shutdown();
}

static void test_shift_select(void) {
    printf("\n=== Test: Shift-Select (Mouse) ===\n");

    dosgui_explorer_init();

    /* Simulate shift key being pressed in key handler */
    dosgui_explorer_handle_key(0xE048, 0x01); /* Up arrow + Shift modifier */
    /* The key handler sets g_shift_pressed = true when mods & 0x01 */
    /* The mouse handler then uses this state for shift-click selection */
    TEST_ASSERT(1, "shift key tracking compiles and runs without crash");

    /* Test shift-click via mouse handler */
    dosgui_explorer_handle_mouse(300, 150, 1, 1);
    TEST_ASSERT(1, "mouse handler with shift pressed doesn't crash");

    dosgui_explorer_shutdown();
}

static void test_image_preview(void) {
    printf("\n=== Test: Image Preview ===\n");

    dosgui_explorer_init();
    ExExplorerState *ex = dosgui_explorer_state();

    /* Test no-data preview (no pixels loaded) */
    ex->preview.type = EX_PREVIEW_IMAGE;
    ex->preview.img_w = 100;
    ex->preview.img_h = 100;
    ex->preview.img_pixels = NULL;
    /* Render should not crash */
    dosgui_explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with NULL pixels doesn't crash");

    /* Test with pixel data */
    uint32_t pixels[100 * 100];
    memset(pixels, 0xFF00FF00, sizeof(pixels)); /* Green */
    ex->preview.img_pixels = pixels;
    ex->preview.img_w = 100;
    ex->preview.img_h = 100;
    dosgui_explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with pixel data doesn't crash");

    /* Test with large image (needs downscaling) */
    uint32_t big_pixels[400 * 300];
    memset(big_pixels, 0x00FF0000, sizeof(big_pixels)); /* Blue */
    ex->preview.img_pixels = big_pixels;
    ex->preview.img_w = 400;
    ex->preview.img_h = 300;
    dosgui_explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with large image doesn't crash");

    /* Cleanup */
    ex->preview.img_pixels = NULL;
    ex->preview.type = EX_PREVIEW_NONE;
    dosgui_explorer_shutdown();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  dosgui_explorer Test Suite                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    test_init_shutdown();
    test_navigation();
    test_view_modes();
    test_sort();
    test_toggles();
    test_selection();
    test_file_ops();
    test_helpers();
    test_drive_enumeration();
    test_history();
    test_zip();
    test_state_accessors();
    test_find();
    test_shift_select();
    test_image_preview();

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Results: %d passed, %d failed                          ║\n", tests_passed, tests_failed);
    printf("╚══════════════════════════════════════════════════════════╝\n");

    return tests_failed > 0 ? 1 : 0;
}
