/*
 * file_type_icons.c -- one owner for "what does this file LOOK like".
 *
 * The file list drew every row's icon as a 16 px filled square in the entry's
 * colour, so a .c file, a .png and a folder were the same square in three tints
 * (measured: every row in the File Manager). Windows has a glyph per type; we
 * now have the bakes (assets/icons/files -> wubu_icons_data.h), and this module
 * is the single place that decides which one a name gets.
 *
 * Pure: name + is_dir in, a type and a mask out. No rendering state, no theme.
 */

#include "file_type_icons.h"
#include "../icons/wubu_icons_data.h"
#include "window_chrome.h"

#include <string.h>
#include <ctype.h>

/* Extension -> family. First match wins; the table is ordered so the common
 * cases are cheap. A name with no extension, or an unknown one, is GENERIC --
 * never a blank box. */
static const struct { const char *ext; WubuFileType type; } kExt[] = {
    { "txt",   WUBU_FT_TEXT },   { "md",    WUBU_FT_TEXT },   { "log",  WUBU_FT_TEXT },
    { "nfo",   WUBU_FT_TEXT },   { "readme",WUBU_FT_TEXT },
    { "c",     WUBU_FT_CODE },   { "h",     WUBU_FT_CODE },   { "py",   WUBU_FT_CODE },
    { "sh",    WUBU_FT_CODE },   { "js",    WUBU_FT_CODE },   { "ts",   WUBU_FT_CODE },
    { "json",  WUBU_FT_CODE },   { "toml",  WUBU_FT_CODE },   { "yaml", WUBU_FT_CODE },
    { "yml",   WUBU_FT_CODE },   { "lua",   WUBU_FT_CODE },   { "holy", WUBU_FT_CODE },
    { "mk",    WUBU_FT_CODE },   { "css",   WUBU_FT_CODE },   { "html", WUBU_FT_CODE },
    { "png",   WUBU_FT_IMAGE },  { "jpg",   WUBU_FT_IMAGE },  { "jpeg", WUBU_FT_IMAGE },
    { "gif",   WUBU_FT_IMAGE },  { "webp",  WUBU_FT_IMAGE },  { "bmp",  WUBU_FT_IMAGE },
    { "svg",   WUBU_FT_IMAGE },  { "ico",   WUBU_FT_IMAGE },
    { "wav",   WUBU_FT_AUDIO },  { "mp3",   WUBU_FT_AUDIO },  { "flac", WUBU_FT_AUDIO },
    { "ogg",   WUBU_FT_AUDIO },  { "mid",   WUBU_FT_AUDIO },
    { "mp4",   WUBU_FT_VIDEO },  { "mkv",   WUBU_FT_VIDEO },  { "avi",  WUBU_FT_VIDEO },
    { "mov",   WUBU_FT_VIDEO },  { "webm",  WUBU_FT_VIDEO },
    { "zip",   WUBU_FT_ARCHIVE },{ "tar",   WUBU_FT_ARCHIVE },{ "gz",   WUBU_FT_ARCHIVE },
    { "xz",    WUBU_FT_ARCHIVE },{ "wubu",  WUBU_FT_ARCHIVE },{ "7z",   WUBU_FT_ARCHIVE },
    { "elf",   WUBU_FT_BINARY }, { "bin",   WUBU_FT_BINARY }, { "exe",  WUBU_FT_BINARY },
    { "out",   WUBU_FT_BINARY }, { "so",    WUBU_FT_BINARY },
    { "db",    WUBU_FT_DATA },   { "sqlite",WUBU_FT_DATA },   { "csv",  WUBU_FT_DATA },
    { "tsv",   WUBU_FT_DATA },   { "ics",   WUBU_FT_DATA },
    { "conf",  WUBU_FT_CONFIG }, { "ini",   WUBU_FT_CONFIG }, { "cfg",  WUBU_FT_CONFIG },
    { "env",   WUBU_FT_CONFIG }, { "desktop", WUBU_FT_CONFIG },{ "service", WUBU_FT_CONFIG },
};

/* Lowercased extension after the LAST dot, or "" when there is none. A leading
 * dot (".bashrc") is a name, not an extension. */
static void ext_of(const char *name, char *out, size_t n) {
    out[0] = '\0';
    if (!name) return;
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name || !dot[1]) return;
    size_t i = 0;
    for (dot++; *dot && i + 1 < n && i < 15; dot++, i++)
        out[i] = (char)tolower((unsigned char)*dot);
    out[i] = '\0';
}

WubuFileType wubu_file_type_for(const char *name, int is_dir) {
    if (is_dir) return WUBU_FT_FOLDER;
    char ext[16];
    ext_of(name, ext, sizeof ext);
    if (!ext[0]) return WUBU_FT_GENERIC;
    for (size_t i = 0; i < sizeof kExt / sizeof kExt[0]; i++)
        if (strcmp(ext, kExt[i].ext) == 0) return kExt[i].type;
    return WUBU_FT_GENERIC;
}

const char *wubu_file_type_label(WubuFileType t) {
    switch (t) {
        case WUBU_FT_FOLDER:  return "File Folder";
        case WUBU_FT_TEXT:    return "Text Document";
        case WUBU_FT_CODE:    return "Source File";
        case WUBU_FT_IMAGE:   return "Image";
        case WUBU_FT_AUDIO:   return "Audio";
        case WUBU_FT_VIDEO:   return "Video";
        case WUBU_FT_ARCHIVE: return "Archive";
        case WUBU_FT_BINARY:  return "Program";
        case WUBU_FT_DATA:    return "Data File";
        case WUBU_FT_CONFIG:  return "Configuration";
        default:              return "File";
    }
}

static const unsigned char *mask_for(WubuFileType t) {
    switch (t) {
        case WUBU_FT_FOLDER:  return wubu_icon_apps_folder;   /* one folder master, not two */
        case WUBU_FT_TEXT:    return wubu_icon_files_text;
        case WUBU_FT_CODE:    return wubu_icon_files_code;
        case WUBU_FT_IMAGE:   return wubu_icon_files_image;
        case WUBU_FT_AUDIO:   return wubu_icon_files_audio;
        case WUBU_FT_VIDEO:   return wubu_icon_files_video;
        case WUBU_FT_ARCHIVE: return wubu_icon_files_archive;
        case WUBU_FT_BINARY:  return wubu_icon_files_binary;
        case WUBU_FT_DATA:    return wubu_icon_files_data;
        case WUBU_FT_CONFIG:  return wubu_icon_files_config;
        default:              return wubu_icon_files_generic;
    }
}

int wubu_file_icon_draw(const char *name, int is_dir,
                        int x, int y, int w, int h, uint32_t ink) {
    const unsigned char *m = mask_for(wubu_file_type_for(name, is_dir));
    if (!m) return 0;
    chrome_icon_draw_mask(m, x, y, w, h, ink);
    return 1;
}

int wubu_file_icon_draw_type(WubuFileType t, int x, int y, int w, int h, uint32_t ink) {
    const unsigned char *m = mask_for(t);
    if (!m) return 0;
    chrome_icon_draw_mask(m, x, y, w, h, ink);
    return 1;
}
