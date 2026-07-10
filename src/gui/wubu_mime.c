#include "wubu_mime_internal.h"
#include "wubu_mime.h"
#include "wubu_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <glob.h>
#include <strings.h>

/* ============================================================
 * Internal State
 * ============================================================ */
static MimeSystem g_mime = {0};

/* ============================================================
 * Built-in MIME Types (fallback)
 * ============================================================ */
static const MimeTypeEntry builtin_mimes[] = {
    {".txt", "text/plain", "Plain Text", "wubu-text-editor", true, 10},
    {".md", "text/markdown", "Markdown", "wubu-text-editor", true, 10},
    {".c", "text/x-csrc", "C Source", "wubu-code-editor", true, 10},
    {".h", "text/x-chdr", "C Header", "wubu-code-editor", true, 10},
    {".cpp", "text/x-c++src", "C++ Source", "wubu-code-editor", true, 10},
    {".py", "text/x-python", "Python Script", "wubu-code-editor", true, 10},
    {".js", "text/javascript", "JavaScript", "wubu-code-editor", true, 10},
    {".html", "text/html", "HTML Document", "wubu-browser", true, 10},
    {".css", "text/css", "CSS Stylesheet", "wubu-code-editor", true, 10},
    {".json", "application/json", "JSON Data", "wubu-code-editor", true, 10},
    {".xml", "application/xml", "XML Document", "wubu-code-editor", true, 10},
    {".pdf", "application/pdf", "PDF Document", "wubu-pdf-viewer", false, 10},
    {".png", "image/png", "PNG Image", "wubu-image-viewer", false, 10},
    {".jpg", "image/jpeg", "JPEG Image", "wubu-image-viewer", false, 10},
    {".jpeg", "image/jpeg", "JPEG Image", "wubu-image-viewer", false, 10},
    {".gif", "image/gif", "GIF Image", "wubu-image-viewer", false, 10},
    {".bmp", "image/bmp", "BMP Image", "wubu-image-viewer", false, 10},
    {".svg", "image/svg+xml", "SVG Image", "wubu-image-viewer", true, 10},
    {".mp3", "audio/mpeg", "MP3 Audio", "wubu-audio-player", false, 10},
    {".wav", "audio/wav", "WAV Audio", "wubu-audio-player", false, 10},
    {".ogg", "audio/ogg", "Ogg Audio", "wubu-audio-player", false, 10},
    {".flac", "audio/flac", "FLAC Audio", "wubu-audio-player", false, 10},
    {".mp4", "video/mp4", "MP4 Video", "wubu-video-player", false, 10},
    {".mkv", "video/x-matroska", "Matroska Video", "wubu-video-player", false, 10},
    {".webm", "video/webm", "WebM Video", "wubu-video-player", false, 10},
    {".zip", "application/zip", "ZIP Archive", "wubu-archive-manager", false, 10},
    {".tar", "application/x-tar", "TAR Archive", "wubu-archive-manager", false, 10},
    {".gz", "application/gzip", "GZIP Archive", "wubu-archive-manager", false, 10},
    {".wubu", "application/x-wubu", "WuBuOS Container", "wubu-container-runner", false, 20},
    {""  , "application/octet-stream", "Unknown", "wubu-text-editor", false, 0}
};

/* ============================================================
 * String Utilities
 * ============================================================ */

/* ============================================================
 * MIME System API
 * ============================================================ */
MimeSystem *wubu_mime_state(void) {
    return &g_mime;
}

int wubu_mime_init(void) {
    memset(&g_mime, 0, sizeof(g_mime));

    /* Load built-in MIME types */
    for (size_t i = 0; i < sizeof(builtin_mimes)/sizeof(builtin_mimes[0]); i++) {
        if (g_mime.mime_type_count < MAX_MIME_TYPES) {
            g_mime.mime_types[g_mime.mime_type_count++] = builtin_mimes[i];
        }
    }

    /* Setup data directories */
    const char *home = getenv("HOME");
    if (home) {
        snprintf(g_mime.user_data_dir, sizeof(g_mime.user_data_dir), "%s/.local/share/applications", home);
    }

    const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
    const char *defaults[] = {"/usr/share/applications", "/usr/local/share/applications", NULL};
    const char **dirs = xdg_data_dirs ? (const char**)xdg_data_dirs : defaults;
    
    if (xdg_data_dirs) {
        char *copy = strdup(xdg_data_dirs);
        char *tok = strtok(copy, ":");
        while (tok && g_mime.system_data_dir_count < 8) {
            strncpy(g_mime.system_data_dirs[g_mime.system_data_dir_count++], tok, 511);
            tok = strtok(NULL, ":");
        }
        free(copy);
    } else {
        for (int i = 0; defaults[i] && g_mime.system_data_dir_count < 8; i++) {
            strncpy(g_mime.system_data_dirs[g_mime.system_data_dir_count++], defaults[i], 511);
        }
    }

    wubu_mime_scan_desktop_dirs();
    return 0;
}

void wubu_mime_shutdown(void) {
    /* Free any dynamically allocated strings in desktop entries */
    for (int i = 0; i < g_mime.desktop_entry_count; i++) {
        /* Currently all strings are in fixed buffers */
    }
    g_mime.desktop_entry_count = 0;
    g_mime.association_count = 0;
}

int wubu_mime_load_desktop_file(const char *path) {
    if (g_mime.desktop_entry_count >= MAX_DESKTOP_ENTRIES) return -1;
    
    DesktopEntry entry;
    if (!parse_desktop_file(path, &entry)) return -1;
    if (entry.hidden || entry.no_display) return 0;
    if (strcmp(entry.type, "Application") != 0) return 0;

    g_mime.desktop_entries[g_mime.desktop_entry_count++] = entry;
    return 0;
}

int wubu_mime_scan_desktop_dirs(void) {
    /* User directory first (overrides system) */
    DIR *d = opendir(g_mime.user_data_dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (str_endswith(ent->d_name, ".desktop")) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", g_mime.user_data_dir, ent->d_name);
                wubu_mime_load_desktop_file(path);
            }
        }
        closedir(d);
    }

    /* System directories */
    for (int i = 0; i < g_mime.system_data_dir_count; i++) {
        d = opendir(g_mime.system_data_dirs[i]);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d))) {
                if (str_endswith(ent->d_name, ".desktop")) {
                    char path[1024];
                    snprintf(path, sizeof(path), "%s/%s", g_mime.system_data_dirs[i], ent->d_name);
                    /* Don't override user entries with same ID */
                    bool exists = false;
                    const char *base = strrchr(ent->d_name, '/');
                    base = base ? base + 1 : ent->d_name;
                    char *dot = strrchr(base, '.');
                    if (dot) *dot = '\0';
                    for (int j = 0; j < g_mime.desktop_entry_count; j++) {
                        if (strcmp(g_mime.desktop_entries[j].id, base) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (dot) *dot = '.';
                    if (!exists) wubu_mime_load_desktop_file(path);
                }
            }
            closedir(d);
        }
    }
    return 0;
}

const MimeTypeEntry *wubu_mime_lookup_by_extension(const char *filename) {
    const char *ext = get_file_extension(filename);
    if (!ext || !*ext) return NULL;

    for (int i = g_mime.mime_type_count - 1; i >= 0; i--) {
        if (strcasecmp(g_mime.mime_types[i].extension, ext) == 0) {
            return &g_mime.mime_types[i];
        }
    }
    return NULL;
}

const MimeTypeEntry *wubu_mime_lookup_by_type(const char *mime_type) {
    if (!mime_type) return NULL;
    for (int i = 0; i < g_mime.mime_type_count; i++) {
        if (strcmp(g_mime.mime_types[i].mime_type, mime_type) == 0) {
            return &g_mime.mime_types[i];
        }
    }
    return NULL;
}

const char *wubu_mime_guess_type(const char *filename) {
    const MimeTypeEntry *mime = wubu_mime_lookup_by_extension(filename);
    return mime ? mime->mime_type : "application/octet-stream";
}

const DesktopEntry *wubu_mime_find_handler(const char *mime_type, MimeAction action) {
    (void)action; /* For now, ignore action */
    if (!mime_type) return NULL;

    /* First check associations */
    for (int i = 0; i < g_mime.association_count; i++) {
        /* For simplicity, check if any associated file's MIME type matches */
        const MimeTypeEntry *mime = wubu_mime_lookup_by_extension(g_mime.associations[i].extension);
        if (mime && strcmp(mime->mime_type, mime_type) == 0) {
            return wubu_mime_get_desktop_entry(g_mime.associations[i].handler_id);
        }
    }

    /* Find best matching desktop entry */
    const DesktopEntry *best = NULL;
    int best_priority = -1;

    for (int i = 0; i < g_mime.desktop_entry_count; i++) {
        const DesktopEntry *de = &g_mime.desktop_entries[i];
        if (de->mime_types[0]) {
            /* Check if this handler supports the MIME type */
            char *copy = strdup(de->mime_types);
            char *tok = strtok(copy, ";");
            while (tok) {
                if (strcmp(tok, mime_type) == 0) {
                    /* Find priority from builtin mimes */
                    int priority = 0;
                    for (size_t k = 0; k < sizeof(builtin_mimes)/sizeof(builtin_mimes[0]); k++) {
                        if (strcmp(builtin_mimes[k].mime_type, mime_type) == 0) {
                            priority = builtin_mimes[k].priority;
                            break;
                        }
                    }
                    if (priority > best_priority) {
                        best_priority = priority;
                        best = de;
                    }
                    break;
                }
                tok = strtok(NULL, ";");
            }
            free(copy);
        }
    }

    /* Fallback to default handler from builtin */
    if (!best) {
        const MimeTypeEntry *mime = wubu_mime_lookup_by_type(mime_type);
        if (mime && mime->default_handler[0]) {
            best = wubu_mime_get_desktop_entry(mime->default_handler);
        }
    }

    return best;
}

const DesktopEntry *wubu_mime_get_desktop_entry(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < g_mime.desktop_entry_count; i++) {
        if (strcmp(g_mime.desktop_entries[i].id, id) == 0) {
            return &g_mime.desktop_entries[i];
        }
    }
    return NULL;
}

int wubu_mime_associate(const char *extension, const char *handler_id, MimeAction action) {
    if (!extension || !handler_id) return -1;
    if (g_mime.association_count >= MAX_MIME_TYPES) return -1;

    /* Check if already exists */
    for (int i = 0; i < g_mime.association_count; i++) {
        if (strcmp(g_mime.associations[i].extension, extension) == 0 &&
            g_mime.associations[i].action == action) {
            strncpy(g_mime.associations[i].handler_id, handler_id, 127);
            return 0;
        }
    }

    FileAssociation *fa = &g_mime.associations[g_mime.association_count++];
    strncpy(fa->extension, extension, MAX_EXTENSION_LEN - 1);
    strncpy(fa->handler_id, handler_id, 127);
    fa->action = action;
    return 0;
}

int wubu_mime_remove_association(const char *extension, MimeAction action) {
    for (int i = 0; i < g_mime.association_count; i++) {
        if (strcmp(g_mime.associations[i].extension, extension) == 0 &&
            g_mime.associations[i].action == action) {
            for (int j = i; j < g_mime.association_count - 1; j++) {
                g_mime.associations[j] = g_mime.associations[j + 1];
            }
            g_mime.association_count--;
            return 0;
        }
    }
    return -1;
}

const char *wubu_mime_get_association(const char *extension, MimeAction action) {
    for (int i = 0; i < g_mime.association_count; i++) {
        if (strcmp(g_mime.associations[i].extension, extension) == 0 &&
            g_mime.associations[i].action == action) {
            return g_mime.associations[i].handler_id;
        }
    }
    return NULL;
}

/* Simple command execution with placeholder substitution */
static int execute_command(const char *cmd_template, const char *file_path) {
    if (!cmd_template || !file_path) return -1;

    char cmd[1024];
    const char *src = cmd_template;
    char *dst = cmd;
    char *end = cmd + sizeof(cmd) - 1;

    while (*src && dst < end) {
        if (src[0] == '%' && src[1]) {
            switch (src[1]) {
                case 'f': /* Single file */
                case 'F': /* Multiple files (use first) */
                    dst += snprintf(dst, end - dst, "'%s'", file_path);
                    break;
                case 'u': /* Single URL */
                case 'U': /* Multiple URLs */
                    dst += snprintf(dst, end - dst, "'%s'", file_path);
                    break;
                default:
                    *dst++ = *src;
                    *dst++ = src[1];
            }
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    /* Execute in background */
    int pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    } else if (pid > 0) {
        return 0;
    }
    return -1;
}

int wubu_mime_launch(const char *file_path, const char *handler_id) {
    const DesktopEntry *de = wubu_mime_get_desktop_entry(handler_id);
    if (!de || !de->exec[0]) return -1;
    return execute_command(de->exec, file_path);
}

int wubu_mime_launch_with_mime(const char *file_path, const char *mime_type, MimeAction action) {
    const DesktopEntry *de = wubu_mime_find_handler(mime_type, action);
    if (!de) return -1;
    return execute_command(de->exec, file_path);
}

int wubu_mime_open_url(const char *url) {
    if (!url) return -1;
    const DesktopEntry *de = wubu_mime_find_handler("x-scheme-handler/http", MIME_ACTION_OPEN);
    if (!de) de = wubu_mime_find_handler("x-scheme-handler/https", MIME_ACTION_OPEN);
    if (!de) return -1;
    return execute_command(de->exec, url);
}

int wubu_mime_set_default(const char *mime_type, const char *handler_id) {
    const MimeTypeEntry *mime = wubu_mime_lookup_by_type(mime_type);
    if (!mime) return -1;
    /* Find in mutable array and update */
    for (int i = 0; i < g_mime.mime_type_count; i++) {
        if (strcmp(g_mime.mime_types[i].mime_type, mime_type) == 0) {
            strncpy(g_mime.mime_types[i].default_handler, handler_id, MAX_HANDLER_CMD_LEN - 1);
            return 0;
        }
    }
    return -1;
}

const char *wubu_mime_get_default(const char *mime_type) {
    const MimeTypeEntry *mime = wubu_mime_lookup_by_type(mime_type);
    return mime ? mime->default_handler : NULL;
}

bool wubu_mime_is_text_file(const char *filename) {
    const MimeTypeEntry *mime = wubu_mime_lookup_by_extension(filename);
    return mime ? mime->is_text : false;
}

const char *wubu_mime_get_description(const char *filename) {
    const MimeTypeEntry *mime = wubu_mime_lookup_by_extension(filename);
    return mime ? mime->description : "Unknown File Type";
}

void wubu_mime_list_handlers(const char *mime_type, const DesktopEntry ***out, int *count) {
    static const DesktopEntry *handlers[MAX_DESKTOP_ENTRIES];
    int n = 0;

    for (int i = 0; i < g_mime.desktop_entry_count && n < MAX_DESKTOP_ENTRIES; i++) {
        const DesktopEntry *de = &g_mime.desktop_entries[i];
        if (de->mime_types[0]) {
            char *copy = strdup(de->mime_types);
            char *tok = strtok(copy, ";");
            while (tok) {
                if (strcmp(tok, mime_type) == 0) {
                    handlers[n++] = de;
                    break;
                }
                tok = strtok(NULL, ";");
            }
            free(copy);
        }
    }
    *out = handlers;
    *count = n;
}
