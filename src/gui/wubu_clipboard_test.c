/*
 * wubu_clipboard_test.c -- WuBuOS Clipboard Manager Test (logic only)
 * Tests internal clipboard logic without Wayland integration
 */

#define WUBU_CLIPBOARD_TEST_MODE 1
#include "wubu_clipboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test the internal clipboard logic without Wayland */
int main(void) {
    printf("=== WuBuOS Clipboard Logic Test ===\n\n");
    
    /* Test 1: Set clipboard text */
    printf("Test 1: Set clipboard text...\n");
    bool ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, "Hello, WuBuOS!");
    if (!ok) {
        printf("  FAIL: Failed to set clipboard text\n");
        return 1;
    }
    printf("  PASS: Clipboard text set\n");
    
    /* Test 2: Check clipboard has data */
    printf("\nTest 2: Check clipboard has data...\n");
    bool has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_CLIPBOARD);
    if (!has) {
        printf("  FAIL: Clipboard should have data\n");
        return 1;
    }
    printf("  PASS: Clipboard has data\n");
    
    /* Test 3: Get clipboard text */
    printf("\nTest 3: Get clipboard text...\n");
    char *text = NULL;
    size_t len = 0;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text) {
        printf("  FAIL: Failed to get clipboard text\n");
        return 1;
    }
    if (strcmp(text, "Hello, WuBuOS!") != 0) {
        printf("  FAIL: Text mismatch: got '%s'\n", text);
        free(text);
        return 1;
    }
    printf("  PASS: Got text '%s' (len=%zu)\n", text, len);
    free(text);
    
    /* Test 4: Set primary selection */
    printf("\nTest 4: Set primary selection...\n");
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_PRIMARY, "Primary selection text");
    if (!ok) {
        printf("  FAIL: Failed to set primary selection\n");
        return 1;
    }
    printf("  PASS: Primary selection set\n");
    
    /* Test 5: Check primary selection has data */
    printf("\nTest 5: Check primary selection has data...\n");
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_PRIMARY);
    if (!has) {
        printf("  FAIL: Primary selection should have data\n");
        return 1;
    }
    printf("  PASS: Primary selection has data\n");
    
    /* Test 6: Get primary selection text */
    printf("\nTest 6: Get primary selection text...\n");
    text = NULL;
    len = 0;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_PRIMARY, &text, &len);
    if (!ok || !text) {
        printf("  FAIL: Failed to get primary selection text\n");
        return 1;
    }
    if (strcmp(text, "Primary selection text") != 0) {
        printf("  FAIL: Text mismatch: got '%s'\n", text);
        free(text);
        return 1;
    }
    printf("  PASS: Got text '%s' (len=%zu)\n", text, len);
    free(text);
    
    /* Test 7: Clear clipboard */
    printf("\nTest 7: Clear clipboard...\n");
    wubu_clipboard_clear(CLIPBOARD_SELECTION_CLIPBOARD);
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_CLIPBOARD);
    if (has) {
        printf("  FAIL: Clipboard should be empty after clear\n");
        return 1;
    }
    printf("  PASS: Clipboard cleared\n");
    
    /* Test 8: Clear primary selection */
    printf("\nTest 8: Clear primary selection...\n");
    wubu_clipboard_clear(CLIPBOARD_SELECTION_PRIMARY);
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_PRIMARY);
    if (has) {
        printf("  FAIL: Primary selection should be empty after clear\n");
        return 1;
    }
    printf("  PASS: Primary selection cleared\n");
    
    /* Test 9: Convenience helpers */
    printf("\nTest 9: Convenience helpers...\n");
    ok = wubu_clipboard_copy("Copy test");
    if (!ok) {
        printf("  FAIL: wubu_clipboard_copy failed\n");
        return 1;
    }
    text = NULL;
    ok = wubu_clipboard_paste(&text);
    if (!ok || !text || strcmp(text, "Copy test") != 0) {
        printf("  FAIL: wubu_clipboard_paste failed\n");
        free(text);
        return 1;
    }
    printf("  PASS: copy/paste round-trip: '%s'\n", text);
    free(text);
    
    ok = wubu_primary_copy("Primary copy test");
    if (!ok) {
        printf("  FAIL: wubu_primary_copy failed\n");
        return 1;
    }
    text = NULL;
    ok = wubu_primary_paste(&text);
    if (!ok || !text || strcmp(text, "Primary copy test") != 0) {
        printf("  FAIL: wubu_primary_paste failed\n");
        free(text);
        return 1;
    }
    printf("  PASS: primary copy/paste round-trip: '%s'\n", text);
    free(text);
    
    /* Test 10: Empty string handling */
    printf("\nTest 10: Empty string handling...\n");
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, "");
    if (!ok) {
        printf("  FAIL: Failed to set empty string\n");
        return 1;
    }
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_CLIPBOARD);
    if (has) {
        printf("  FAIL: Empty string should result in empty clipboard\n");
        return 1;
    }
    printf("  PASS: Empty string handled correctly\n");
    
    /* Test 11: NULL handling */
    printf("\nTest 11: NULL handling...\n");
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, NULL);
    if (!ok) {
        printf("  FAIL: Failed to set NULL\n");
        return 1;
    }
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_CLIPBOARD);
    if (has) {
        printf("  FAIL: NULL should result in empty clipboard\n");
        return 1;
    }
    printf("  PASS: NULL handled correctly\n");
    
    /* Test 12: Set data with MIME */
    printf("\nTest 12: Set data with MIME...\n");
    ClipboardData data = {
        .mime_type = CLIPBOARD_MIME_TEXT,
        .data = "MIME data test",
        .size = strlen("MIME data test")
    };
    ok = wubu_clipboard_set_data(CLIPBOARD_SELECTION_CLIPBOARD, &data, 1);
    if (!ok) {
        printf("  FAIL: Failed to set data with MIME\n");
        return 1;
    }
    text = NULL;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text || strcmp(text, "MIME data test") != 0) {
        printf("  FAIL: MIME data mismatch\n");
        free(text);
        return 1;
    }
    printf("  PASS: MIME data round-trip: '%s'\n", text);
    free(text);
    
    /* Test 13: Unicode text */
    printf("\nTest 13: Unicode text...\n");
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, "Hello 世界 🌍");
    if (!ok) {
        printf("  FAIL: Failed to set unicode text\n");
        return 1;
    }
    text = NULL;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text || strcmp(text, "Hello 世界 🌍") != 0) {
        printf("  FAIL: Unicode text mismatch\n");
        free(text);
        return 1;
    }
    printf("  PASS: Unicode text round-trip: '%s' (len=%zu)\n", text, len);
    free(text);
    
    /* Test 14: Large text */
    printf("\nTest 14: Large text...\n");
    char large_text[10000];
    for (int i = 0; i < 9999; i++) {
        large_text[i] = 'a' + (i % 26);
    }
    large_text[9999] = '\0';
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, large_text);
    if (!ok) {
        printf("  FAIL: Failed to set large text\n");
        return 1;
    }
    text = NULL;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text || len != 9999 || memcmp(text, large_text, 9999) != 0) {
        printf("  FAIL: Large text mismatch\n");
        free(text);
        return 1;
    }
    printf("  PASS: Large text round-trip (len=%zu)\n", len);
    free(text);
    
    /* Test 15: Multi-MIME clipboard (text + HTML) */
    printf("\nTest 15: Multi-MIME clipboard (text + HTML)...\n");
    ClipboardData multi_data[] = {
        {.mime_type = CLIPBOARD_MIME_TEXT, .data = "Plain text content", .size = strlen("Plain text content")},
        {.mime_type = CLIPBOARD_MIME_HTML, .data = "<b>HTML content</b>", .size = strlen("<b>HTML content</b>")},
    };
    ok = wubu_clipboard_set_data(CLIPBOARD_SELECTION_CLIPBOARD, multi_data, 2);
    if (!ok) {
        printf("  FAIL: Failed to set multi-MIME data\n");
        return 1;
    }
    has = wubu_clipboard_has_data(CLIPBOARD_SELECTION_CLIPBOARD);
    if (!has) {
        printf("  FAIL: Multi-MIME clipboard should have data\n");
        return 1;
    }
    
    /* Retrieve text */
    text = NULL;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text || strcmp(text, "Plain text content") != 0) {
        printf("  FAIL: Multi-MIME text mismatch: got '%s'\n", text ? text : "NULL");
        free(text);
        return 1;
    }
    free(text);
    
    /* Retrieve HTML */
    void *html_data = NULL;
    size_t html_len = 0;
    ok = wubu_clipboard_get_data(CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_MIME_HTML, &html_data, &html_len);
    if (!ok || !html_data || html_len != strlen("<b>HTML content</b>") || memcmp(html_data, "<b>HTML content</b>", html_len) != 0) {
        printf("  FAIL: Multi-MIME HTML mismatch\n");
        free(html_data);
        return 1;
    }
    printf("  PASS: Multi-MIME round-trip (text + HTML)\n");
    free(html_data);
    
    /* Test 16: MIME overwrite */
    printf("\nTest 16: MIME overwrite...\n");
    ok = wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, "New text only");
    if (!ok) {
        printf("  FAIL: Failed to overwrite with text\n");
        return 1;
    }
    text = NULL;
    ok = wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, &text, &len);
    if (!ok || !text || strcmp(text, "New text only") != 0) {
        printf("  FAIL: Overwrite text mismatch\n");
        free(text);
        return 1;
    }
    free(text);
    
    /* Try to get old HTML - should fail */
    html_data = NULL;
    html_len = 0;
    ok = wubu_clipboard_get_data(CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_MIME_HTML, &html_data, &html_len);
    if (ok && html_data) {
        printf("  FAIL: Old HTML should not exist after overwrite\n");
        free(html_data);
        return 1;
    }
    printf("  PASS: MIME overwrite works correctly\n");
    
    /* Test 17: Primary selection multi-MIME */
    printf("\nTest 17: Primary selection multi-MIME...\n");
    ClipboardData primary_data[] = {
        {.mime_type = CLIPBOARD_MIME_TEXT, .data = "Primary text", .size = strlen("Primary text")},
        {.mime_type = CLIPBOARD_MIME_URI_LIST, .data = "file:///home/user/file.txt", .size = strlen("file:///home/user/file.txt")},
    };
    ok = wubu_clipboard_set_data(CLIPBOARD_SELECTION_PRIMARY, primary_data, 2);
    if (!ok) {
        printf("  FAIL: Failed to set primary multi-MIME\n");
        return 1;
    }
    void *uri_data = NULL;
    size_t uri_len = 0;
    ok = wubu_clipboard_get_data(CLIPBOARD_SELECTION_PRIMARY, CLIPBOARD_MIME_URI_LIST, &uri_data, &uri_len);
    if (!ok || !uri_data || uri_len != strlen("file:///home/user/file.txt") || memcmp(uri_data, "file:///home/user/file.txt", uri_len) != 0) {
        printf("  FAIL: Primary URI list mismatch\n");
        free(uri_data);
        return 1;
    }
    printf("  PASS: Primary multi-MIME round-trip (text + URI list)\n");
    free(uri_data);

    printf("\n=== All tests passed! ===\n");
    return 0;
}