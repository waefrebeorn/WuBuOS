/*
 * file_assoc.c -- the extension -> handler table, in one place.
 */
#include "file_assoc.h"

#include <string.h>

const char *file_assoc_ext(const char *path)
{
    static char ext[16];
    ext[0] = '\0';
    if (!path) return ext;

    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(slash ? slash + 1 : path, '.');
    if (!dot || dot[1] == '\0') return ext;

    size_t n = 0;
    for (const char *p = dot + 1; *p && n < sizeof ext - 1; p++, n++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        ext[n] = c;
    }
    ext[n] = '\0';
    return ext;
}

/* Text-ish formats Notepad can genuinely read and re-save (it handles plain bytes). Anything needing a parser
 * we do not ship -- images, PDFs, archives -- is ASSOC_NONE so the caller asks instead of opening garbage. */
static int is_text_ext(const char *e)
{
    static const char *const text[] = {
        "txt", "log", "md", "json", "ini", "cfg", "conf", "csv", "yaml", "yml", "xml", "nfo", "asc",
    };
    for (size_t i = 0; i < sizeof text / sizeof text[0]; i++)
        if (strcmp(e, text[i]) == 0) return 1;
    return 0;
}

AssocKind file_assoc_kind(const char *path)
{
    const char *e = file_assoc_ext(path);
    if (!e[0]) return ASSOC_NONE;

    if (strcmp(e, "wet") == 0) return ASSOC_WET;
    if (strcmp(e, "c") == 0 || strcmp(e, "h") == 0 || strcmp(e, "hc") == 0) return ASSOC_WCODE;
    if (is_text_ext(e)) return ASSOC_NOTEPAD;
    return ASSOC_NONE;
}

const char *file_assoc_label(AssocKind kind)
{
    switch (kind) {
        case ASSOC_NOTEPAD: return "Notepad";
        case ASSOC_WCODE:   return "W-Code";
        case ASSOC_WET:     return ".WET applet";
        default:            return "";
    }
}
