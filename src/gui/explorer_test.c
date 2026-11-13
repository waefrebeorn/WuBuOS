/*
 * explorer_test.c  --  Test suite for explorer
 */

#include "explorer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/* -- Self-contained ZIP fixture builder ---------------------------- */
/* Builds a minimal but spec-valid uncompressed (STORE) ZIP at `path`
 * containing `nfiles` entries named data0.bin..dataN.bin, each with
 * repeating byte content.  Lets the zip-mount test run without any
 * external `zip` tool or pre-existing fixture file. */
static int make_real_zip(const char *path, int nfiles) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t sig_lfh = 0x04034b50u, sig_cdh = 0x02014b50u, sig_eocd = 0x06054b50u;
    uint32_t cdir_off = 0, cdir_size = 0;
    uint16_t n = (uint16_t)nfiles;
    for (int i = 0; i < nfiles; i++) {
        char name[32];
        snprintf(name, sizeof(name), "data%d.bin", i);
        uint16_t nl = (uint16_t)strlen(name);
        size_t payload = 16 + (size_t)i;          /* distinct sizes */
        /* local file header */
        fwrite(&sig_lfh, 1, 4, f);
        uint16_t ver = 20, flags = 0, method = 0, mt = 0, md = 0;
        uint32_t crc = 0, csize = (uint32_t)payload, usize = (uint32_t)payload;
        uint16_t el = 0, cl = 0;
        fwrite(&ver, 1, 2, f); fwrite(&flags, 1, 2, f); fwrite(&method, 1, 2, f);
        fwrite(&mt, 1, 2, f);  fwrite(&md, 1, 2, f);
        fwrite(&crc, 1, 4, f); fwrite(&csize, 1, 4, f); fwrite(&usize, 1, 4, f);
        fwrite(&nl, 1, 2, f);  fwrite(&el, 1, 2, f);
        fwrite(name, 1, nl, f);
        for (size_t b = 0; b < payload; b++) fputc((int)(b & 0xff), f);
        cdir_off += (uint32_t)(30 + nl + payload);
    }
    for (int i = 0; i < nfiles; i++) {
        char name[32];
        snprintf(name, sizeof(name), "data%d.bin", i);
        uint16_t nl = (uint16_t)strlen(name);
        size_t payload = 16 + (size_t)i;
        fwrite(&sig_cdh, 1, 4, f);
        uint16_t vmb = 20, vnd = 20, flags = 0, method = 0, mt = 0, md = 0;
        uint32_t crc = 0, csize = (uint32_t)payload, usize = (uint32_t)payload;
        uint16_t el = 0, cl = 0, dns = 0, ia = 0; uint32_t ea = 0, lho = 0;
        uint16_t dc = 0;
        fwrite(&vmb, 1, 2, f); fwrite(&vnd, 1, 2, f); fwrite(&flags, 1, 2, f);
        fwrite(&method, 1, 2, f); fwrite(&mt, 1, 2, f); fwrite(&md, 1, 2, f);
        fwrite(&crc, 1, 4, f); fwrite(&csize, 1, 4, f); fwrite(&usize, 1, 4, f);
        fwrite(&nl, 1, 2, f);  fwrite(&el, 1, 2, f);  fwrite(&cl, 1, 2, f);
        fwrite(&dns, 1, 2, f); fwrite(&ia, 1, 2, f);  fwrite(&ea, 1, 4, f);
        fwrite(&lho, 1, 4, f); fwrite(name, 1, nl, f);
        cdir_size += (uint32_t)(46 + nl);
    }
    fwrite(&sig_eocd, 1, 4, f);
    uint16_t dsn = 0, ds = 0, tt = n, tt2 = n;
    uint32_t cds = cdir_size, cdo = cdir_off; uint16_t cl = 0;
    fwrite(&dsn, 1, 2, f); fwrite(&ds, 1, 2, f);
    fwrite(&tt, 1, 2, f);  fwrite(&tt2, 1, 2, f);
    fwrite(&cds, 1, 4, f); fwrite(&cdo, 1, 4, f); fwrite(&cl, 1, 2, f);
    fclose(f);
    return 0;
}

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
    
    int ret = explorer_init();
    TEST_ASSERT(ret == 0, "explorer_init returns 0");
    
    ExExplorerState *ex = explorer_state();
    TEST_ASSERT(ex != NULL, "explorer_state returns valid pointer");
    TEST_ASSERT(ex->entry_count >= 0, "entry_count initialized");
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "default view mode is DETAILS");
    TEST_ASSERT(ex->sort_column == EX_SORT_NAME, "default sort is NAME");
    TEST_ASSERT(ex->sort_ascending == true, "default sort is ascending");
    TEST_ASSERT(ex->toolbar_visible == true, "toolbar visible by default");
    TEST_ASSERT(ex->preview_visible == true, "preview visible by default");

    /* File Manager must open on the GUEST namespace root, never on the host's
     * $HOME.  The old start path was getenv("HOME"), so Explorer opened showing
     * the Linux home directory -- the user complaint that the desktop is a window
     * onto the host OS rather than its own filesystem. */
    TEST_ASSERT(strcmp(ex->current_path, "/") == 0,
                "Explorer starts at the guest root, not $HOME");

    explorer_shutdown();
    TEST_ASSERT(!explorer_is_open(), "explorer not open after shutdown");
}

static void test_navigation(void) {
    printf("\n=== Test: Navigation ===\n");
    
    explorer_init();
    
    const char *initial = explorer_current_path();
    TEST_ASSERT(strlen(initial) > 0, "current path not empty");
    
    /* Test navigate to root */
    explorer_navigate("/");
    TEST_ASSERT(strcmp(explorer_current_path(), "/") == 0, "navigate to root");
    
    /* Test go_up from root stays at root */
    explorer_go_up();
    TEST_ASSERT(strcmp(explorer_current_path(), "/") == 0, "go_up from root stays at root");
    
    explorer_navigate("/tmp");
    const char *tmp_path = explorer_current_path();
    TEST_ASSERT(strcmp(tmp_path, "/tmp") == 0 || strcmp(tmp_path, "/private/tmp") == 0, "navigate to /tmp");
    
    explorer_go_up();
    TEST_ASSERT(strcmp(explorer_current_path(), "/") == 0, "go_up from /tmp goes to /");
    
    explorer_shutdown();
}

static void test_view_modes(void) {
    printf("\n=== Test: View Modes ===\n");
    
    explorer_init();
    
    explorer_set_view_mode(EX_VIEW_ICONS);
    ExExplorerState *ex = explorer_state();
    TEST_ASSERT(ex->view_mode == EX_VIEW_ICONS, "set view mode to ICONS");
    
    explorer_set_view_mode(EX_VIEW_LIST);
    TEST_ASSERT(ex->view_mode == EX_VIEW_LIST, "set view mode to LIST");
    
    explorer_set_view_mode(EX_VIEW_TILES);
    TEST_ASSERT(ex->view_mode == EX_VIEW_TILES, "set view mode to TILES");
    
    explorer_set_view_mode(EX_VIEW_DETAILS);
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "set view mode to DETAILS");
    
    /* Invalid mode should be ignored */
    explorer_set_view_mode(99);
    TEST_ASSERT(ex->view_mode == EX_VIEW_DETAILS, "invalid view mode ignored");
    
    explorer_shutdown();
}

static void test_sort(void) {
    printf("\n=== Test: Sort ===\n");
    
    explorer_init();
    
    explorer_set_sort(EX_SORT_SIZE, true);
    ExExplorerState *ex = explorer_state();
    TEST_ASSERT(ex->sort_column == EX_SORT_SIZE, "sort column SIZE");
    TEST_ASSERT(ex->sort_ascending == true, "sort ascending");
    
    explorer_set_sort(EX_SORT_DATE, false);
    TEST_ASSERT(ex->sort_column == EX_SORT_DATE, "sort column DATE");
    TEST_ASSERT(ex->sort_ascending == false, "sort descending");
    
    explorer_set_sort(EX_SORT_TYPE, true);
    TEST_ASSERT(ex->sort_column == EX_SORT_TYPE, "sort column TYPE");
    
    explorer_set_sort(EX_SORT_NAME, true);
    TEST_ASSERT(ex->sort_column == EX_SORT_NAME, "sort column NAME");
    
    explorer_shutdown();
}

static void test_toggles(void) {
    printf("\n=== Test: Toggles ===\n");
    
    explorer_init();
    
    ExExplorerState *ex = explorer_state();
    
    bool hidden_before = ex->show_hidden;
    explorer_toggle_hidden();
    TEST_ASSERT(ex->show_hidden != hidden_before, "toggle hidden works");
    
    bool ext_before = ex->show_extensions;
    explorer_toggle_extensions();
    TEST_ASSERT(ex->show_extensions != ext_before, "toggle extensions works");
    
    bool preview_before = ex->preview_visible;
    explorer_toggle_preview();
    TEST_ASSERT(ex->preview_visible != preview_before, "toggle preview works");
    
    bool toolbar_before = ex->toolbar_visible;
    explorer_toggle_toolbar();
    TEST_ASSERT(ex->toolbar_visible != toolbar_before, "toggle toolbar works");
    
    explorer_shutdown();
}

static void test_selection(void) {
    printf("\n=== Test: Selection ===\n");
    
    explorer_init();
    
    ExExplorerState *ex = explorer_state();
    
    /* Initially no selection */
    explorer_clear_selection();
    TEST_ASSERT(ex->selection_count == 0, "clear selection -> 0");
    
    /* Test toggle selection on invalid index */
    explorer_toggle_selection(-1);
    TEST_ASSERT(ex->selection_count == 0, "toggle invalid idx doesn't crash");
    explorer_toggle_selection(9999);
    TEST_ASSERT(ex->selection_count == 0, "toggle out of bounds doesn't crash");
    
    explorer_select_all();
    /* After select_all, depends on entry count */
    printf("  ℹ selection_count after select_all: %d\n", ex->selection_count);
    
    explorer_clear_selection();
    TEST_ASSERT(ex->selection_count == 0, "clear selection again -> 0");
    
    explorer_shutdown();
}

static void test_file_ops(void) {
    printf("\n=== Test: File Operations (Clipboard) ===\n");
    
    explorer_init();
    
    ExExplorerState *ex = explorer_state();
    
    /* Copy with no selection */
    ex->selection_count = 0;
    explorer_copy();
    TEST_ASSERT(ex->clipboard_count == 0, "copy with no selection -> empty clipboard");
    
    /* Cut with no selection */
    explorer_cut();
    TEST_ASSERT(ex->clipboard_count == 0, "cut with no selection -> empty clipboard");
    
    /* Paste with empty clipboard */
    explorer_paste();
    TEST_ASSERT(!ex->file_op.in_progress, "paste with empty clipboard -> no op");
    
    /* Delete with no selection */
    explorer_delete(false);
    TEST_ASSERT(!ex->file_op.in_progress, "delete with no selection -> no op");
    
    explorer_shutdown();
}

static void test_helpers(void) {
    printf("\n=== Test: Helper Functions ===\n");
    
    explorer_init();
    
    char buf[64];
    
    /* Test type strings */
    TEST_ASSERT_STR_EQ(explorer_type_str(EX_ENTRY_FILE), "File", "type_str FILE");
    TEST_ASSERT_STR_EQ(explorer_type_str(EX_ENTRY_DIR), "Folder", "type_str DIR");
    TEST_ASSERT_STR_EQ(explorer_type_str(EX_ENTRY_DRIVE), "Drive", "type_str DRIVE");
    TEST_ASSERT_STR_EQ(explorer_type_str(EX_ENTRY_ZIP), "Archive", "type_str ZIP");
    
    /* Test view mode names */
    TEST_ASSERT_STR_EQ(explorer_view_mode_name(EX_VIEW_DETAILS), "Details", "view_mode DETAILS");
    TEST_ASSERT_STR_EQ(explorer_view_mode_name(EX_VIEW_ICONS), "Icons", "view_mode ICONS");
    TEST_ASSERT_STR_EQ(explorer_view_mode_name(EX_VIEW_LIST), "List", "view_mode LIST");
    TEST_ASSERT_STR_EQ(explorer_view_mode_name(EX_VIEW_TILES), "Tiles", "view_mode TILES");
    
    /* Test type colors */
    uint32_t file_color = explorer_type_color(EX_ENTRY_FILE);
    uint32_t dir_color = explorer_type_color(EX_ENTRY_DIR);
    TEST_ASSERT(file_color == 0xFFFFFF, "file color white");
    TEST_ASSERT(dir_color == 0xFFD700, "dir color gold");
    
    /* Test format_size */
    explorer_format_size(512, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "512 B", "format_size 512");
    
    explorer_format_size(1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 KB", "format_size 1KB");
    
    explorer_format_size(1024 * 1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 MB", "format_size 1MB");
    
    explorer_format_size(1024 * 1024 * 1024, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "1.0 GB", "format_size 1GB");
    
    /* Test format_time (just check it doesn't crash) */
    explorer_format_time(time(NULL), buf, sizeof(buf));
    TEST_ASSERT(strlen(buf) > 0, "format_time produces output");
    
    explorer_shutdown();
}

static void test_drive_enumeration(void) {
    printf("\n=== Test: Drive Enumeration ===\n");
    
    explorer_init();
    
    char paths[16][EX_MAX_PATH];
    char labels[16][64];
    int count = explorer_enumerate_drives(paths, labels, 16);
    
    TEST_ASSERT(count > 0, "enumerate_drives returns at least one drive");
    TEST_ASSERT(strcmp(paths[0], "/") == 0, "first drive is root");
    TEST_ASSERT(strlen(labels[0]) > 0, "first drive has label");
    
    printf("  ℹ Found %d drives:\n", count);
    for (int i = 0; i < count; i++) {
        printf("    %d: %s -> %s\n", i, labels[i], paths[i]);
    }
    
    explorer_shutdown();
}

static void test_history(void) {
    printf("\n=== Test: Navigation History ===\n");
    
    explorer_init();
    
    ExExplorerState *ex = explorer_state();
    
    explorer_navigate("/tmp");
    const char *path1 = explorer_current_path();
    
    explorer_navigate("/home");
    const char *path2 = explorer_current_path();
    
    /* Go back */
    explorer_go_back();
    TEST_ASSERT(strcmp(explorer_current_path(), path1) == 0, "go_back works");
    
    /* Go forward */
    explorer_go_forward();
    TEST_ASSERT(strcmp(explorer_current_path(), path2) == 0, "go_forward works");
    
    explorer_shutdown();
}

static void test_zip(void) {
    printf("\n=== Test: Zip Archive (Stub) ===\n");
    
    explorer_init();
    
    ExExplorerState *ex = explorer_state();
    
    TEST_ASSERT(!explorer_is_in_zip(), "initially not in zip");

        /* Build a real, spec-valid ZIP fixture so the test is self-sufficient */
        TEST_ASSERT(make_real_zip("/tmp/test.zip", 5) == 0, "create zip fixture");
        /* Test with real zip file */
        bool ret = explorer_mount_zip("/tmp/test.zip");
        TEST_ASSERT(ret, "mount_zip returns true for real zip");
        TEST_ASSERT(explorer_is_in_zip(), "in_zip_archive is true after mount");
        TEST_ASSERT(ex->entry_count > 0, "entries populated from zip");

        explorer_unmount_zip();
        TEST_ASSERT(!ex->in_zip_archive, "unmount_zip clears flag");
    
        /* Test with non-existent file */
        ret = explorer_mount_zip("/fake/archive.zip");
        TEST_ASSERT(!ret, "mount_zip returns false for missing file");
    
    explorer_shutdown();
}

static void test_state_accessors(void) {
    printf("\n=== Test: State Accessors ===\n");
    
    explorer_init();
    
    ExExplorerState *s1 = explorer_state();
    ExExplorerState *s2 = explorer_state();
    TEST_ASSERT(s1 == s2, "explorer_state returns same pointer");
    
    const char *path = explorer_current_path();
    TEST_ASSERT(path != NULL, "explorer_current_path returns non-NULL");
    TEST_ASSERT(strlen(path) > 0, "current path not empty");
    
    explorer_shutdown();
}

/* -- New Feature Tests ------------------------------------------- */

static void test_find(void) {
    printf("\n=== Test: Find (Ctrl+F) ===\n");

    explorer_init();

    /* Shift+Ctrl+F activates find mode - simulate by calling handle_key with Ctrl+F */
    explorer_handle_key('f', 0x04); /* Ctrl+F */
    /* After activation, the find state should be accessible via status text */
    ExExplorerState *ex = explorer_state();
    TEST_ASSERT(strlen(ex->status_text) > 0, "Ctrl+F sets status text");

    /* Test escape cancels find */
    explorer_handle_key(0x01, 0); /* Escape */
    TEST_ASSERT(ex->status_text[0] != '\0', "Escape doesn't crash (status set)");

    /* Test typing in find mode */
    explorer_handle_key('f', 0x04); /* Ctrl+F - reactivate */
    explorer_handle_key('t', 0);    /* Type 't' */
    TEST_ASSERT(ex->status_text[0] != '\0', "Typing in find mode sets status text");

    explorer_handle_key(0x01, 0);   /* Cancel */
    explorer_shutdown();
}

static void test_shift_select(void) {
    printf("\n=== Test: Shift-Select (Mouse) ===\n");

    explorer_init();

    /* Simulate shift key being pressed in key handler */
    explorer_handle_key(0xE048, 0x01); /* Up arrow + Shift modifier */
    /* The key handler sets g_shift_pressed = true when mods & 0x01 */
    /* The mouse handler then uses this state for shift-click selection */
    TEST_ASSERT(1, "shift key tracking compiles and runs without crash");

    /* Test shift-click via mouse handler */
    explorer_handle_mouse(300, 150, 1, 1);
    TEST_ASSERT(1, "mouse handler with shift pressed doesn't crash");

    explorer_shutdown();
}

static void test_image_preview(void) {
    printf("\n=== Test: Image Preview ===\n");

    explorer_init();
    ExExplorerState *ex = explorer_state();

    /* Test no-data preview (no pixels loaded) */
    ex->preview.type = EX_PREVIEW_IMAGE;
    ex->preview.img_w = 100;
    ex->preview.img_h = 100;
    ex->preview.img_pixels = NULL;
    /* Render should not crash */
    explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with NULL pixels doesn't crash");

    /* Test with pixel data */
    uint32_t pixels[100 * 100];
    memset(pixels, 0xFF00FF00, sizeof(pixels)); /* Green */
    ex->preview.img_pixels = pixels;
    ex->preview.img_w = 100;
    ex->preview.img_h = 100;
    explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with pixel data doesn't crash");

    /* Test with large image (needs downscaling) */
    uint32_t big_pixels[400 * 300];
    memset(big_pixels, 0x00FF0000, sizeof(big_pixels)); /* Blue */
    ex->preview.img_pixels = big_pixels;
    ex->preview.img_w = 400;
    ex->preview.img_h = 300;
    explorer_render(NULL, 1024, 768);
    TEST_ASSERT(1, "image preview with large image doesn't crash");

    /* Cleanup */
    ex->preview.img_pixels = NULL;
    ex->preview.type = EX_PREVIEW_NONE;
    explorer_shutdown();
}

/* -- Main --------------------------------------------------------- */

/* The unification property: whatever the guest STORE holds is what File Manager
 * lists -- same namespace Notepad writes into, folders included, and only direct
 * children at the requested depth. */
static void test_guest_store_visible(void) {
    printf("\n=== Test: guest store is what Explorer lists ===\n");
    extern int  fs_add_file(const char *, const uint8_t *, uint32_t);
    extern int  fs_add_dir(const char *);
    extern void fs_reset(void);

    fs_reset();
    fs_add_file("notes.txt", (const uint8_t *)"hello", 5);
    fs_add_dir("Documents");
    fs_add_file("Documents/report.txt", (const uint8_t *)"q", 1);

    explorer_init();
    ExExplorerState *ex = explorer_state();

    bool saw_notes = false, saw_documents = false;
    for (int i = 0; i < ex->entry_count; i++) {
        if (strcmp(ex->entries[i].name, "notes.txt") == 0) saw_notes = true;
        if (strcmp(ex->entries[i].name, "Documents") == 0) {
            saw_documents = true;
            TEST_ASSERT(ex->entries[i].type == EX_ENTRY_DIR,
                        "a folder appears as a folder, not a file");
        }
        /* Nested entries must NOT surface in the parent listing. */
        TEST_ASSERT(strcmp(ex->entries[i].name, "report.txt") != 0,
                    "nested entry stays out of the parent listing");
    }

    TEST_ASSERT(saw_notes, "a file saved through the guest store is visible");
    TEST_ASSERT(saw_documents, "a folder created in the guest store is visible");

    /* Descending works too -- this is what "File Manager" means. */
    ex_scan_directory("/Documents");
    bool saw_nested = false;
    for (int i = 0; i < ex->entry_count; i++)
        if (strcmp(ex->entries[i].name, "report.txt") == 0) saw_nested = true;
    TEST_ASSERT(saw_nested, "descending into the folder reveals its contents");

    fs_reset();
    explorer_shutdown();
}

/* File operations must act on the GUEST store.  Before this, Delete on a document
 * saved by Notepad either failed silently or removed an unrelated host file,
 * because the workers went straight to ex_9p_*() (host paths). */
static void test_guest_file_ops(void) {
    printf("\n=== Test: copy/move/delete act on guest files ===\n");
    extern int   fs_add_file(const char *, const uint8_t *, uint32_t);
    extern int   fs_add_dir(const char *);
    extern void  fs_reset(void);

    fs_reset();
    fs_add_file("a.txt", (const uint8_t *)"payload!", 8);
    fs_add_dir("Folder");
    fs_add_file("Folder/inner.txt", (const uint8_t *)"inside", 6);
    explorer_init();

    ExExplorerState *ex = explorer_state();

    /* -- COPY a file into another folder -- */
    memset(&ex->file_op, 0, sizeof(ex->file_op));
    ex->file_op.type = EX_OP_COPY;
    strncpy(ex->file_op.dest_path, "/Folder", sizeof(ex->file_op.dest_path) - 1);
    snprintf(ex->file_op.paths[0], sizeof(ex->file_op.paths[0]), "%s", "a.txt");
    ex->file_op.count = 1;
    ex_handle_file_op(ex);

    TEST_ASSERT(guest_file_exists("Folder/a.txt"),
                "copy creates the destination in the guest store");
    TEST_ASSERT(!guest_file_exists("a.txt") == false,
                "copy leaves the original in place");

    /* -- MOVE it back out -- */
    memset(&ex->file_op, 0, sizeof(ex->file_op));
    ex->file_op.type = EX_OP_MOVE;
    strncpy(ex->file_op.dest_path, "/", sizeof(ex->file_op.dest_path) - 1);
    snprintf(ex->file_op.paths[0], sizeof(ex->file_op.paths[0]),
             "%s", "Folder/a.txt");
    ex->file_op.count = 1;
    ex_handle_file_op(ex);

    TEST_ASSERT(guest_file_exists("a.txt"), "move relocates the entry");
    TEST_ASSERT(!guest_file_exists("Folder/a.txt"),
                "move removes the source (not a silent duplicate)");

    /* -- DELETE a folder recursively -- */
    memset(&ex->file_op, 0, sizeof(ex->file_op));
    ex->file_op.type = EX_OP_DELETE;
    snprintf(ex->file_op.paths[0], sizeof(ex->file_op.paths[0]), "%s", "Folder");
    ex->file_op.count = 1;
    ex_handle_file_op(ex);

    TEST_ASSERT(!guest_file_is_dir("Folder"),
                "delete removes the folder itself");
    TEST_ASSERT(!guest_file_exists("Folder/inner.txt"),
                "delete removes everything INSIDE it too");

    fs_reset();
    explorer_shutdown();
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  explorer Test Suite                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    test_init_shutdown();
    test_guest_store_visible();
    test_guest_file_ops();
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
