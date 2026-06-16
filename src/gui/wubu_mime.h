#ifndef WUBU_MIME_H
#define WUBU_MIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -- MIME Type Database --------------------------------------------- */

#define MAX_MIME_TYPES 256
#define MAX_EXTENSION_LEN 32
#define MAX_MIME_TYPE_LEN 128
#define MAX_HANDLER_CMD_LEN 512

typedef enum {
    MIME_ACTION_OPEN = 0,
    MIME_ACTION_EDIT = 1,
    MIME_ACTION_PRINT = 2,
    MIME_ACTION_VIEW = 3
} MimeAction;

/* MIME type entry */
typedef struct {
    char extension[MAX_EXTENSION_LEN];    /* e.g., ".png" */
    char mime_type[MAX_MIME_TYPE_LEN];    /* e.g., "image/png" */
    char description[128];                /* Human-readable */
    char default_handler[MAX_HANDLER_CMD_LEN]; /* Default .desktop ID */
    bool is_text;                         /* Text vs binary */
    int priority;                         /* Higher = preferred */
} MimeTypeEntry;

/* Desktop Entry (simplified .desktop file parser) */
#define MAX_DESKTOP_ENTRIES 128
#define MAX_DESKTOP_KEY_LEN 64
#define MAX_DESKTOP_VAL_LEN 512
#define MAX_DESKTOP_ACTIONS 8

typedef struct {
    char id[128];                         /* e.g., "wubu-text-editor" */
    char name[128];
    char comment[256];
    char exec[MAX_HANDLER_CMD_LEN];       /* Command with %f, %u placeholders */
    char icon[128];
    char terminal[8];                     /* "true"/"false" */
    char type[16];                        /* "Application"/"Link"/"Directory" */
    char categories[256];
    char mime_types[512];                 /* Semicolon-separated */
    char actions[256];                    /* Semicolon-separated action names */
    bool hidden;
    bool no_display;
    int startup_notify;
    
    /* Parsed actions */
    struct {
        char name[64];
        char exec[MAX_HANDLER_CMD_LEN];
    } action_list[MAX_DESKTOP_ACTIONS];
    int action_count;
} DesktopEntry;

/* File Association (extension -> handler) */
typedef struct {
    char extension[MAX_EXTENSION_LEN];
    char handler_id[128];                 /* Desktop entry ID */
    MimeAction action;
} FileAssociation;

/* MIME System State */
typedef struct {
    MimeTypeEntry mime_types[MAX_MIME_TYPES];
    int mime_type_count;
    
    DesktopEntry desktop_entries[MAX_DESKTOP_ENTRIES];
    int desktop_entry_count;
    
    FileAssociation associations[MAX_MIME_TYPES];
    int association_count;
    
    /* Cache directories */
    char user_data_dir[512];      /* ~/.local/share/applications */
    char system_data_dirs[8][512]; /* /usr/share/applications, etc. */
    int system_data_dir_count;
} MimeSystem;

/* -- MIME System API ------------------------------------------------- */

/* Initialize MIME system (scans .desktop files) */
int  wubu_mime_init(void);
void wubu_mime_shutdown(void);

/* Get global state */
MimeSystem *wubu_mime_state(void);

/* MIME type lookup */
const MimeTypeEntry *wubu_mime_lookup_by_extension(const char *filename);
const MimeTypeEntry *wubu_mime_lookup_by_type(const char *mime_type);
const char *wubu_mime_guess_type(const char *filename);

/* Desktop entry management */
int wubu_mime_load_desktop_file(const char *path);
int wubu_mime_scan_desktop_dirs(void);
const DesktopEntry *wubu_mime_find_handler(const char *mime_type, MimeAction action);
const DesktopEntry *wubu_mime_get_desktop_entry(const char *id);

/* File associations */
int wubu_mime_associate(const char *extension, const char *handler_id, MimeAction action);
int wubu_mime_remove_association(const char *extension, MimeAction action);
const char *wubu_mime_get_association(const char *extension, MimeAction action);

/* Launch application */
int wubu_mime_launch(const char *file_path, const char *handler_id);
int wubu_mime_launch_with_mime(const char *file_path, const char *mime_type, MimeAction action);
int wubu_mime_open_url(const char *url);

/* Default applications */
int wubu_mime_set_default(const char *mime_type, const char *handler_id);
const char *wubu_mime_get_default(const char *mime_type);

/* Helpers */
bool wubu_mime_is_text_file(const char *filename);
const char *wubu_mime_get_description(const char *filename);
void wubu_mime_list_handlers(const char *mime_type, const DesktopEntry ***out, int *count);

#endif /* WUBU_MIME_H */