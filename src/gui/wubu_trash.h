#ifndef WUBU_TRASH_H
#define WUBU_TRASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>

/* -- Trash Constants ------------------------------------------------ */

#define TRASH_MAX_ENTRIES 4096
#define TRASH_INFO_VERSION 1

/* -- Trash Entry ---------------------------------------------------- */

typedef struct {
    char original_path[4096];     /* Original absolute path */
    char deleted_path[4096];      /* Path in trash/files/ */
    char info_path[4096];         /* Path in trash/info/ */
    uint64_t size;                /* File size in bytes */
    time_t deletion_time;         /* When moved to trash */
    bool is_directory;
} TrashEntry;

/* -- Trash State ---------------------------------------------------- */

typedef struct {
    char trash_dir[4096];         /* ~/.local/share/Trash */
    char files_dir[4096];         /* ~/.local/share/Trash/files */
    char info_dir[4096];          /* ~/.local/share/Trash/info */
    
    TrashEntry entries[TRASH_MAX_ENTRIES];
    int entry_count;
    
    uint64_t max_size_bytes;      /* Max trash size (0 = unlimited) */
    uint64_t current_size_bytes;  /* Current total size */
    
    bool auto_expire_enabled;     /* Auto-delete old items */
    int auto_expire_days;         /* Days before auto-delete (0 = disabled) */
} TrashState;

/* -- Trash API ------------------------------------------------------ */

/* Initialize trash system */
int  wubu_trash_init(void);
void wubu_trash_shutdown(void);

/* Get trash state */
TrashState *wubu_trash_state(void);

/* Move file/directory to trash */
int wubu_trash_move(const char *path);

/* Restore from trash */
int wubu_trash_restore(const char *trash_name, char *restored_path, size_t path_size);

/* Restore all items */
int wubu_trash_restore_all(void);

/* Permanently delete from trash */
int wubu_trash_delete(const char *trash_name);

/* Empty trash (delete all) */
int wubu_trash_empty(void);

/* List trash contents */
int wubu_trash_list(TrashEntry **out_entries, int *out_count);

/* Get trash entry by index */
const TrashEntry *wubu_trash_get(int index);

/* Find trash entry by original path */
int wubu_trash_find_by_path(const char *original_path);

/* Size management */
uint64_t wubu_trash_get_size(void);
uint64_t wubu_trash_get_max_size(void);
void wubu_trash_set_max_size(uint64_t max_bytes);

/* Auto-expire */
void wubu_trash_enable_auto_expire(int days);
void wubu_trash_disable_auto_expire(void);
int wubu_trash_run_auto_expire(void);

/* Check if path is in trash */
bool wubu_trash_is_in_trash(const char *path);

/* Get unique trash name for a file (handles conflicts) */
const char *wubu_trash_get_unique_name(const char *original_path, char *out_name, size_t out_size);

#endif /* WUBU_TRASH_H */