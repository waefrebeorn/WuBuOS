/* explorer_preview.c -- File preview subsystem.
 *
 * Self-contained: builds the preview metadata/text/image descriptor for the
 * selected entry. Uses g_explorer (extern), ex_get_extension (explorer_tree.c),
 * and the public ex_9p_* backend. Minimal includes.
 */

#include "../egojar.h"
#include "explorer_internal.h"
#include "shell/preview_decode.h"
#include <stdlib.h>
#include <string.h>

void explorer_update_preview(int idx) {
    /* A preview buffer belongs to ONE selection; leaving the old one in place while a new
     * file is chosen is how a pane shows the previous photo under the new file's name. */
    free(g_explorer.preview.img_pixels);
    g_explorer.preview.img_pixels = NULL;
    g_explorer.preview.img_w = g_explorer.preview.img_h = 0;

    if (idx < 0 || idx >= g_explorer.entry_count) {
        g_explorer.preview.type = EX_PREVIEW_NONE;
        return;
    }

    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) {
        g_explorer.preview.type = EX_PREVIEW_NONE;
        return;
    }

    strncpy(g_explorer.preview.path, entry->full_path, EX_MAX_PATH - 1);
    g_explorer.preview.file_size = entry->size;
    g_explorer.preview.modified = entry->modified;
    strncpy(g_explorer.preview.mime_type, entry->type_str, 63);

    if (entry->type == EX_ENTRY_DIR) {
        g_explorer.preview.type = EX_PREVIEW_METADATA;
    } else {
        const char *ext = ex_get_extension(entry->name);
        if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
            strcmp(ext, "log") == 0 || strcmp(ext, "c") == 0 ||
            strcmp(ext, "h") == 0 || strcmp(ext, "cpp") == 0 ||
            strcmp(ext, "hpp") == 0 || strcmp(ext, "py") == 0 ||
            strcmp(ext, "js") == 0 || strcmp(ext, "sh") == 0 ||
            strcmp(ext, "rs") == 0 || strcmp(ext, "json") == 0 ||
            strcmp(ext, "xml") == 0 || strcmp(ext, "html") == 0 ||
            strcmp(ext, "css") == 0) {
            g_explorer.preview.type = EX_PREVIEW_TEXT;

            int fd = ex_9p_open(entry->full_path, O_RDONLY);
            if (fd >= 0) {
                ssize_t n = ex_9p_read(fd, g_explorer.preview.text_buffer, sizeof(g_explorer.preview.text_buffer) - 1);
                if (n > 0) {
                    g_explorer.preview.text_buffer[n] = '\0';
                    g_explorer.preview.text_lines = 0;
                    for (int i = 0; i < n; i++) {
                        if (g_explorer.preview.text_buffer[i] == '\n') g_explorer.preview.text_lines++;
                    }
                }
                ex_9p_close(fd);
            }
        } else if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 ||
                   strcmp(ext, "jpeg") == 0 || strcmp(ext, "gif") == 0 ||
                   strcmp(ext, "bmp") == 0) {
            /* Our own decoders: wubu_pixmap (PNG/BMP), wubu_gif (first frame), wubu_jpeg. */
            uint32_t *px = NULL;
            int iw = 0, ih = 0;
            if (wubu_preview_decode_file(entry->full_path, &px, &iw, &ih, 0) == WUBU_PREVIEW_OK) {
                g_explorer.preview.type = EX_PREVIEW_IMAGE;
                g_explorer.preview.img_pixels = px;
                g_explorer.preview.img_w = iw;
                g_explorer.preview.img_h = ih;
            } else {
                /* a format we cannot read is described, not drawn as an empty pane */
                g_explorer.preview.type = EX_PREVIEW_METADATA;
            }
        } else {
            g_explorer.preview.type = EX_PREVIEW_METADATA;
        }
    }
}
