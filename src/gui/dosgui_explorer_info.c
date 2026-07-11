/* dosgui_explorer_info.c -- WuBuOS explorer: file metadata extraction.
 * Extracted from dosgui_explorer.c (separable leaf). Self-contained: stat +
 * type-by-extension classification. Uses the public/internal explorer API
 * (ex_9p_stat, ex_get_extension, dosgui_explorer_type_color).
 * C11, minimal includes.
 */
#include "dosgui_explorer.h"
#include "dosgui_explorer_internal.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

void ex_get_file_info(const char *path, ExEntry *entry) {
    struct stat st;
    if (ex_9p_stat(path, &st) != 0) {
        entry->type = EX_ENTRY_UNKNOWN;
        entry->size = 0;
        entry->modified = 0;
        entry->created = 0;
        strcpy(entry->type_str, "Unknown");
        entry->icon_color = dosgui_explorer_type_color(EX_ENTRY_UNKNOWN);
        return;
    }

    entry->size = st.st_size;
    entry->modified = st.st_mtime;
    entry->created = st.st_ctime;
    entry->hidden = (entry->name[0] == '.');
    entry->readonly = !(st.st_mode & S_IWUSR);
    entry->icon_color = 0xFFFFFF;

    if (S_ISDIR(st.st_mode)) {
        entry->type = EX_ENTRY_DIR;
        strcpy(entry->type_str, "File Folder");
        entry->icon_color = 0xFFD700; /* Gold */
    } else if (S_ISLNK(st.st_mode)) {
        entry->type = EX_ENTRY_SYMLINK;
        strcpy(entry->type_str, "Shortcut");
        entry->icon_color = 0x00FFFF; /* Cyan */
    } else if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
        entry->type = EX_ENTRY_SPECIAL;
        strcpy(entry->type_str, "Device");
        entry->icon_color = 0x8000FF; /* Purple */
    } else {
        entry->type = EX_ENTRY_FILE;
        const char *ext = ex_get_extension(entry->name);
        strncpy(entry->extension, ext, sizeof(entry->extension) - 1);

        /* Determine type by extension */
        if (strcmp(ext, "zip") == 0 || strcmp(ext, "tar") == 0 ||
            strcmp(ext, "gz") == 0 || strcmp(ext, "bz2") == 0 ||
            strcmp(ext, "xz") == 0 || strcmp(ext, "7z") == 0) {
            entry->type = EX_ENTRY_ZIP;
            strcpy(entry->type_str, "Compressed Archive");
            entry->icon_color = 0xFF8000; /* Orange */
        } else if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
                   strcmp(ext, "log") == 0 || strcmp(ext, "ini") == 0 ||
                   strcmp(ext, "cfg") == 0 || strcmp(ext, "conf") == 0) {
            strcpy(entry->type_str, "Text Document");
            entry->icon_color = 0xFFFFFF;
        } else if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0 ||
                   strcmp(ext, "cpp") == 0 || strcmp(ext, "hpp") == 0 ||
                   strcmp(ext, "py") == 0 || strcmp(ext, "js") == 0 ||
                   strcmp(ext, "sh") == 0 || strcmp(ext, "rs") == 0) {
            strcpy(entry->type_str, "Source Code");
            entry->icon_color = 0x00FF00;
        } else if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 ||
                   strcmp(ext, "jpeg") == 0 || strcmp(ext, "gif") == 0 ||
                   strcmp(ext, "bmp") == 0 || strcmp(ext, "webp") == 0) {
            strcpy(entry->type_str, "Image");
            entry->icon_color = 0xFF00FF;
        } else if (strcmp(ext, "mp3") == 0 || strcmp(ext, "wav") == 0 ||
                   strcmp(ext, "flac") == 0 || strcmp(ext, "ogg") == 0) {
            strcpy(entry->type_str, "Audio");
            entry->icon_color = 0x00FFFF;
        } else if (strcmp(ext, "mp4") == 0 || strcmp(ext, "mkv") == 0 ||
                   strcmp(ext, "avi") == 0 || strcmp(ext, "mov") == 0) {
            strcpy(entry->type_str, "Video");
            entry->icon_color = 0xFFFF00;
        } else if (strcmp(ext, "exe") == 0 || strcmp(ext, "dll") == 0 ||
                   strcmp(ext, "so") == 0 || strcmp(ext, "dylib") == 0) {
            strcpy(entry->type_str, "Executable");
            entry->icon_color = 0xFF0000;
        } else if (strcmp(ext, "pdf") == 0) {
            strcpy(entry->type_str, "PDF Document");
            entry->icon_color = 0xFF0000;
        } else if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0 ||
                   strcmp(ext, "css") == 0) {
            strcpy(entry->type_str, "Web Document");
            entry->icon_color = 0x0000FF;
        } else {
            snprintf(entry->type_str, sizeof(entry->type_str), "%s File",
                     ext[0] ? ext : "Unknown");
            entry->icon_color = 0xFFFFFF;
        }
    }

    /* ex_format_time(entry->modified, entry->type_str + strlen(entry->type_str), 0); // Not used, just formatting */
}
