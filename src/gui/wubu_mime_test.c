#include "wubu_mime.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Testing MIME system...\n");
    wubu_mime_init();

    /* Test built-in MIME types */
    const MimeTypeEntry *mime = wubu_mime_lookup_by_extension("test.txt");
    assert(mime && strcmp(mime->mime_type, "text/plain") == 0);
    
    mime = wubu_mime_lookup_by_extension("image.png");
    assert(mime && strcmp(mime->mime_type, "image/png") == 0);
    
    mime = wubu_mime_lookup_by_type("application/pdf");
    assert(mime && strcmp(mime->extension, ".pdf") == 0);

    /* Test guess type */
    const char *type = wubu_mime_guess_type("document.pdf");
    assert(strcmp(type, "application/pdf") == 0);
    
    type = wubu_mime_guess_type("unknown.xyz");
    assert(strcmp(type, "application/octet-stream") == 0);

    /* Test text file detection */
    assert(wubu_mime_is_text_file("readme.md") == true);
    assert(wubu_mime_is_text_file("photo.jpg") == false);

    /* Test description */
    const char *desc = wubu_mime_get_description("song.mp3");
    assert(strcmp(desc, "MP3 Audio") == 0);

    /* Test default handler */
    const char *handler = wubu_mime_get_default("image/png");
    assert(handler && strcmp(handler, "wubu-image-viewer") == 0);

    /* Test desktop entry scanning */
    int count = wubu_mime_state()->desktop_entry_count;
    printf("Found %d desktop entries\n", count);
    for (int i = 0; i < count; i++) {
        printf("  - %s (%s)\n", wubu_mime_state()->desktop_entries[i].id, wubu_mime_state()->desktop_entries[i].name);
    }

    wubu_mime_shutdown();
    printf("✅ All MIME tests passed\n");
    return 0;
}