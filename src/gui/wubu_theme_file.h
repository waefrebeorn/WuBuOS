/*
 * wubu_theme_file.h -- one text file defines a whole theme.
 *
 * The Omarchy idea worth taking (MIT, github.com/omacom/omarchy): a theme is a
 * single source of truth that every surface reads, so changing the look means
 * editing one file instead of rebuilding. WuBuOS already has runtime theme
 * switching, but the palettes live in a C array and need a recompile to add.
 *
 * Format (comments with '#', blank lines ignored, unknown keys rejected):
 *     desktop_bg     = #008080
 *     win_face       = C0C0C0
 *     win_title_active = #000080
 *
 * Both '#RRGGBB' and 'RRGGBB' are accepted. Parse is pure (no I/O), so it is
 * testable and usable from the kernel; load_file is the hosted convenience.
 */
#ifndef WUBU_THEME_FILE_H
#define WUBU_THEME_FILE_H

#include <stdint.h>
#include "wubu_theme.h"

/* Parse theme text into `out` (untouched fields keep their previous value).
 * Returns the number of keys applied, or -1 on a malformed line / bad colour /
 * unknown key. A non-negative result means every key in the text was valid. */
int wubu_theme_parse(const char *text, WubuThemeColors *out);

/* Read a file and parse it. Returns wubu_theme_parse's result, -2 if the file
 * cannot be read or is larger than 64 KiB. */
int wubu_theme_load_file(const char *path, WubuThemeColors *out);

/* Name of a colour key, or NULL. Exposed so the WM can list what a theme may
 * set without duplicating the table. */
const char *wubu_theme_key_name(int index);

#endif /* WUBU_THEME_FILE_H */
