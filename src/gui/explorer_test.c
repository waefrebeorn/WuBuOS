/*
 * explorer_test.c  --  Test suite for explorer
 */

#include "explorer.h"
#include "../kernel/vbe.h"      /* pixel gate for the tab strip */
#include "wubu_theme.h"         /* colours come from the theme, not from the test */

#include "explorer_internal.h"   /* shared context-menu geometry */
#include "../runtime/wubu_libraries.h"
#include <sys/stat.h>
#include <unistd.h>
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

/* Scrolling: the list had a `list_scroll_y` field that NOTHING read, so a folder with more rows than the pane
 * could not be scrolled at all. Windows' rule is the one implemented: the wheel moves three rows, and the
 * KEYBOARD scrolls the list to wherever the focus went -- not on paint. */
static void test_list_scrolling(void) {
    printf("\n=== Test: List scrolling ===\n");
    explorer_init();
    ExExplorerState *ex = explorer_state();
    ex->list_view_h = 160;                       /* eight-ish rows visible */
    ex->list_scroll_y = 0;
    const int row = ex_row_h();

    /* this session's folder is empty, so build the list the scroll code actually reads */
    if (ex_view_count(ex) == 0) {
        for (int k = 0; k < 40 && k < EX_MAX_ENTRIES; k++) {
            snprintf(ex->entries[k].name, sizeof ex->entries[k].name, "file_%02d.txt", k);
            ex->entries[k].size = 1;
            ex->visible[k] = k;
        }
        ex->entry_count = 40;
        ex->visible_count = 40;
    }
    explorer_handle_mouse(10, 100, 5, 1);        /* wheel down */
    TEST_ASSERT(ex->list_scroll_y == 3 * row, "one wheel notch moves three rows (Windows default)");
    explorer_handle_mouse(10, 100, 4, 1);        /* wheel up */
    TEST_ASSERT(ex->list_scroll_y == 0, "wheel up returns to the top");
    explorer_handle_mouse(10, 100, 4, 1);        /* and cannot go past it */
    TEST_ASSERT(ex->list_scroll_y == 0, "the wheel cannot scroll above the first row");

    /* keyboard: move the focus to the last entry and the list must follow it */
    if (ex_view_count(ex) > 0) {
        int last = ex_view_count(ex) - 1;
        explorer_clear_selection();
        ex->focus_idx = ex_view_entry(ex, last);
        ex->list_scroll_y = 0;
        explorer_handle_key(0xE050, 0);          /* Down: re-asserts the focus, then ensures visible */
        int need_bottom = last * row + row;
        TEST_ASSERT(ex->list_scroll_y + ex->list_view_h >= need_bottom &&
                    ex->list_scroll_y <= last * row,
                    "the keyboard scrolls the focused row into view (and does not overshoot it)");
    } else {
        printf("  ℹ no entries to focus; skipping the keyboard check\n");
    }
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

static void test_search(void) {
    printf("\n=== Test: Search field (the Win11 toolbar box) ===\n");

    explorer_init();
    ExExplorerState *ex = explorer_state();

    /* A known list, not whatever the machine's root happens to hold: the search model is what is
     * under test, and a gate that depends on the host's files is a gate that fails on another box. */
    memset(ex->entries, 0, sizeof ex->entries);
    ex->entry_count = 3;
    snprintf(ex->entries[0].name, sizeof ex->entries[0].name, "report.png");
    snprintf(ex->entries[0].type_str, sizeof ex->entries[0].type_str, "PNG Image");
    snprintf(ex->entries[0].extension, sizeof ex->entries[0].extension, "png");
    snprintf(ex->entries[1].name, sizeof ex->entries[1].name, "notes.txt");
    snprintf(ex->entries[1].type_str, sizeof ex->entries[1].type_str, "Text Document");
    snprintf(ex->entries[1].extension, sizeof ex->entries[1].extension, "txt");
    snprintf(ex->entries[2].name, sizeof ex->entries[2].name, "Documents");
    snprintf(ex->entries[2].type_str, sizeof ex->entries[2].type_str, "Folder");
    ex->focus_idx = -1;
    ex_view_invalidate(ex);

    /* 1. No query: the view is the whole list. */
    ex_search_set(ex, "");
    ex_view_sync(ex);
    int total = ex_view_count(ex);
    TEST_ASSERT(total == ex->entry_count, "no query: every entry is visible");
    TEST_ASSERT(total == 3, "the test list is the three entries it was given");

    /* 2. Name, extension and type all match -- the three things a user types. */
    ex_search_set(ex, "repo");
    TEST_ASSERT(ex_view_count(ex) == 1 && ex_view_entry(ex, 0) == 0, "a name substring finds one entry");
    ex_search_set(ex, "png");
    TEST_ASSERT(ex_view_count(ex) == 1 && ex_view_entry(ex, 0) == 0, "an extension finds the image");
    ex_search_set(ex, "folder");
    TEST_ASSERT(ex_view_count(ex) == 1 && ex_view_entry(ex, 0) == 2, "a type string finds the folder");

    /* 3. Invariant: every visible row is an entry that MATCHES the query. This is the assertion that
     *    catches a filter applied to the drawing but not to the hit-test. */
    ex_search_set(ex, "o");
    int all_match = 1;
    for (int r = 0; r < ex_view_count(ex); r++) {
        int e = ex_view_entry(ex, r);
        if (e < 0 || !ex_entry_matches_search(&ex->entries[e], "o")) { all_match = 0; break; }
    }
    TEST_ASSERT(all_match, "every visible row matches the query");
    ex_search_set(ex, "p");     /* only report.png holds a p */
    TEST_ASSERT(ex_view_count(ex) < total, "a matching query hides the entries it does not match");

    /* 4. Case-insensitive: the same query shouted shows the same rows (Windows behaviour). */
    int before = ex_view_count(ex);      /* still the "p" view */
    ex_search_set(ex, "P");
    TEST_ASSERT(ex_view_count(ex) == before, "search ignores case");
    ex_search_set(ex, "repo");

    /* 4. A query matching nothing empties the list, and row 0 stops being an entry (a click on that
     *    row must find nothing rather than entry 0). */
    ex_search_set(ex, "zzzz-no-such-entry-xyz");
    TEST_ASSERT(ex_view_count(ex) == 0, "a query matching nothing shows nothing");
    TEST_ASSERT(ex_view_entry(ex, 0) == -1, "row 0 of an empty view is not an entry");

    /* 5. Focus is clamped: a selection past the new end is what makes Delete act on nothing. */
    ex_search_clear(ex);
    ex->focus_idx = ex->entry_count - 1;
    ex_search_set(ex, "zzzz-no-such-entry-xyz");
    TEST_ASSERT(ex->focus_idx < ex_view_count(ex), "focus is clamped into the filtered view");
    ex_search_clear(ex);
    TEST_ASSERT(ex_view_count(ex) == ex->entry_count, "clearing the query restores the list");

    /* 6. The field's box: absent when the toolbar is too narrow to hold it, and otherwise to the left
     *    of the view label -- the click handler asks the same function, so a field that was not drawn
     *    cannot be focused. */
    int fx, fy, fw, fh;
    TEST_ASSERT(!ex_search_rect(ex, 0, 0, 500, ex_toolbar_h(), &fx, &fy, &fw, &fh),
                "no search field when the toolbar has no room for one");
    TEST_ASSERT(ex_search_rect(ex, 0, 0, 1024, ex_toolbar_h(), &fx, &fy, &fw, &fh),
                "the field exists on a normal toolbar");
    TEST_ASSERT(fx > 390, "the field sits right of the buttons");
    TEST_ASSERT(fx + fw <= 1024 - 180, "the field does not run under the view label");
    TEST_ASSERT(fh > 0 && fy + fh <= ex_toolbar_h(), "the field fits inside the toolbar strip");

    /* 6b. It has to be VISIBLE, not just state: render the toolbar onto a real surface and count the
     *     pixels the field paints. A field that exists in a struct and never draws is the failure this
     *     tree keeps shipping (the tab strip had the same gate for the same reason). */
    vbe_init(1024, 768);
    vbe_reset_clip();
    wubu_theme_set(THEME_DARK);
    vbe_fill_rect(0, 0, 1024, ex_toolbar_h(), 0x000000);   /* a known background to measure against */
    ex_search_set(ex, "");
    ex_render_toolbar(ex, NULL, 1024, 768, 0, 0, 1024, ex_toolbar_h());
    fx = fy = fw = fh = 0;
    bool have_rect = ex_search_rect(ex, 0, 0, 1024, ex_toolbar_h(), &fx, &fy, &fw, &fh);
    TEST_ASSERT(have_rect, "the field has a rect to measure");
    if (!have_rect) {        /* nothing was drawn: measuring a rect that does not exist is how this
                              * assertion used to meet uninitialised values (the mutation run hung) */
        explorer_shutdown();
        return;
    }
    int placeholder_px = 0;
    unsigned long placeholder_sig = 0;
    for (int yy = fy; yy < fy + fh; yy++)
        for (int xx = fx; xx < fx + fw; xx++) {
            uint32_t p = vbe_get_pixel(xx, yy);
            if (p != 0) placeholder_px++;
            placeholder_sig = placeholder_sig * 31u + p;
        }
    TEST_ASSERT(placeholder_px > fw, "the search field is painted, not just state");

    /* The placeholder and a query must render DIFFERENTLY -- otherwise the box shows "Search" while
     * the list is filtered, and the user has no idea why most files vanished. */
    vbe_fill_rect(0, 0, 1024, ex_toolbar_h(), 0x000000);
    ex_search_set(ex, "report");
    ex_render_toolbar(ex, NULL, 1024, 768, 0, 0, 1024, ex_toolbar_h());
    int query_px = 0;
    unsigned long query_sig = 0;
    for (int yy = fy; yy < fy + fh; yy++)
        for (int xx = fx; xx < fx + fw; xx++) {
            uint32_t p = vbe_get_pixel(xx, yy);
            if (p != 0) query_px++;
            query_sig = query_sig * 31u + p;
        }
    TEST_ASSERT(query_px > 0, "the typed query paints inside the field");
    TEST_ASSERT(query_sig != placeholder_sig,
                "a typed query renders differently from the placeholder");
    ex_search_clear(ex);

    /* 7. Ctrl+F focuses the field (the find-next modal is gone). */
    explorer_handle_key('f', 0x02);          /* MOD_CTRL */
    TEST_ASSERT(ex->search_active, "Ctrl+F focuses the search field");
    explorer_handle_key('z', 0);             /* a text field eats the character */
    TEST_ASSERT(strcmp(ex->search, "z") == 0, "typing filters instead of running a shortcut");
    explorer_handle_key(0x01, 0);            /* Esc */
    TEST_ASSERT(!ex->search_active && ex->search[0] == '\0', "Esc clears and releases the field");

    /* Negative control: MOD_ALT is 0x04, the mask this test used to pass as Ctrl. It must not focus
     * the field, so a following character must not become the query. */
    explorer_handle_key('f', 0x04);
    explorer_handle_key('q', 0);
    TEST_ASSERT(!ex->search_active && ex->search[0] == '\0',
                "MOD_ALT is NOT read as Ctrl (the field did not open)");

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

/* Context-menu row geometry -- the contract between the renderer and hit-testing.
 *
 * The menu was entirely unclickable before, and it becomes unclickable again the moment
 * these two disagree, so the mapping is pinned here instead of living only in a comment.
 * Rows MUST mirror ex_render_context_menu:
 *   0..4 Open/Open With.../Cut/Copy/Paste | 5 separator | 6..8 Delete/Rename/Properties
 */
static void test_ctx_menu_rows(void) {
    printf("\n=== Test: context menu row geometry ===\n");

    const int mx = 200, my = 200;         /* menu origin */

    /* THE ROUND TRIP IS THE CONTRACT. The renderer draws item `i` at
     * menu_y + 2 + ex_ctx_item_row(i) * EX_CTX_ITEM_H; the input path converts a click
     * to a row with ex_ctx_row_at() and then back to an item with ex_ctx_item_at_row().
     * If any of those three disagree, the menu becomes unclickable -- which is how it
     * shipped. So walk every item through all three and demand identity. */
    for (int i = 0; i < EX_CTX_ITEMS; i++) {
        int row = ex_ctx_item_row(i);
        TEST_ASSERT(row >= 0, "item has a display row");
        int y = my + 2 + row * EX_CTX_ITEM_H + 1;      /* 1px inside its own row */
        int clicked_row = ex_ctx_row_at(mx + 5, y, mx, my);
        int clicked_item = ex_ctx_item_at_row(clicked_row);
        if (row < 0 || clicked_item != i) {
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "item %d: drawn at row %d, click resolves to item %d",
                     i, row, clicked_item);
            TEST_ASSERT(0, msg);
        } else {
            tests_passed++;          /* no tests_run counter in this harness */
        }
    }

    /* The separator occupies a row of its own and must never resolve to an item. */
    int sep_row = EX_CTX_SEPARATOR_ROW;
    TEST_ASSERT(ex_ctx_row_at(mx + 5, my + 2 + sep_row * EX_CTX_ITEM_H + 1, mx, my)
                    == sep_row,
                "the separator row is clickable as a row");
    TEST_ASSERT(ex_ctx_item_at_row(sep_row) == -1,
                "the separator row is not an item");
    TEST_ASSERT(ex_ctx_item_at_row(EX_CTX_ROWS) == -1, "a row past the last is refused");
    TEST_ASSERT(ex_ctx_item_row(-1) == -1, "a negative item is refused");
    TEST_ASSERT(ex_ctx_item_row(EX_CTX_ITEMS) == -1, "an item past the last is refused");

    /* Refusals that must stay refusals. The menu's top edge is menu_y+2, so a click at
     * exactly that y IS row 0; only ABOVE it is empty. All four arguments, always --
     * an earlier version of this test passed two, which is undefined behaviour. */
    TEST_ASSERT(ex_ctx_row_at(mx - 5,    my + 2, mx, my) == -1, "left of the menu refused");
    TEST_ASSERT(ex_ctx_row_at(mx + 1000, my + 2, mx, my) == -1, "right of the menu refused");
    TEST_ASSERT(ex_ctx_row_at(mx + 5,    my + 1, mx, my) == -1, "above the menu refused");
    TEST_ASSERT(ex_ctx_row_at(mx + 5,    my + 2, mx, my) == 0,  "menu top edge is row 0");
    TEST_ASSERT(ex_ctx_row_at(mx + 5,    my + 2, -1,  my) == -1,
                "closed menu refuses all clicks");
}


static void test_tabs(void) {
    printf("\n=== Test: Tabs (Win11) ===\n");

    explorer_init();
    TEST_ASSERT(explorer_tab_count() == 1, "one tab exists as soon as the explorer opens");

    explorer_navigate("/tmp");
    int t1 = explorer_tab_open("/home");
    TEST_ASSERT(t1 == 1, "a second tab was created");
    TEST_ASSERT(explorer_tab_count() == 2, "two tabs");
    TEST_ASSERT(strcmp(explorer_current_path(), "/home") == 0, "the new tab is active at /home");

    /* Switching saves the active tab and restores the target's path -- the whole point. */
    TEST_ASSERT(explorer_tab_switch(0), "switched to tab 0");
    TEST_ASSERT(strcmp(explorer_current_path(), "/tmp") == 0, "tab 0 restored /tmp");
    TEST_ASSERT(strcmp(explorer_tab_path(1), "/home") == 0, "tab 1 remembers /home");

    TEST_ASSERT(explorer_tab_close(1), "closed tab 1");
    TEST_ASSERT(explorer_tab_count() == 1, "one tab left");
    TEST_ASSERT(!explorer_tab_close(0), "the last tab is not closable");

    /* Geometry: the strip owns a band, and two cards do not overlap. */
    int w0 = 0, w1 = 0, x0 = 0, x1 = 0;
    explorer_tab_open("/tmp");
    TEST_ASSERT(explorer_tab_rect(0, 1024, &x0, &w0), "tab 0 has a rect");
    TEST_ASSERT(explorer_tab_rect(1, 1024, &x1, &w1), "tab 1 has a rect");
    TEST_ASSERT(x1 >= x0 + w0, "tab cards do not overlap");
    TEST_ASSERT(x1 + w1 <= 1024, "tab cards stay inside the width");
    int expect_top = ex_breadcrumb_h() +
                     (g_explorer.toolbar_visible ? ex_toolbar_h() : 0) + ex_tab_h();
    TEST_ASSERT(ex_content_top() == expect_top,
                "content starts below breadcrumbs + toolbar + strip");

    /* It has to be VISIBLE: render and count pixels the strip painted in its band.
     * explorer_render() returns early unless the explorer is OPEN -- every other render
     * assertion in this file only checks "doesn't crash", which is why none of them would
     * have noticed a surface that never painted. */
    /* A real surface, and the strip's own renderer: explorer_render() returns early unless
     * explorer_is_open(), and that asks the WM for a window this link graph does not carry.
     * Driving ex_render_tabs() directly measures the drawing rather than the window. */
    int strip_y = ex_breadcrumb_h() + (g_explorer.toolbar_visible ? ex_toolbar_h() : 0);
    vbe_init(1024, 768);
    vbe_reset_clip();     /* a clip left from an earlier test silently discards every fill */
    wubu_theme_set(THEME_DARK);
    ex_render_tabs(&g_explorer, 1024, strip_y, ex_tab_h());
    /* Count across BOTH cards: the active one paints accent, the inactive one paints the
     * window face, and "non-zero" alone assumed the face was non-black -- with no theme set
     * it is black, so the first version of this gate measured nothing. */
    int painted = 0;
    for (int y = strip_y; y < strip_y + ex_tab_h(); y++)
        for (int x = x0; x < x1 + w1; x++)
            if (vbe_get_pixel(x, y) != 0) painted++;
    TEST_ASSERT(painted > w0 * 2, "the tab strip is painted, not just state");

    explorer_shutdown();
}

/* -- Libraries: a merged list over several real folders -------------------------------- */
static void test_library(void) {
    printf("\n-- Libraries --\n");

    char root[] = "/tmp/extestXXXXXX";
    if (!mkdtemp(root)) { TEST_ASSERT(0, "temp dir for the library fixture"); return; }
    char home[300], conf[400], pics[400], albums[400], f1[500], f2[500];
    snprintf(home, sizeof home, "%s/home", root);
    snprintf(conf, sizeof conf, "%s/libraries.conf", root);
    snprintf(pics, sizeof pics, "%s/Pictures", home);
    snprintf(albums, sizeof albums, "%s/Albums", home);
    snprintf(f1, sizeof f1, "%s/one.txt", pics);
    snprintf(f2, sizeof f2, "%s/two.txt", albums);
    mkdir(home, 0755); mkdir(pics, 0755); mkdir(albums, 0755);
    FILE *g = fopen(f1, "w"); if (g) { fputs("one", g); fclose(g); }
    g = fopen(f2, "w"); if (g) { fputs("two", g); fclose(g); }
    setenv("HOME", home, 1);
    setenv("WUBU_LIBRARIES_CONF", conf, 1);

    explorer_init();
    TEST_ASSERT(!explorer_in_library(), "a fresh Explorer is not in a library");

    /* Put a second folder in Pictures so the view has to merge two directories. */
    {
        WubuLibraries libs;
        wubu_libraries_load(&libs);
        (void)wubu_library_add(&libs, WUBU_LIB_PICTURES, albums);
        (void)wubu_libraries_save(&libs);
    }

    TEST_ASSERT(explorer_open_library(WUBU_LIB_PICTURES), "the Pictures library opens");
    TEST_ASSERT(explorer_in_library(), "the Explorer knows it is in a library");
    TEST_ASSERT(explorer_library_id() == WUBU_LIB_PICTURES, "the open library is Pictures");

    ExExplorerState *ex = explorer_state();
    TEST_ASSERT(ex->entry_count >= 2, "the merged list holds the files from both folders");
    bool saw_one = false, saw_two = false;
    for (int i = 0; i < ex->entry_count; i++) {
        if (strcmp(ex->entries[i].name, "one.txt") == 0) saw_one = true;
        if (strcmp(ex->entries[i].name, "two.txt") == 0) saw_two = true;
        TEST_ASSERT(ex->entries[i].full_path[0] == '/', "every merged entry has an absolute path");
        TEST_ASSERT(ex->entries[i].type_str[0] != '\0', "every merged entry names its source folder");
    }
    TEST_ASSERT(saw_one && saw_two, "files from both library folders are listed");

    char status[160];
    ex_library_status(ex, status, sizeof status);
    TEST_ASSERT(strstr(status, "Pictures library") != NULL, "the status bar names the library");
    TEST_ASSERT(strstr(status, "2 folder") != NULL, "the status bar counts the library's folders");

    /* The nav-pane shortcuts: the hit-test must agree with the block the renderer draws. */
    int block = ex_library_block_h();
    int tree_y = ex_content_top();
    for (int i = 0; i < 4; i++) {
        int sy = tree_y + i * (block / 4);
        TEST_ASSERT(ex_hit_library_shortcut(ex, tree_y, 10, sy + 5) == i,
                    "each shortcut row hits its own library");
    }
    TEST_ASSERT(ex_hit_library_shortcut(ex, tree_y, 10, tree_y + block) == -1,
                "below the block is the tree, not a shortcut");

    /* Navigating to a real folder leaves library mode: a merged list that survives navigation would
     * show a folder's contents under another folder's name. */
    explorer_navigate(pics);
    TEST_ASSERT(!explorer_in_library(), "navigating to a real path leaves library mode");
    /* This graph scans through the stub filesystem, so the count is not asserted here -- what matters
     * is that the merged entries are GONE, not what a stubbed directory scan returns. */
    bool leaked = false;
    for (int i = 0; i < ex->entry_count; i++)
        if (strcmp(ex->entries[i].name, "two.txt") == 0) leaked = true;
    TEST_ASSERT(!leaked, "a real folder does not keep showing the library's other files");

    /* Refresh inside a library re-merges instead of scanning the anchor folder. */
    TEST_ASSERT(explorer_open_library(WUBU_LIB_PICTURES), "the library reopens");
    explorer_refresh();
    TEST_ASSERT(explorer_in_library(), "refresh keeps library mode");
    TEST_ASSERT(explorer_state()->entry_count >= 2, "refresh re-merges both folders");

    /* The pseudo-path is what the nav pane navigates to, so it must route to the library. */
    explorer_navigate("//lib:2");
    TEST_ASSERT(explorer_library_id() == WUBU_LIB_MUSIC, "the //lib:<id> path opens that library");

    explorer_shutdown();
    char rm[500];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }
}

/* -- Toolbar: every painted button must be a target ------------------------------------ */
static void test_toolbar(void) {
    printf("\n-- Toolbar buttons --\n");
    explorer_init();
    ExExplorerState *ex = explorer_state();

    TEST_ASSERT(ex_toolbar_button_count(ex) == 5, "a plain folder has five toolbar buttons");
    TEST_ASSERT(strcmp(ex_toolbar_button_label(ex, 0), "Back") == 0, "button 0 is Back");
    TEST_ASSERT(strcmp(ex_toolbar_button_label(ex, 4), "New Folder") == 0, "button 4 is New Folder");
    TEST_ASSERT(ex_toolbar_button_label(ex, 5) == NULL, "there is no sixth button in a folder");

    int last_r = -1;
    for (int i = 0; i < ex_toolbar_button_count(ex); i++) {
        int bx, by, bw, bh;
        TEST_ASSERT(ex_toolbar_button_rect(ex, i, 0, 0, &bx, &by, &bw, &bh), "every button has a rect");
        TEST_ASSERT(by >= 0 && by + bh <= ex_toolbar_h(), "every button sits inside the toolbar strip");
        TEST_ASSERT(bx > last_r, "buttons do not overlap and are laid out left to right");
        last_r = bx + bw;
    }

    /* Pixels: the rect the click handler uses must be the rect that was PAINTED. */
    vbe_init(1024, 768);
    vbe_reset_clip();
    wubu_theme_set(THEME_DARK);
    ex->library = -1;
    ex_render_toolbar(ex, NULL, 1024, 768, 0, 0, 1024, ex_toolbar_h());
    for (int i = 0; i < ex_toolbar_button_count(ex); i++) {
        int bx, by, bw, bh;
        ex_toolbar_button_rect(ex, i, 0, 0, &bx, &by, &bw, &bh);
        /* The centre of a button can be glyph ink; the strip behind it is dark but NOT black, so
         * "!= 0" would pass for a button that was never painted. Compare the button's own face colour
         * at a point the label cannot cover. */
        uint32_t p = vbe_get_pixel(bx + bw - 6, by + bh / 2);
        TEST_ASSERT(p == tc()->btn_face, "the button is painted with its own face where it is clickable");
    }

    /* In a library the strip grows by one button, and the hit-test follows. */
    ex->library = WUBU_LIB_PICTURES;
    TEST_ASSERT(ex_toolbar_button_count(ex) == 6, "a library has a sixth button");
    /* The label is allowed to be NULL (that is what "no sixth button" looks like); strcmp(NULL, ...) is
     * how a mutant SEGFAULTED instead of failing this gate. */
    const char *l5 = ex_toolbar_button_label(ex, 5);
    TEST_ASSERT(l5 != NULL && strcmp(l5, "Add folders") == 0, "the sixth button is Add folders");
    int bx, by, bw, bh;
    TEST_ASSERT(ex_toolbar_button_rect(ex, 5, 0, 0, &bx, &by, &bw, &bh), "it has a rect");
    TEST_ASSERT(bx >= 390, "it sits after the five folder buttons, where the strip has room");
    ex->library = -1;
    explorer_shutdown();
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  explorer Test Suite                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    test_init_shutdown();
    test_guest_store_visible();
    test_guest_file_ops();
    test_tabs();
    test_navigation();
    test_view_modes();
    test_sort();
    test_toggles();
    test_selection();
    test_list_scrolling();
    test_file_ops();
    test_helpers();
    test_drive_enumeration();
    test_history();
    test_zip();
    test_state_accessors();
    test_search();
    test_shift_select();
    test_image_preview();
    test_ctx_menu_rows();
    test_library();
    test_toolbar();

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Results: %d passed, %d failed                          ║\n", tests_passed, tests_failed);
    printf("╚══════════════════════════════════════════════════════════╝\n");

    return tests_failed > 0 ? 1 : 0;
}
