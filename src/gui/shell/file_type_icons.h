/*
 * file_type_icons.h -- the ONE place that answers "what does this file look like".
 *
 * A caller passes a file name (and whether it is a directory) and gets a type,
 * a label for the Type column, and a glyph drawn in the caller's box. The masks
 * are baked from assets/icons/files/*.svg; there is no per-caller icon table.
 */

#ifndef WUBU_FILE_TYPE_ICONS_H
#define WUBU_FILE_TYPE_ICONS_H

#include <stdint.h>

typedef enum {
    WUBU_FT_GENERIC = 0,
    WUBU_FT_TEXT,
    WUBU_FT_CODE,
    WUBU_FT_IMAGE,
    WUBU_FT_AUDIO,
    WUBU_FT_VIDEO,
    WUBU_FT_ARCHIVE,
    WUBU_FT_BINARY,
    WUBU_FT_DATA,
    WUBU_FT_CONFIG,
    WUBU_FT_FOLDER,
    WUBU_FT_COUNT
} WubuFileType;

/* Classify by name (extension, case-insensitive) and dir-ness. */
WubuFileType wubu_file_type_for(const char *name, int is_dir);

/* Human label for the Type column ("Source File", "Image", ...). */
const char *wubu_file_type_label(WubuFileType t);

/* Draw the type's glyph in a w x h box. Returns 0 only if the type has no
 * master, which cannot happen for a classified type (GENERIC always has one). */
int wubu_file_icon_draw(const char *name, int is_dir,
                        int x, int y, int w, int h, uint32_t ink);
int wubu_file_icon_draw_type(WubuFileType t, int x, int y, int w, int h, uint32_t ink);

#endif /* WUBU_FILE_TYPE_ICONS_H */
