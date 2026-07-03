/*
 * dosgui_explorer_zip.c  --  WuBuOS Explorer Zip Archive Module
 *
 * Extracted from dosgui_explorer.c lines 667-979
 * Self-contained ZIP central directory parsing + libzip dlopen wrapper
 */

#include "dosgui_explorer_internal.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>

/* -- Zip Archives ------------------------------------------------- */

/* ZIP local file header signature */
#define ZIP_LOCAL_FILE_HEADER_SIG   0x04034b50
#define ZIP_CENTRAL_DIR_HEADER_SIG  0x02014b50
#define ZIP_END_CENTRAL_DIR_SIG     0x06054b50

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression_method;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_field_len;
} ZipLocalHeader;

typedef struct {
    uint32_t signature;
    uint16_t version_made_by;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression_method;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_field_len;
    uint16_t comment_len;
    uint16_t disk_num_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_header_offset;
} ZipCentralDirHeader;

typedef struct {
    uint32_t signature;
    uint16_t disk_number;
    uint16_t central_dir_disk;
    uint16_t central_dir_entries_this_disk;
    uint16_t central_dir_entries_total;
    uint32_t central_dir_size;
    uint32_t central_dir_offset;
    uint16_t comment_len;
} ZipEndCentralDir;
#pragma pack(pop)

/* Libzip function pointers (dlopen) */
typedef struct {
    void *handle;
    void *(*zip_open)(const char *, int, int *);
    void (*zip_close)(void *);
    void *(*zip_fopen_index)(void *, uint64_t, int);
    void (*zip_fclose)(void *);
    int64_t (*zip_fread)(void *, void *, uint64_t);
    int (*zip_stat_index)(void *, uint64_t, int, void *);
    int64_t (*zip_get_num_entries)(void *, int);
    const char *(*zip_get_name)(void *, uint64_t, int);
} LibzipFunctions;

static LibzipFunctions g_libzip = {0};

static bool ex_libzip_load(void) {
    if (g_libzip.handle) return true;
    g_libzip.handle = dlopen("libzip.so.4", RTLD_LAZY);
    if (!g_libzip.handle) return false;
    g_libzip.zip_open = dlsym(g_libzip.handle, "zip_open");
    g_libzip.zip_close = dlsym(g_libzip.handle, "zip_close");
    g_libzip.zip_fopen_index = dlsym(g_libzip.handle, "zip_fopen_index");
    g_libzip.zip_fclose = dlsym(g_libzip.handle, "zip_fclose");
    g_libzip.zip_fread = dlsym(g_libzip.handle, "zip_fread");
    g_libzip.zip_stat_index = dlsym(g_libzip.handle, "zip_stat_index");
    g_libzip.zip_get_num_entries = dlsym(g_libzip.handle, "zip_get_num_entries");
    g_libzip.zip_get_name = dlsym(g_libzip.handle, "zip_get_name");
    return g_libzip.zip_open != NULL;
}

static void ex_libzip_unload(void) {
    if (g_libzip.handle) {
        dlclose(g_libzip.handle);
        memset(&g_libzip, 0, sizeof(g_libzip));
    }
}

/* Zip entry cache for mounted archive */
typedef struct {
    char name[256];
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    uint32_t crc32;
    uint16_t compression_method;
    time_t modified;
    int central_dir_index;
    bool is_directory;
} ExZipEntry;

static ExZipEntry g_zip_entries[EX_MAX_ZIP_ENTRIES];
static int g_zip_entry_count = 0;
static bool g_zip_entries_valid = false;

/* Read zip central directory using raw file I/O (no libzip headers needed) */
static int ex_zip_read_central_directory(const char *zip_path) {
    if (!zip_path) return -1;
    
    int fd = open(zip_path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    
    /* Read last 64KB to find End of Central Directory record */
    size_t search_size = st.st_size < 65536 ? st.st_size : 65536;
    uint8_t *buf = malloc(search_size);
    if (!buf) {
        close(fd);
        return -1;
    }
    
    off_t read_offset = st.st_size - search_size;
    if (lseek(fd, read_offset, SEEK_SET) < 0) {
        free(buf);
        close(fd);
        return -1;
    }
    
    if (read(fd, buf, search_size) != (ssize_t)search_size) {
        free(buf);
        close(fd);
        return -1;
    }
    
    /* Search for End of Central Directory signature */
    ZipEndCentralDir *ecdr = NULL;
    for (size_t i = 0; i + sizeof(ZipEndCentralDir) <= search_size; i++) {
        if (*(uint32_t*)(buf + i) == ZIP_END_CENTRAL_DIR_SIG) {
            ecdr = (ZipEndCentralDir*)(buf + i);
            break;
        }
    }
    
    if (!ecdr) {
        free(buf);
        close(fd);
        return -1;
    }
    
    /* Now read central directory entries */
    off_t central_dir_offset = ecdr->central_dir_offset;
    uint16_t entry_count = ecdr->central_dir_entries_total;
    
    if (lseek(fd, central_dir_offset, SEEK_SET) < 0) {
        free(buf);
        close(fd);
        return -1;
    }
    
    g_zip_entry_count = 0;
    
    for (int i = 0; i < entry_count && g_zip_entry_count < EX_MAX_ZIP_ENTRIES; i++) {
        ZipCentralDirHeader cdh;
        if (read(fd, &cdh, sizeof(cdh)) != sizeof(cdh)) break;
        
        if (cdh.signature != ZIP_CENTRAL_DIR_HEADER_SIG) break;
        
        /* Read filename */
        char filename[256];
        if (cdh.filename_len >= sizeof(filename)) cdh.filename_len = sizeof(filename) - 1;
        if (read(fd, filename, cdh.filename_len) != cdh.filename_len) break;
        filename[cdh.filename_len] = '\0';
        
        /* Skip extra field and comment */
        lseek(fd, cdh.extra_field_len + cdh.comment_len, SEEK_CUR);
        
        /* Store entry */
        ExZipEntry *ze = &g_zip_entries[g_zip_entry_count];
        WUBU_STRCPY(ze->name, filename, sizeof(ze->name));
        ze->uncompressed_size = cdh.uncompressed_size;
        ze->compressed_size = cdh.compressed_size;
        ze->crc32 = cdh.crc32;
        ze->compression_method = cdh.compression_method;
        ze->central_dir_index = i;
        ze->is_directory = (filename[cdh.filename_len - 1] == '/') || (cdh.uncompressed_size == 0 && cdh.compressed_size == 0);
        
        /* Convert DOS time to time_t */
        struct tm tm = {0};
        tm.tm_year = ((cdh.last_mod_date >> 9) & 0x7F) + 80;  /* Years since 1900 */
        tm.tm_mon = ((cdh.last_mod_date >> 5) & 0xF) - 1;      /* 0-11 */
        tm.tm_mday = cdh.last_mod_date & 0x1F;                  /* 1-31 */
        tm.tm_hour = (cdh.last_mod_time >> 11) & 0x1F;
        tm.tm_min = (cdh.last_mod_time >> 5) & 0x3F;
        tm.tm_sec = (cdh.last_mod_time & 0x1F) * 2;
        ze->modified = mktime(&tm);
        
        g_zip_entry_count++;
    }
    
    g_zip_entries_valid = (g_zip_entry_count > 0);
    
    free(buf);
    close(fd);
    return g_zip_entry_count;
}

/* Populate explorer entries from zip cache */
static void ex_zip_populate_entries(void) {
    g_explorer.entry_count = 0;
    
    for (int i = 0; i < g_zip_entry_count && g_explorer.entry_count < EX_MAX_ENTRIES; i++) {
        ExZipEntry *ze = &g_zip_entries[i];
        
        ExEntry *entry = &g_explorer.entries[g_explorer.entry_count];
        WUBU_STRCPY(entry->name, ze->name, sizeof(entry->name));
        WUBU_SNPRINTF(entry->full_path, sizeof(entry->full_path), "%s/%s", g_explorer.current_zip_path, ze->name);
        
        if (ze->is_directory) {
            entry->type = EX_ENTRY_DIR;
        } else {
            entry->type = EX_ENTRY_FILE;
        }
        entry->size = ze->uncompressed_size;
        entry->modified = ze->modified;
        entry->created = ze->modified;
        entry->hidden = false;
        entry->readonly = true;  /* Zip entries are read-only in mount */
        entry->is_selected = false;
        
        /* Extract extension */
        const char *dot = strrchr(ze->name, '.');
        if (dot && !ze->is_directory) {
            WUBU_STRCPY(entry->extension, dot + 1, sizeof(entry->extension));
            for (char *p = entry->extension; *p; p++) *p = tolower(*p);
        } else {
            entry->extension[0] = '\0';
        }
        
        /* Store zip info */
        entry->zip_info.zip_index = ze->central_dir_index;
        WUBU_STRCPY(entry->zip_info.zip_path, g_explorer.current_zip_path, sizeof(entry->zip_info.zip_path));
        
        g_explorer.entry_count++;
    }
    
    /* Update breadcrumbs */
    ex_update_breadcrumbs(&g_explorer);
}

/* -- Public API --------------------------------------------------- */

bool dosgui_explorer_mount_zip(const char *zip_path) {
    if (!zip_path) return false;
    
    /* Try libzip first if available */
    if (ex_libzip_load()) {
        int err = 0;
        void *archive = g_libzip.zip_open(zip_path, 0, &err);
        if (archive) {
            /* Success with libzip */
            WUBU_STRCPY(g_explorer.current_zip_path, zip_path, sizeof(g_explorer.current_zip_path));
            g_explorer.in_zip_archive = true;
            
            /* Use libzip to populate entries */
            int64_t num_entries = g_libzip.zip_get_num_entries(archive, 0);
            g_zip_entry_count = 0;
            
            for (int64_t i = 0; i < num_entries && g_zip_entry_count < EX_MAX_ZIP_ENTRIES; i++) {
                const char *name = g_libzip.zip_get_name(archive, i, 0);
                if (!name) continue;
                
                ExZipEntry *ze = &g_zip_entries[g_zip_entry_count];
                WUBU_STRCPY(ze->name, name, sizeof(ze->name));
                ze->central_dir_index = (int)i;
                ze->is_directory = (name[strlen(name) - 1] == '/');
                g_zip_entry_count++;
            }
            
            g_libzip.zip_close(archive);
            g_zip_entries_valid = true;
            ex_zip_populate_entries();
            return true;
        }
    }
    
    /* Fallback: parse zip manually using zlib structures */
    int count = ex_zip_read_central_directory(zip_path);
    if (count > 0) {
        WUBU_STRCPY(g_explorer.current_zip_path, zip_path, sizeof(g_explorer.current_zip_path));
        g_explorer.in_zip_archive = true;
        ex_zip_populate_entries();
        return true;
    }
    
    return false;
}

void dosgui_explorer_unmount_zip(void) {
    g_explorer.in_zip_archive = false;
    g_explorer.current_zip_path[0] = '\0';
    g_zip_entries_valid = false;
    g_zip_entry_count = 0;
    dosgui_explorer_go_up();
}

bool dosgui_explorer_is_in_zip(void) {
    return g_explorer.in_zip_archive;
}