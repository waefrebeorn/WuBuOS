#include "wubu_trash.h"
#include "wubu_theme.h"
#include "wubu_settings.h"
#include "../runtime/wubu_arch.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>

/* ============================================================
 * Internal State
 * ============================================================ */
static TrashState g_trash = {0};

/* ============================================================
 * Helpers
 * ============================================================ */
static uint64_t get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return (uint64_t)st.st_size;
    return 0;
}

static uint64_t get_dir_size(const char *path) {
    uint64_t total = 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        
        struct stat st;
        if (lstat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += get_dir_size(full);
            } else {
                total += (uint64_t)st.st_size;
            }
        }
    }
    closedir(d);
    return total;
}

static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path, 0700) == 0;
}

static char *get_unique_filename(const char *dir, const char *base_name) {
    char *result = malloc(4096);
    if (!result) return NULL;
    
    struct stat st;
    int counter = 0;
    
    while (true) {
        if (counter == 0) {
            snprintf(result, 4096, "%s/%s", dir, base_name);
        } else {
            char *dot = strrchr(base_name, '.');
            if (dot && dot > strrchr(base_name, '/')) {
                size_t base_len = dot - base_name;
                snprintf(result, 4096, "%s/%.*s_%d%s", dir, (int)base_len, base_name, counter, dot);
            } else {
                snprintf(result, 4096, "%s/%s_%d", dir, base_name, counter);
            }
        }
        
        if (stat(result, &st) != 0) {
            return result;
        }
        counter++;
        if (counter > 10000) break; /* Safety */
    }
    
    free(result);
    return NULL;
}

static void write_info_file(const char *info_path, const char *original_path, time_t deletion_time) {
    FILE *f = fopen(info_path, "w");
    if (!f) return;
    
    fprintf(f, "[Trash Info]\n");
    fprintf(f, "Version=%d\n", TRASH_INFO_VERSION);
    fprintf(f, "Path=%s\n", original_path);
    
    /* Format deletion time as ISO 8601 */
    struct tm *tm = localtime(&deletion_time);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm);
    fprintf(f, "DeletionDate=%s\n", time_buf);
    
    fclose(f);
}

static bool read_info_file(const char *info_path, char *original_path, size_t path_size, time_t *deletion_time) {
    FILE *f = fopen(info_path, "r");
    if (!f) return false;
    
    char line[512];
    bool has_version = false;
    bool has_path = false;
    bool has_date = false;
    time_t date = 0;
    
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\n\r")] = '\0';
        
        if (strcmp(key, "Version") == 0) {
            has_version = true;
        } else if (strcmp(key, "Path") == 0) {
            strncpy(original_path, val, path_size - 1);
            original_path[path_size - 1] = '\0';
            has_path = true;
        } else if (strcmp(key, "DeletionDate") == 0) {
            /* Parse ISO 8601: YYYY-MM-DDTHH:MM:SS */
            struct tm tm = {0};
            if (sscanf(val, "%d-%d-%dT%d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                date = mktime(&tm);
                has_date = true;
            }
        }
    }
    fclose(f);
    
    if (deletion_time && has_date) *deletion_time = date;
    return has_version && has_path;
}

/* ============================================================
 * Trash API
 * ============================================================ */

TrashState *wubu_trash_state(void) {
    return &g_trash;
}

int wubu_trash_init(void) {
    memset(&g_trash, 0, sizeof(g_trash));
    
    /* Get trash directory from XDG or default */
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    
    if (xdg_data_home) {
        snprintf(g_trash.trash_dir, sizeof(g_trash.trash_dir), "%s/Trash", xdg_data_home);
    } else if (home) {
        snprintf(g_trash.trash_dir, sizeof(g_trash.trash_dir), "%s/.local/share/Trash", home);
    } else {
        return -1;
    }
    
    snprintf(g_trash.files_dir, sizeof(g_trash.files_dir), "%s/files", g_trash.trash_dir);
    snprintf(g_trash.info_dir, sizeof(g_trash.info_dir), "%s/info", g_trash.trash_dir);
    
    /* Create directories */
    ensure_dir(g_trash.trash_dir);
    ensure_dir(g_trash.files_dir);
    ensure_dir(g_trash.info_dir);
    
    /* Load existing trash entries */
    DIR *d = opendir(g_trash.info_dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && g_trash.entry_count < TRASH_MAX_ENTRIES) {
            if (!strstr(ent->d_name, ".trashinfo")) continue;
            
            char info_path[4096];
            snprintf(info_path, sizeof(info_path), "%s/%s", g_trash.info_dir, ent->d_name);
            
            char original_path[4096] = {0};
            time_t deletion_time = 0;
            
            if (read_info_file(info_path, original_path, sizeof(original_path), &deletion_time)) {
                /* Find corresponding file in files/ */
                char *base = strdup(ent->d_name);
                char *dot = strstr(base, ".trashinfo");
                if (dot) *dot = '\0';
                
                char file_path[4096];
                snprintf(file_path, sizeof(file_path), "%s/%s", g_trash.files_dir, base);
                free(base);
                
                struct stat st;
                if (stat(file_path, &st) == 0) {
                    TrashEntry *e = &g_trash.entries[g_trash.entry_count++];
                    strncpy(e->original_path, original_path, sizeof(e->original_path) - 1);
                    strncpy(e->deleted_path, file_path, sizeof(e->deleted_path) - 1);
                    strncpy(e->info_path, info_path, sizeof(e->info_path) - 1);
                    e->size = (uint64_t)st.st_size;
                    e->deletion_time = deletion_time;
                    e->is_directory = S_ISDIR(st.st_mode);
                    g_trash.current_size_bytes += e->size;
                }
            }
        }
        closedir(d);
    }
    
    /* Load settings if available */
    if (wubu_settings_get()) {
        const WubuSettings *s = wubu_settings_get();
        g_trash.max_size_bytes = s->privacy.auto_clear_temp ? 1024 * 1024 * 1024 : 0; /* 1GB default if enabled */
        g_trash.auto_expire_enabled = s->privacy.history_retention_days > 0;
        g_trash.auto_expire_days = s->privacy.history_retention_days;
    }
    
    return 0;
}

void wubu_trash_shutdown(void) {
    g_trash.entry_count = 0;
    g_trash.current_size_bytes = 0;
}

int wubu_trash_move(const char *path) {
    if (!path || !path[0]) return -1;
    
    /* Get absolute path */
    char abs_path[4096];
    if (!realpath(path, abs_path)) {
        /* File might not exist - try to get absolute path anyway */
        if (path[0] == '/') {
            strncpy(abs_path, path, sizeof(abs_path) - 1);
        } else {
            char cwd[4096];
            if (!getcwd(cwd, sizeof(cwd))) return -1;
            snprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
        }
    }
    
    struct stat st;
    if (lstat(abs_path, &st) != 0) return -1; /* Doesn't exist */
    
    /* Check if already in trash */
    if (strncmp(abs_path, g_trash.trash_dir, strlen(g_trash.trash_dir)) == 0) {
        return -1; /* Can't trash trash */
    }
    
    /* Check if we need to make room */
    uint64_t item_size = S_ISDIR(st.st_mode) ? get_dir_size(abs_path) : (uint64_t)st.st_size;
    
    while (g_trash.max_size_bytes > 0 && 
           g_trash.current_size_bytes + item_size > g_trash.max_size_bytes &&
           g_trash.entry_count > 0) {
        /* Remove oldest entry */
        int oldest_idx = 0;
        for (int i = 1; i < g_trash.entry_count; i++) {
            if (g_trash.entries[i].deletion_time < g_trash.entries[oldest_idx].deletion_time) {
                oldest_idx = i;
            }
        }
        wubu_trash_delete(g_trash.entries[oldest_idx].deleted_path);
    }
    
    /* Get base name for trash file */
    char *base = basename(abs_path);
    char *unique_name = get_unique_filename(g_trash.files_dir, base);
    if (!unique_name) return -1;
    
    /* Move file to trash */
    if (rename(abs_path, unique_name) != 0) {
        free(unique_name);
        return -1;
    }
    
    /* Write .trashinfo file */
    char *trashinfo_name = get_unique_filename(g_trash.info_dir, base);
    if (!trashinfo_name) {
        /* Restore file on failure */
        rename(unique_name, abs_path);
        free(unique_name);
        return -1;
    }
    
    /* Change extension to .trashinfo */
    char *dot = strrchr(trashinfo_name, '.');
    if (dot && dot > strrchr(trashinfo_name, '/')) {
        strcpy(dot, ".trashinfo");
    } else {
        strcat(trashinfo_name, ".trashinfo");
    }
    
    time_t now = time(NULL);
    write_info_file(trashinfo_name, abs_path, now);
    
    /* Add to entries */
    if (g_trash.entry_count < TRASH_MAX_ENTRIES) {
        TrashEntry *e = &g_trash.entries[g_trash.entry_count++];
        strncpy(e->original_path, abs_path, sizeof(e->original_path) - 1);
        strncpy(e->deleted_path, unique_name, sizeof(e->deleted_path) - 1);
        strncpy(e->info_path, trashinfo_name, sizeof(e->info_path) - 1);
        e->size = item_size;
        e->deletion_time = now;
        e->is_directory = S_ISDIR(st.st_mode);
        g_trash.current_size_bytes += item_size;
    }
    
    free(unique_name);
    free(trashinfo_name);
    return 0;
}

int wubu_trash_restore(const char *trash_name, char *restored_path, size_t path_size) {
    if (!trash_name) return -1;
    
    /* Find entry by deleted_path basename */
    int idx = -1;
    for (int i = 0; i < g_trash.entry_count; i++) {
        const char *base = basename(g_trash.entries[i].deleted_path);
        if (strcmp(base, trash_name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) return -1;
    
    TrashEntry *e = &g_trash.entries[idx];
    
    /* Ensure parent directory exists */
    char *parent = dirname(strdup(e->original_path));
    /* Use mkdir_p instead of system("mkdir -p") */
    wubu_arch_mkdir_p(parent, 0755);
    free(parent);
    
    /* Restore file */
    if (rename(e->deleted_path, e->original_path) != 0) {
        return -1;
    }
    
    /* Remove .trashinfo */
    unlink(e->info_path);
    
    /* Update size */
    g_trash.current_size_bytes -= e->size;
    
    if (restored_path) {
        strncpy(restored_path, e->original_path, path_size - 1);
        restored_path[path_size - 1] = '\0';
    }
    
    /* Remove from entries */
    for (int i = idx; i < g_trash.entry_count - 1; i++) {
        g_trash.entries[i] = g_trash.entries[i + 1];
    }
    g_trash.entry_count--;
    
    return 0;
}

int wubu_trash_restore_all(void) {
    int restored = 0;
    while (g_trash.entry_count > 0) {
        if (wubu_trash_restore(basename(g_trash.entries[0].deleted_path), NULL, 0) == 0) {
            restored++;
        } else {
            break;
        }
    }
    return restored;
}

int wubu_trash_delete(const char *trash_name) {
    if (!trash_name) return -1;
    
    int idx = -1;
    for (int i = 0; i < g_trash.entry_count; i++) {
        const char *base = basename(g_trash.entries[i].deleted_path);
        if (strcmp(base, trash_name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) return -1;
    
    TrashEntry *e = &g_trash.entries[idx];
    
    /* Remove file */
    unlink(e->deleted_path);
    /* Remove info */
    unlink(e->info_path);
    
    /* Update size */
    g_trash.current_size_bytes -= e->size;
    
    /* Remove from entries */
    for (int i = idx; i < g_trash.entry_count - 1; i++) {
        g_trash.entries[i] = g_trash.entries[i + 1];
    }
    g_trash.entry_count--;
    
    return 0;
}

int wubu_trash_empty(void) {
    for (int i = 0; i < g_trash.entry_count; i++) {
        unlink(g_trash.entries[i].deleted_path);
        unlink(g_trash.entries[i].info_path);
    }
    g_trash.entry_count = 0;
    g_trash.current_size_bytes = 0;
    return 0;
}

int wubu_trash_list(TrashEntry **out_entries, int *out_count) {
    if (out_entries) *out_entries = g_trash.entries;
    if (out_count) *out_count = g_trash.entry_count;
    return g_trash.entry_count;
}

const TrashEntry *wubu_trash_get(int index) {
    if (index < 0 || index >= g_trash.entry_count) return NULL;
    return &g_trash.entries[index];
}

int wubu_trash_find_by_path(const char *original_path) {
    if (!original_path) return -1;
    for (int i = 0; i < g_trash.entry_count; i++) {
        if (strcmp(g_trash.entries[i].original_path, original_path) == 0) {
            return i;
        }
    }
    return -1;
}

uint64_t wubu_trash_get_size(void) {
    return g_trash.current_size_bytes;
}

uint64_t wubu_trash_get_max_size(void) {
    return g_trash.max_size_bytes;
}

void wubu_trash_set_max_size(uint64_t max_bytes) {
    g_trash.max_size_bytes = max_bytes;
}

void wubu_trash_enable_auto_expire(int days) {
    g_trash.auto_expire_enabled = true;
    g_trash.auto_expire_days = days;
}

void wubu_trash_disable_auto_expire(void) {
    g_trash.auto_expire_enabled = false;
}

int wubu_trash_run_auto_expire(void) {
    if (!g_trash.auto_expire_enabled || g_trash.auto_expire_days <= 0) return 0;
    
    time_t now = time(NULL);
    time_t cutoff = now - (time_t)g_trash.auto_expire_days * 86400;
    
    int expired = 0;
    for (int i = g_trash.entry_count - 1; i >= 0; i--) {
        if (g_trash.entries[i].deletion_time < cutoff) {
            wubu_trash_delete(basename(g_trash.entries[i].deleted_path));
            expired++;
        }
    }
    return expired;
}

bool wubu_trash_is_in_trash(const char *path) {
    if (!path) return false;
    
    char abs_path[4096];
    if (!realpath(path, abs_path)) return false;
    
    return strncmp(abs_path, g_trash.trash_dir, strlen(g_trash.trash_dir)) == 0;
}

const char *wubu_trash_get_unique_name(const char *original_path, char *out_name, size_t out_size) {
    if (!original_path || !out_name) return NULL;
    
    char *path_copy = strdup(original_path);
    if (!path_copy) return NULL;
    
    char *base = basename(path_copy);
    char *unique = get_unique_filename(g_trash.files_dir, base);
    free(path_copy);
    
    if (unique) {
        const char *result = basename(unique);
        strncpy(out_name, result, out_size - 1);
        out_name[out_size - 1] = '\0';
        free(unique);
        return out_name;
    }
    return NULL;
}