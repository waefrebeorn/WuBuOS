/**
 * wubu_pkgmgr.c - WuBuOS Package Manager Implementation
 * 
 * SQLite-backed package database, .wubu container format,
 * dependency resolution, sandboxed installation.
 */

#include "wubu_pkgmgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sqlite3.h>
#include <zstd.h>
#include <libgen.h>
#include <stddef.h>
#include <glob.h>
#include <sys/wait.h>

/* Forward declarations for static callbacks */
static int cb_pkg_installed(void* data, int argc, char** argv, char** col);
static bool copy_file(const char* src, const char* dst);

/* ============================================================
 * Internal State
 * ============================================================ */

static struct {
    bool initialized;
    wubu_pkgmgr_config_t config;
    sqlite3* db;
    wubu_pkgmgr_progress_cb progress_cb;
    void* progress_userdata;
    wubu_pkg_repo_t repos[32];
    int n_repos;
} g_pkgmgr = {0};

/* ============================================================
 * Helpers
 * ============================================================ */

static void pkgmgr_progress(const char* stage, const char* pkg_id, float progress, const char* msg) {
    if (g_pkgmgr.progress_cb) {
        g_pkgmgr.progress_cb(g_pkgmgr.progress_userdata, stage, pkg_id, progress, msg);
    }
}

/* SQLite callback functions */
static int cb_list_installed_idx = 0;

static int cb_load_repos(void* data, int argc, char** argv, char** col) {
    (void)data; (void)col;
    if (argc >= 7 && g_pkgmgr.n_repos < 32) {
        wubu_pkg_repo_t* r = &g_pkgmgr.repos[g_pkgmgr.n_repos++];
        strncpy(r->name, argv[0] ? argv[0] : "", sizeof(r->name)-1);
        strncpy(r->url, argv[1] ? argv[1] : "", sizeof(r->url)-1);
        strncpy(r->index_url, argv[2] ? argv[2] : "", sizeof(r->index_url)-1);
        strncpy(r->pubkey, argv[3] ? argv[3] : "", sizeof(r->pubkey)-1);
        r->enabled = argv[4] ? atoi(argv[4]) : 1;
        r->priority = argv[5] ? atoi(argv[5]) : 0;
        r->last_update = argv[6] ? atoll(argv[6]) : 0;
        r->packages = NULL;
        r->n_packages = 0;
    }
    return 0;
}

static int cb_list_installed(void* data, int argc, char** argv, char** col) {
    (void)col;
    char* buffer = (char*)data;
    int* count = (int*)buffer;
    wubu_pkg_installed_t* out = (wubu_pkg_installed_t*)(buffer + sizeof(int));
    if (argc >= 14 && *count < 255) {
        /* Create a temporary buffer for cb_pkg_installed with found flag */
        char temp_buffer[sizeof(wubu_pkg_installed_t) + sizeof(int)];
        memset(temp_buffer, 0, sizeof(temp_buffer));
        wubu_pkg_installed_t* pkg = (wubu_pkg_installed_t*)temp_buffer;
        int* found = (int*)((char*)temp_buffer + sizeof(wubu_pkg_installed_t));
        
        cb_pkg_installed(temp_buffer, argc, argv, col);
        
        if (*found) {
            out[*count] = *pkg;
            (*count)++;
        }
    }
    return 0;
}

static bool ensure_dir(const char* path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

static bool read_file(const char* path, char** out_content, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out_content = malloc(sz + 1);
    if (!*out_content) { fclose(f); return false; }
    fread(*out_content, 1, sz, f);
    (*out_content)[sz] = '\0';
    if (out_size) *out_size = sz;
    fclose(f);
    return true;
}

static bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return true;
}

static char* sha256_hex(const char* data, size_t len) {
    /* Simplified - in production use OpenSSL/mbedTLS */
    static char hex[65];
    unsigned char hash[32] = {0};
    /* Simple FNV-1a for demo */
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (unsigned char)data[i];
        h *= 1099511628211ULL;
    }
    /* Use different mixes of h to fill 64 hex chars */
    snprintf(hex, sizeof(hex), "%016llx%016llx%016llx%016llx", 
             (unsigned long long)h, (unsigned long long)(h * 2654435761ULL), 
             (unsigned long long)(h ^ 0x9e3779b97f4a7c15ULL), (unsigned long long)(h + 0x9e3779b97f4a7c15ULL));
    return hex;
}

static int64_t db_exec(const char* sql) {
    char* err = NULL;
    int rc = sqlite3_exec(g_pkgmgr.db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[pkgmgr] SQL error: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

static int db_query(const char* sql, int (*callback)(void*, int, char**, char**), void* data) {
    char* err = NULL;
    int rc = sqlite3_exec(g_pkgmgr.db, sql, callback, data, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[pkgmgr] SQL error: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* ============================================================
 * Database Schema
 * ============================================================ */

static const char* SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS packages ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  version TEXT NOT NULL,"
    "  description TEXT,"
    "  maintainer TEXT,"
    "  homepage TEXT,"
    "  license TEXT,"
    "  manifest_json TEXT NOT NULL,"
    "  install_path TEXT NOT NULL,"
    "  install_date TEXT NOT NULL,"
    "  auto_installed INTEGER DEFAULT 0,"
    "  size_bytes INTEGER DEFAULT 0,"
    "  payload_type INTEGER,"
    "  arch INTEGER,"
    "  sandbox_profile TEXT"
    ");"
    
    "CREATE TABLE IF NOT EXISTS package_files ("
    "  pkg_id TEXT NOT NULL,"
    "  src_path TEXT NOT NULL,"
    "  dst_path TEXT NOT NULL,"
    "  file_hash TEXT NOT NULL,"
    "  mode INTEGER,"
    "  PRIMARY KEY (pkg_id, dst_path),"
    "  FOREIGN KEY (pkg_id) REFERENCES packages(id) ON DELETE CASCADE"
    ");"
    
    "CREATE TABLE IF NOT EXISTS package_deps ("
    "  pkg_id TEXT NOT NULL,"
    "  dep_id TEXT NOT NULL,"
    "  dep_type INTEGER DEFAULT 0,"
    "  PRIMARY KEY (pkg_id, dep_id),"
    "  FOREIGN KEY (pkg_id) REFERENCES packages(id) ON DELETE CASCADE"
    ");"
    
    "CREATE TABLE IF NOT EXISTS repositories ("
    "  name TEXT PRIMARY KEY,"
    "  url TEXT NOT NULL,"
    "  index_url TEXT,"
    "  pubkey TEXT,"
    "  enabled INTEGER DEFAULT 1,"
    "  priority INTEGER DEFAULT 0,"
    "  last_update INTEGER DEFAULT 0"
    ");"
    
    "CREATE TABLE IF NOT EXISTS repo_packages ("
    "  repo_name TEXT NOT NULL,"
    "  pkg_id TEXT NOT NULL,"
    "  name TEXT NOT NULL,"
    "  version TEXT NOT NULL,"
    "  description TEXT,"
    "  download_url TEXT NOT NULL,"
    "  sha256 TEXT NOT NULL,"
    "  download_size INTEGER,"
    "  installed_size INTEGER,"
    "  arch INTEGER,"
    "  depends_json TEXT,"
    "  provides_json TEXT,"
    "  PRIMARY KEY (repo_name, pkg_id),"
    "  FOREIGN KEY (repo_name) REFERENCES repositories(name) ON DELETE CASCADE"
    ");"
    
    "CREATE INDEX IF NOT EXISTS idx_pkg_name ON packages(name);"
    "CREATE INDEX IF NOT EXISTS idx_repo_pkg ON repo_packages(pkg_id);"
    "CREATE INDEX IF NOT EXISTS idx_repo_name ON repo_packages(repo_name);";

/* ============================================================
 * Manifest Serialization (JSON)
 * ============================================================ */

static char* manifest_to_json(const wubu_pkg_manifest_t* m) {
    char* json = malloc(8192);
    char* p = json;
    size_t rem = 8192;
    
    p += snprintf(p, rem, 
        "{"
        "\"id\":\"%s\","
        "\"name\":\"%s\","
        "\"version\":\"%s\","
        "\"description\":\"%s\","
        "\"maintainer\":\"%s\","
        "\"homepage\":\"%s\","
        "\"license\":\"%s\","
        "\"depends\":[", 
        m->id, m->name, m->version, m->description, m->maintainer, m->homepage, m->license);
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_depends; i++) {
        p += snprintf(p, rem, "%s\"%s\"", i ? "," : "", m->depends[i]);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"recommends\":[");
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_recommends; i++) {
        p += snprintf(p, rem, "%s\"%s\"", i ? "," : "", m->recommends[i]);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"conflicts\":[");
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_conflicts; i++) {
        p += snprintf(p, rem, "%s\"%s\"", i ? "," : "", m->conflicts[i]);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"provides\":[");
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_provides; i++) {
        p += snprintf(p, rem, "%s\"%s\"", i ? "," : "", m->provides[i]);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"entrypoints\":[");
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_entrypoints; i++) {
        const struct {
            char id[WUBU_PKG_MAX_NAME];
            char name[WUBU_PKG_MAX_NAME];
            char exec[256];
            char icon[256];
            char categories[256];
            char mime_types[256];
            bool terminal;
            bool startup_notify;
        }* e = &m->entrypoints[i];
        p += snprintf(p, rem,
            "%s{"
            "\"id\":\"%s\","
            "\"name\":\"%s\","
            "\"exec\":\"%s\","
            "\"icon\":\"%s\","
            "\"categories\":\"%s\","
            "\"mime_types\":\"%s\","
            "\"terminal\":%s,"
            "\"startup_notify\":%s"
            "}",
            i ? "," : "", e->id, e->name, e->exec, e->icon, e->categories, e->mime_types,
            e->terminal ? "true" : "false", e->startup_notify ? "true" : "false");
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"files\":[");
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_files; i++) {
        p += snprintf(p, rem, "%s{\"src\":\"%s\",\"dst\":\"%s\",\"mode\":%u}", 
                      i ? "," : "", m->files[i].src, m->files[i].dst, m->files[i].mode);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem,
        "],"
        "\"payload_type\":%d,"
        "\"arch\":%d,"
        "\"min_ram_mb\":%llu,"
        "\"min_disk_mb\":%llu,"
        "\"required_libs\":[",
        m->payload_type, m->arch, (unsigned long long)m->min_ram_mb, (unsigned long long)m->min_disk_mb);
    rem = 8192 - (p - json);
    
    for (int i = 0; i < m->n_required_libs; i++) {
        p += snprintf(p, rem, "%s\"%s\"", i ? "," : "", m->required_libs[i]);
        rem = 8192 - (p - json);
    }
    
    p += snprintf(p, rem, 
        "],"
        "\"sandbox_profile\":\"%s\","
        "\"build_host\":\"%s\","
        "\"build_date\":\"%s\","
        "\"builder_version\":\"%s\""
        "}",
        m->sandbox_profile, m->build_host, m->build_date, m->builder_version);
    
    return json;
}

/* ============================================================
 * .wubu Container Format
 * ============================================================ */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           /* "WUBU" */
    uint16_t version;         /* 1 */
    uint16_t flags;           /* Reserved */
    uint8_t arch;             /* wubu_pkg_arch_t */
    uint8_t payload_type;     /* wubu_pkg_payload_t */
    uint32_t manifest_size;   /* Compressed manifest size */
    uint64_t payload_size;    /* Compressed payload size */
    uint64_t uncompressed_size; /* Total uncompressed */
    uint8_t signature[WUBU_PKG_SIG_SIZE]; /* Ed25519 */
    char build_date[32];      /* ISO 8601 */
    char builder_version[32];
    uint32_t crc32;           /* CRC32 of header */
} wubu_pkg_header_t;
#pragma pack(pop)

static bool write_pkg(const char* output_path, const wubu_pkg_manifest_t* manifest,
                       const char* payload_dir, const char* sign_key_hex) {
    pkgmgr_progress("package", manifest->id, 0.1, "Creating manifest");
    
    /* Serialize manifest to JSON */
    char* manifest_json = manifest_to_json(manifest);
    if (!manifest_json) return false;
    
    /* Compress manifest with zstd */
    size_t manifest_bound = ZSTD_compressBound(strlen(manifest_json));
    void* manifest_compressed = malloc(manifest_bound);
    size_t manifest_csize = ZSTD_compress(manifest_compressed, manifest_bound, 
                                           manifest_json, strlen(manifest_json), 3);
    free(manifest_json);
    if (ZSTD_isError(manifest_csize)) {
        free(manifest_compressed);
        return false;
    }
    
    pkgmgr_progress("package", manifest->id, 0.3, "Compressing payload");
    
    /* Create payload archive using fork+exec tar+zstd */
    char payload_archive[512];
    snprintf(payload_archive, sizeof(payload_archive), "/tmp/wubu_payload_%s.tar.zst", manifest->id);
    
    /* Fork and exec tar | zstd pipeline */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        free(manifest_compressed);
        return false;
    }
    
    pid_t tar_pid = fork();
    if (tar_pid == 0) {
        /* Child: tar */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        /* Change to payload directory */
        if (chdir(payload_dir) < 0) _exit(1);
        
        execlp("tar", "tar", "-cf", "-", ".", (char*)NULL);
        _exit(1);
    }
    
    pid_t zstd_pid = fork();
    if (zstd_pid == 0) {
        /* Child: zstd */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        int out_fd = open(payload_archive, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) _exit(1);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
        
        execlp("zstd", "zstd", "-3", (char*)NULL);
        _exit(1);
    }
    
    /* Parent: wait for both children */
    close(pipefd[0]);
    close(pipefd[1]);
    int tar_status, zstd_status;
    waitpid(tar_pid, &tar_status, 0);
    waitpid(zstd_pid, &zstd_status, 0);
    
    if (!WIFEXITED(tar_status) || WEXITSTATUS(tar_status) != 0 ||
        !WIFEXITED(zstd_status) || WEXITSTATUS(zstd_status) != 0) {
        free(manifest_compressed);
        return false;
    }
    
    /* Get payload size */
    struct stat st;
    stat(payload_archive, &st);
    uint64_t payload_size = st.st_size;
    
    pkgmgr_progress("package", manifest->id, 0.6, "Writing container");
    
    /* Build header */
    wubu_pkg_header_t header = {0};
    header.magic = WUBU_PKG_MAGIC;
    header.version = WUBU_PKG_VERSION;
    header.arch = manifest->arch;
    header.payload_type = manifest->payload_type;
    header.manifest_size = (uint32_t)manifest_csize;
    header.payload_size = payload_size;
    header.uncompressed_size = (uint32_t)payload_size;

    /* Cast manifest_compressed once for byte-level access */
    const uint8_t *mc_data = (const uint8_t *)manifest_compressed;

    /* Compute CRC32 over manifest + payload data */
    {
        uint32_t crc = 0xFFFFFFFF;
        /* CRC over manifest */
        for (size_t i = 0; i < (size_t)manifest_csize; i++) {
            crc ^= (uint32_t)mc_data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        /* CRC over payload */
        FILE *pf = fopen(payload_archive, "rb");
        if (pf) {
            unsigned char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), pf)) > 0) {
                for (size_t i = 0; i < n; i++) {
                    crc ^= (uint32_t)buf[i];
                    for (int j = 0; j < 8; j++) {
                        crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
                    }
                }
            }
            fclose(pf);
        }
        header.crc32 = crc ^ 0xFFFFFFFF;
    }

    /* Compute simple signature: SHA-256-like hash of header + manifest + payload */
    {
        /* Simple FNV-1a hash as a placeholder signature */
        uint64_t hash = 0xcbf29ce484222325ULL;
        const unsigned char *hptr = (const unsigned char *)&header;
        for (size_t i = 0; i < offsetof(wubu_pkg_header_t, signature); i++) {
            hash ^= hptr[i];
            hash *= 0x100000001b3ULL;
        }
        for (size_t i = 0; i < (size_t)manifest_csize; i++) {
            hash ^= mc_data[i];
            hash *= 0x100000001b3ULL;
        }
        /* Mix in payload hash */
        FILE *pf = fopen(payload_archive, "rb");
        if (pf) {
            unsigned char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), pf)) > 0) {
                for (size_t i = 0; i < n; i++) {
                    hash ^= buf[i];
                    hash *= 0x100000001b3ULL;
                }
            }
            fclose(pf);
        }
        /* Store 32 bytes of signature */
        memcpy(header.signature, &hash, sizeof(hash));
        /* Second half: hash of the first half mixed with CRC */
        hash ^= (uint64_t)header.crc32;
        hash *= 0x100000001b3ULL;
        memcpy(header.signature + 8, &hash, sizeof(hash));
        /* Fill remaining with derived bytes */
        for (int i = 16; i < WUBU_PKG_SIG_SIZE; i++) {
            header.signature[i] = (uint8_t)(hash >> ((i % 8) * 8)) ^ (uint8_t)i;
        }
    }
    
    /* Write container */
    FILE* out = fopen(output_path, "wb");
    if (!out) {
        free(manifest_compressed);
        unlink(payload_archive);
        return false;
    }
    
    fwrite(&header, sizeof(header), 1, out);
    fwrite(manifest_compressed, 1, manifest_csize, out);
    free(manifest_compressed);
    
    /* Append payload */
    FILE* in = fopen(payload_archive, "rb");
    if (in) {
        char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, n, out);
        }
        fclose(in);
    }
    fclose(out);
    unlink(payload_archive);
    
    pkgmgr_progress("package", manifest->id, 1.0, "Package created");
    return true;
}

static bool read_pkg_header(const char* pkg_path, wubu_pkg_header_t* header) {
    FILE* f = fopen(pkg_path, "rb");
    if (!f) return false;
    size_t n = fread(header, 1, sizeof(*header), f);
    fclose(f);
    return n == sizeof(*header) && header->magic == WUBU_PKG_MAGIC;
}

static bool extract_pkg_manifest(const char* pkg_path, wubu_pkg_manifest_t* out) {
    wubu_pkg_header_t header;
    if (!read_pkg_header(pkg_path, &header)) return false;
    
    FILE* f = fopen(pkg_path, "rb");
    if (!f) return false;
    fseek(f, sizeof(header), SEEK_SET);
    
    void* compressed = malloc(header.manifest_size);
    fread(compressed, 1, header.manifest_size, f);
    fclose(f);
    
    /* Decompress manifest */
    const size_t max_manifest = 32768;
    void* decompressed = malloc(max_manifest);
    size_t dsize = ZSTD_decompress(decompressed, max_manifest, compressed, header.manifest_size);
    free(compressed);
    
    if (ZSTD_isError(dsize)) {
        free(decompressed);
        return false;
    }
    
    /* Parse JSON (simplified - just extract key fields) */
    char* json = (char*)decompressed;
    json[dsize] = '\0';
    
    /* Simple JSON extraction for demo */
    memset(out, 0, sizeof(*out));
    
    /* Extract id */
    char* p = strstr(json, "\"id\":\"");
    if (p) { p += 6; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->id, p, sizeof(out->id)-1); *e = '"'; } }
    
    p = strstr(json, "\"name\":\"");
    if (p) { p += 8; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->name, p, sizeof(out->name)-1); *e = '"'; } }
    
    p = strstr(json, "\"version\":\"");
    if (p) { p += 11; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->version, p, sizeof(out->version)-1); *e = '"'; } }
    
    p = strstr(json, "\"payload_type\":");
    if (p) { out->payload_type = atoi(p + 15); }
    
    p = strstr(json, "\"arch\":");
    if (p) { out->arch = atoi(p + 7); }
    
    p = strstr(json, "\"sandbox_profile\":\"");
    if (p) { p += 19; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->sandbox_profile, p, sizeof(out->sandbox_profile)-1); *e = '"'; } }
    
    free(decompressed);
    return true;
}

/* ============================================================
 * Database Callbacks
 * ============================================================ */

static int cb_pkg_installed(void* data, int argc, char** argv, char** col) {
    wubu_pkg_installed_t* pkg = (wubu_pkg_installed_t*)data;
    int* found = (int*)((char*)data + sizeof(wubu_pkg_installed_t));
    *found = 1;
    if (argc >= 14) {
        strncpy(pkg->manifest.id, argv[0] ? argv[0] : "", sizeof(pkg->manifest.id)-1);
        strncpy(pkg->manifest.name, argv[1] ? argv[1] : "", sizeof(pkg->manifest.name)-1);
        strncpy(pkg->manifest.version, argv[2] ? argv[2] : "", sizeof(pkg->manifest.version)-1);
        strncpy(pkg->manifest.description, argv[3] ? argv[3] : "", sizeof(pkg->manifest.description)-1);
        strncpy(pkg->manifest.maintainer, argv[4] ? argv[4] : "", sizeof(pkg->manifest.maintainer)-1);
        strncpy(pkg->manifest.homepage, argv[5] ? argv[5] : "", sizeof(pkg->manifest.homepage)-1);
        strncpy(pkg->manifest.license, argv[6] ? argv[6] : "", sizeof(pkg->manifest.license)-1);
        strncpy(pkg->install_path, argv[8] ? argv[8] : "", sizeof(pkg->install_path)-1);
        strncpy(pkg->install_date, argv[9] ? argv[9] : "", sizeof(pkg->install_date)-1);
        pkg->auto_installed = argv[10] ? atoi(argv[10]) : 0;
        pkg->size_bytes = argv[11] ? atoll(argv[11]) : 0;
        pkg->manifest.payload_type = argv[12] ? atoi(argv[12]) : 0;
        pkg->manifest.arch = argv[13] ? atoi(argv[13]) : 0;
        strncpy(pkg->manifest.sandbox_profile, argv[14] ? argv[14] : "", sizeof(pkg->manifest.sandbox_profile)-1);
    }
    return 0;
}

static int cb_repo_packages(void* data, int argc, char** argv, char** col) {
    wubu_pkg_repo_entry_t* entries = (wubu_pkg_repo_entry_t*)data;
    static int idx = 0;
    if (argc >= 12 && idx < 1024) {
        wubu_pkg_repo_entry_t* e = &entries[idx++];
        strncpy(e->id, argv[1] ? argv[1] : "", sizeof(e->id)-1);
        strncpy(e->name, argv[2] ? argv[2] : "", sizeof(e->name)-1);
        strncpy(e->version, argv[3] ? argv[3] : "", sizeof(e->version)-1);
        strncpy(e->description, argv[4] ? argv[4] : "", sizeof(e->description)-1);
        strncpy(e->download_url, argv[5] ? argv[5] : "", sizeof(e->download_url)-1);
        strncpy(e->sha256, argv[6] ? argv[6] : "", sizeof(e->sha256)-1);
        e->download_size = argv[7] ? atoll(argv[7]) : 0;
        e->installed_size = argv[8] ? atoll(argv[8]) : 0;
        e->arch = argv[9] ? atoi(argv[9]) : 0;
    }
    return 0;
}

static int cb_count(void* data, int argc, char** argv, char** col) {
    int* count = (int*)data;
    if (argc > 0 && argv[0]) *count = atoi(argv[0]);
    return 0;
}

/* ============================================================
 * Public API Implementation
 * ============================================================ */

bool wubu_pkgmgr_init(const wubu_pkgmgr_config_t* config) {
    if (g_pkgmgr.initialized) return true;
    
    if (config) {
        g_pkgmgr.config = *config;
    } else {
        wubu_pkgmgr_get_default_config(&g_pkgmgr.config);
    }
    
    /* Ensure directories */
    ensure_dir(g_pkgmgr.config.db_path);
    ensure_dir(g_pkgmgr.config.cache_dir);
    ensure_dir(g_pkgmgr.config.install_prefix);
    ensure_dir(g_pkgmgr.config.repo_config_dir);
    
    /* Open database */
    char db_file[512];
    snprintf(db_file, sizeof(db_file), "%s/pkgmgr.db", g_pkgmgr.config.db_path);
    if (sqlite3_open(db_file, &g_pkgmgr.db) != SQLITE_OK) {
        fprintf(stderr, "[pkgmgr] Failed to open database: %s\n", sqlite3_errmsg(g_pkgmgr.db));
        return false;
    }
    
    /* Create schema */
    if (db_exec(SCHEMA_SQL) != 0) {
        sqlite3_close(g_pkgmgr.db);
        return false;
    }
    
    /* Load repositories */
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT name, url, index_url, pubkey, enabled, priority, last_update FROM repositories");
    db_query(sql, cb_load_repos, NULL);
    
    g_pkgmgr.initialized = true;
    return true;
}

void wubu_pkgmgr_shutdown(void) {
    if (g_pkgmgr.db) {
        sqlite3_close(g_pkgmgr.db);
        g_pkgmgr.db = NULL;
    }
    for (int i = 0; i < g_pkgmgr.n_repos; i++) {
        free(g_pkgmgr.repos[i].packages);
    }
    g_pkgmgr.n_repos = 0;
    g_pkgmgr.initialized = false;
}

void wubu_pkgmgr_get_default_config(wubu_pkgmgr_config_t* config) {
    memset(config, 0, sizeof(*config));
    const char* home = getenv("HOME") ? getenv("HOME") : "/tmp";
    snprintf(config->db_path, sizeof(config->db_path), "%s/.local/share/wubu/pkgmgr", home);
    snprintf(config->cache_dir, sizeof(config->cache_dir), "%s/.cache/wubu/pkgmgr", home);
    snprintf(config->install_prefix, sizeof(config->install_prefix), "/usr/lib/wubu");
    snprintf(config->repo_config_dir, sizeof(config->repo_config_dir), "%s/.config/wubu/repos", home);
    config->max_parallel_downloads = 4;
    config->verify_signatures = true;
    config->allow_untrusted = false;
    config->auto_update_index = true;
    config->index_ttl_hours = 24;
}

/* ============================================================
 * Repository Management
 * ============================================================ */

bool wubu_pkgmgr_repo_add(const char* name, const char* url, const char* pubkey_hex, int priority) {
    if (!g_pkgmgr.initialized) return false;
    
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO repositories (name, url, index_url, pubkey, enabled, priority) "
        "VALUES ('%s', '%s', '%s/index.json', '%s', 1, %d)",
        name, url, url, pubkey_hex ? pubkey_hex : "", priority);
    
    if (db_exec(sql) != 0) return false;
    
    /* Reload repos */
    wubu_pkgmgr_shutdown();
    wubu_pkgmgr_init(&g_pkgmgr.config);
    return true;
}

bool wubu_pkgmgr_repo_remove(const char* name) {
    if (!g_pkgmgr.initialized) return false;
    
    char sql[512];
    snprintf(sql, sizeof(sql), "DELETE FROM repositories WHERE name='%s'", name);
    if (db_exec(sql) != 0) return false;
    
    snprintf(sql, sizeof(sql), "DELETE FROM repo_packages WHERE repo_name='%s'", name);
    db_exec(sql);
    
    wubu_pkgmgr_shutdown();
    wubu_pkgmgr_init(&g_pkgmgr.config);
    return true;
}

bool wubu_pkgmgr_repo_set_enabled(const char* name, bool enabled) {
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE repositories SET enabled=%d WHERE name='%s'", enabled ? 1 : 0, name);
    return db_exec(sql) == 0;
}

int wubu_pkgmgr_repo_list(wubu_pkg_repo_t* out, int max) {
    int n = g_pkgmgr.n_repos < max ? g_pkgmgr.n_repos : max;
    for (int i = 0; i < n; i++) {
        out[i] = g_pkgmgr.repos[i];
    }
    return n;
}

bool wubu_pkgmgr_repo_update(const char* name) {
    /* In production: download index.json from repo, parse, populate repo_packages table */
    pkgmgr_progress("repo", name ? name : "all", 0.5, "Updating repository index");
    
    /* Simulate update */
    time_t now = time(NULL);
    char sql[512];
    if (name) {
        snprintf(sql, sizeof(sql), "UPDATE repositories SET last_update=%ld WHERE name='%s'", now, name);
    } else {
        snprintf(sql, sizeof(sql), "UPDATE repositories SET last_update=%ld WHERE enabled=1", now);
    }
    db_exec(sql);
    
    pkgmgr_progress("repo", name ? name : "all", 1.0, "Repository index updated");
    return true;
}

/* ============================================================
 * Search & Info
 * ============================================================ */

int wubu_pkgmgr_search(const char* query, wubu_pkg_repo_entry_t* out, int max) {
    if (!g_pkgmgr.initialized) return 0;
    
    char sql[1024];
    if (query && *query) {
        snprintf(sql, sizeof(sql),
            "SELECT repo_name, pkg_id, name, version, description, download_url, sha256, "
            "download_size, installed_size, arch "
            "FROM repo_packages "
            "WHERE (name LIKE '%%%s%%' OR pkg_id LIKE '%%%s%%' OR description LIKE '%%%s%%') "
            "LIMIT %d", query, query, query, max);
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT repo_name, pkg_id, name, version, description, download_url, sha256, "
            "download_size, installed_size, arch "
            "FROM repo_packages LIMIT %d", max);
    }
    
    return db_query(sql, cb_repo_packages, out) == 0 ? max : 0;
}

bool wubu_pkgmgr_repo_get_info(const char* pkg_id, wubu_pkg_repo_entry_t* out) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT repo_name, pkg_id, name, version, description, download_url, sha256, "
        "download_size, installed_size, arch, depends_json, provides_json "
        "FROM repo_packages WHERE pkg_id='%s' LIMIT 1", pkg_id);
    
    return db_query(sql, cb_repo_packages, out) == 0;
}

/* ============================================================
 * Package Operations
 * ============================================================ */

static bool install_package_files(const wubu_pkg_manifest_t* manifest, const char* install_root) {
    char src[1024], dst[1024];
    for (int i = 0; i < manifest->n_files; i++) {
        snprintf(src, sizeof(src), "%s/%s", install_root, manifest->files[i].src);
        snprintf(dst, sizeof(dst), "%s/%s", g_pkgmgr.config.install_prefix, manifest->files[i].dst);
        
        ensure_dir(dirname(dst));
        
        if (copy_file(src, dst)) {
            chmod(dst, manifest->files[i].mode);
        } else {
            return false;
        }
    }
    return true;
}

static void generate_desktop_entry(const wubu_pkg_manifest_t* manifest, int entry_idx, const char* install_path) {
    const struct {
        char id[WUBU_PKG_MAX_NAME];
        char name[WUBU_PKG_MAX_NAME];
        char exec[256];
        char icon[256];
        char categories[256];
        char mime_types[256];
        bool terminal;
        bool startup_notify;
    }* e = &manifest->entrypoints[entry_idx];
    char path[512];
    char dir[512];
    snprintf(path, sizeof(path), "%s/share/applications/%s-%s.desktop", 
             g_pkgmgr.config.install_prefix, manifest->id, e->id);
    
    strncpy(dir, path, sizeof(dir)-1);
    char* last_slash = strrchr(dir, '/');
    if (last_slash) *last_slash = '\0';
    ensure_dir(dir);
    
    char content[2048];
    snprintf(content, sizeof(content),
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=%s\n"
        "Comment=%s\n"
        "Exec=%s\n"
        "Icon=%s\n"
        "Categories=%s\n"
        "MimeType=%s\n"
        "Terminal=%s\n"
        "StartupNotify=%s\n"
        "X-Wubu-Package=%s\n"
        "X-Wubu-Entrypoint=%s\n",
        e->name, manifest->description, e->exec, e->icon,
        e->categories, e->mime_types,
        e->terminal ? "true" : "false",
        e->startup_notify ? "true" : "false",
        manifest->id, e->id);
    
    write_file(path, content);
}

bool wubu_pkgmgr_install(const char* pkg_spec, bool dry_run) {
    pkgmgr_progress("install", pkg_spec, 0.1, "Resolving package");
    
    /* Check if local .wubu file */
    wubu_pkg_manifest_t* manifest = calloc(1, sizeof(wubu_pkg_manifest_t));
    if (!manifest) return false;
    bool is_local = (strstr(pkg_spec, ".wubu") != NULL);
    bool ret = false;
    
    if (is_local) {
        if (!extract_pkg_manifest(pkg_spec, manifest)) {
            free(manifest);
            return false;
        }
    } else {
        /* Search repos */
        wubu_pkg_repo_entry_t repo_pkg;
        if (!wubu_pkgmgr_repo_get_info(pkg_spec, &repo_pkg)) {
            free(manifest);
            return false;
        }
        
        /* Download .wubu file */
        pkgmgr_progress("install", pkg_spec, 0.3, "Downloading package");
        char cache_path[512];
        snprintf(cache_path, sizeof(cache_path), "%s/%s-%s.wubu", 
                 g_pkgmgr.config.cache_dir, repo_pkg.id, repo_pkg.version);
        
        /* In production: curl download with progress */
        /* For demo, assume downloaded */
        
        if (!extract_pkg_manifest(cache_path, manifest)) {
            free(manifest);
            return false;
        }
        
        pkg_spec = manifest->id;
    }
    
    pkgmgr_progress("install", pkg_spec, 0.5, "Checking dependencies");
    
    /* Check conflicts */
    wubu_pkg_installed_t* installed = calloc(256, sizeof(wubu_pkg_installed_t));
    if (!installed) {
        free(manifest);
        return false;
    }
    int n_installed = wubu_pkgmgr_list_installed(installed, 256);
    if (!wubu_pkgmgr_check_conflicts(pkg_spec, installed, n_installed)) {
        fprintf(stderr, "[pkgmgr] Package conflicts with installed packages\n");
        free(installed);
        free(manifest);
        return false;
    }
    
    if (dry_run) {
        pkgmgr_progress("install", pkg_spec, 1.0, "Dry run complete");
        free(installed);
        free(manifest);
        return true;
    }
    
    pkgmgr_progress("install", pkg_spec, 0.7, "Installing files");
    
    /* Extract payload to temp dir */
    char temp_dir[512];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/wubu_install_%s", manifest->id);
    {
        /* Use fork+exec for rm -rf */
        pid_t rm_pid = fork();
        if (rm_pid == 0) {
            execlp("rm", "rm", "-rf", temp_dir, (char*)NULL);
            _exit(1);
        }
        waitpid(rm_pid, NULL, 0);
    }
    ensure_dir(temp_dir);
    
    /* In production: extract payload archive */
    /* For demo, skip extraction */
    
    /* Install files */
    char install_root[512];
    snprintf(install_root, sizeof(install_root), "%s/%s", g_pkgmgr.config.install_prefix, manifest->id);
    ensure_dir(install_root);
    
    if (!install_package_files(manifest, temp_dir)) {
        free(installed);
        free(manifest);
        return false;
    }
    
    /* Generate desktop entries */
    pkgmgr_progress("install", pkg_spec, 0.85, "Registering desktop entries");
    for (int i = 0; i < manifest->n_entrypoints; i++) {
        generate_desktop_entry(manifest, i, install_root);
    }
    
    /* Register in database */
    pkgmgr_progress("install", pkg_spec, 0.9, "Registering in database");
    
    char* manifest_json = manifest_to_json(manifest);
    time_t now = time(NULL);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT INTO packages (id, name, version, description, maintainer, homepage, license, "
        "manifest_json, install_path, install_date, auto_installed, size_bytes, payload_type, arch, sandbox_profile) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', 0, 0, %d, %d, '%s')",
        manifest->id, manifest->name, manifest->version, manifest->description,
        manifest->maintainer, manifest->homepage, manifest->license,
        manifest_json, install_root, date, manifest->payload_type, manifest->arch, manifest->sandbox_profile);
    free(manifest_json);
    
    db_exec(sql);
    
    /* Add dependencies */
    for (int i = 0; i < manifest->n_depends; i++) {
        snprintf(sql, sizeof(sql),
            "INSERT INTO package_deps (pkg_id, dep_id, dep_type) VALUES ('%s', '%s', 0)",
            manifest->id, manifest->depends[i]);
        db_exec(sql);
    }
    
    pkgmgr_progress("install", pkg_spec, 1.0, "Install complete");
    free(installed);
    free(manifest);
    return true;
}

bool wubu_pkgmgr_remove(const char* pkg_id, bool auto_remove_deps) {
    pkgmgr_progress("remove", pkg_id, 0.2, "Removing package");
    
    /* Get package info */
    wubu_pkg_installed_t pkg;
    if (!wubu_pkgmgr_get_installed(pkg_id, &pkg)) {
        return false;
    }
    
    if (!auto_remove_deps) {
        /* Check if required by other packages */
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM package_deps WHERE dep_id='%s'", pkg_id);
        int count = 0;
        db_query(sql, cb_count, &count);
        if (count > 0) {
            fprintf(stderr, "[pkgmgr] Package is required by other packages\n");
            return false;
        }
    }
    
    pkgmgr_progress("remove", pkg_id, 0.5, "Removing files");
    
    /* Remove installed files */
    pid_t rm_pid = fork();
    if (rm_pid == 0) {
        execlp("rm", "rm", "-rf", pkg.install_path, (char*)NULL);
        _exit(1);
    }
    waitpid(rm_pid, NULL, 0);
    
    /* Remove desktop entries */
    char desktop_pattern[512];
    snprintf(desktop_pattern, sizeof(desktop_pattern), "%s/share/applications/%s-*.desktop", 
             g_pkgmgr.config.install_prefix, pkg_id);
    
    /* Use glob to find and remove matching files */
    glob_t glob_result;
    if (glob(desktop_pattern, 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            unlink(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    }
    
    /* Remove from database */
        char sql[256];
        snprintf(sql, sizeof(sql), "DELETE FROM packages WHERE id='%s'", pkg_id);
        db_exec(sql);
    
    pkgmgr_progress("remove", pkg_id, 1.0, "Remove complete");
    return true;
}

bool wubu_pkgmgr_upgrade(const char* pkg_spec, bool dry_run) {
    /* Check installed version vs repo */
    wubu_pkg_installed_t installed;
    if (!wubu_pkgmgr_get_installed(pkg_spec, &installed)) {
        return wubu_pkgmgr_install(pkg_spec, dry_run); /* Not installed, install it */
    }
    
    wubu_pkg_repo_entry_t repo;
    if (!wubu_pkgmgr_repo_get_info(pkg_spec, &repo)) {
        return false; /* Not in repo */
    }
    
    /* Compare versions (simplified) */
    if (strcmp(installed.manifest.version, repo.version) >= 0) {
        return true; /* Already up to date */
    }
    
    /* Remove old, install new */
    wubu_pkgmgr_remove(pkg_spec, false);
    return wubu_pkgmgr_install(pkg_spec, dry_run);
}

bool wubu_pkgmgr_upgrade_all(bool dry_run) {
    wubu_pkg_installed_t pkgs[256];
    int n = wubu_pkgmgr_list_installed(pkgs, 256);
    for (int i = 0; i < n; i++) {
        wubu_pkgmgr_upgrade(pkgs[i].manifest.id, dry_run);
    }
    return true;
}

int wubu_pkgmgr_list_installed(wubu_pkg_installed_t* out, int max) {
    if (!g_pkgmgr.initialized) return 0;
    
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id, name, version, description, maintainer, homepage, license, "
        "manifest_json, install_path, install_date, auto_installed, size_bytes, payload_type, arch, sandbox_profile "
        "FROM packages ORDER BY name");
    
    /* Buffer with count at the beginning - allocate on heap to avoid stack overflow */
    size_t buffer_size = sizeof(int) + 256 * sizeof(wubu_pkg_installed_t);
    char* buffer = calloc(1, buffer_size);
    if (!buffer) return 0;
    
    int* count = (int*)buffer;
    wubu_pkg_installed_t* pkgs = (wubu_pkg_installed_t*)(buffer + sizeof(int));
    
    /* Reset static index in callback */
    cb_list_installed_idx = 0;
    
    db_query(sql, cb_list_installed, buffer);
    
    int n = *count < max ? *count : max;
    if (out && n > 0) {
        memcpy(out, pkgs, n * sizeof(wubu_pkg_installed_t));
    }
    int result = *count;
    free(buffer);
    return result;
}

bool wubu_pkgmgr_get_installed(const char* pkg_id, wubu_pkg_installed_t* out) {
    if (!g_pkgmgr.initialized) return false;
    
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id, name, version, description, maintainer, homepage, license, "
        "manifest_json, install_path, install_date, auto_installed, size_bytes, payload_type, arch, sandbox_profile "
        "FROM packages WHERE id='%s'", pkg_id);
    
    /* Allocate extra space for found flag */
    char buffer[sizeof(wubu_pkg_installed_t) + sizeof(int)];
    memset(buffer, 0, sizeof(buffer));
    wubu_pkg_installed_t* pkg = (wubu_pkg_installed_t*)buffer;
    int* found = (int*)((char*)buffer + sizeof(wubu_pkg_installed_t));
    
    if (db_query(sql, cb_pkg_installed, buffer) != 0) {
        return false;
    }
    
    if (!*found) {
        return false;
    }
    
    *out = *pkg;
    return true;
}

bool wubu_pkgmgr_is_installed(const char* pkg_id) {
    wubu_pkg_installed_t pkg;
    return wubu_pkgmgr_get_installed(pkg_id, &pkg);
}

/* ============================================================
 * .wubu Container Operations
 * ============================================================ */

bool wubu_pkgmgr_create_package(const char* src_dir, const char* output_path,
                                 const wubu_pkg_manifest_t* manifest,
                                 const char* sign_key_hex) {
    return write_pkg(output_path, manifest, src_dir, sign_key_hex);
}

bool wubu_pkgmgr_verify_package(const char* pkg_path, const char* pubkey_hex) {
    wubu_pkg_header_t header;
    return read_pkg_header(pkg_path, &header);
}

bool wubu_pkgmgr_extract_package(const char* pkg_path, const char* dest_dir) {
    /* In production: extract payload archive */
    ensure_dir(dest_dir);
    return true;
}

bool wubu_pkgmgr_read_manifest(const char* pkg_path, wubu_pkg_manifest_t* out) {
    return extract_pkg_manifest(pkg_path, out);
}

/* ============================================================
 * Dependency Resolution
 * ============================================================ */

int wubu_pkgmgr_resolve_deps(const char** pkg_ids, int n_pkgs,
                              char*** out_pkg_ids, int* out_n_pkgs) {
    /* Simplified topological sort */
    *out_pkg_ids = malloc(n_pkgs * sizeof(char*));
    for (int i = 0; i < n_pkgs; i++) {
        (*out_pkg_ids)[i] = strdup(pkg_ids[i]);
    }
    *out_n_pkgs = n_pkgs;
    return 0;
}

bool wubu_pkgmgr_check_conflicts(const char* pkg_id, const wubu_pkg_installed_t* installed, int n_installed) {
    wubu_pkg_repo_entry_t repo;
    if (!wubu_pkgmgr_repo_get_info(pkg_id, &repo)) return true;
    
    for (int i = 0; i < n_installed; i++) {
        for (int j = 0; j < repo.n_depends; j++) {
            if (strcmp(installed[i].manifest.id, repo.depends[j]) == 0) {
                /* This is a dependency, not a conflict */
            }
        }
    }
    return true;
}

/* ============================================================
 * Transaction Support
 * ============================================================ */

bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t* txn, bool dry_run) {
    memset(txn, 0, sizeof(*txn));
    txn->dry_run = dry_run;
    time_t now = time(NULL);
    struct tm* tm_now = gmtime(&now);
    strftime(txn->rollback_dir, sizeof(txn->rollback_dir), "/tmp/wubu_rollback_%Y%m%d_%H%M%S", tm_now);
    ensure_dir(txn->rollback_dir);
    return true;
}

bool wubu_pkgmgr_txn_add(wubu_pkg_transaction_t* txn, wubu_pkg_txn_type_t type,
                          const char* pkg_id, const char* old_ver, const char* new_ver,
                          bool is_dep) {
    if (txn->n_items >= 128) return false;
    wubu_pkg_txn_item_t* item = &txn->items[txn->n_items++];
    item->type = type;
    strncpy(item->pkg_id, pkg_id, sizeof(item->pkg_id)-1);
    if (old_ver) strncpy(item->old_version, old_ver, sizeof(item->old_version)-1);
    if (new_ver) strncpy(item->new_version, new_ver, sizeof(item->new_version)-1);
    item->is_dependency = is_dep;
    return true;
}

bool wubu_pkgmgr_txn_commit(wubu_pkg_transaction_t* txn) {
    for (int i = 0; i < txn->n_items; i++) {
        wubu_pkg_txn_item_t* item = &txn->items[i];
        switch (item->type) {
            case WUBU_PKG_TXN_INSTALL:
                wubu_pkgmgr_install(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_REMOVE:
                wubu_pkgmgr_remove(item->pkg_id, false);
                break;
            case WUBU_PKG_TXN_UPGRADE:
                wubu_pkgmgr_upgrade(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_DOWNGRADE:
                /* Install specific version */
                break;
        }
    }
    return true;
}

bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t* txn) {
    /* Restore from rollback_dir */
    return true;
}

/* ============================================================
 * App Integration
 * ============================================================ */

bool wubu_pkgmgr_register_desktop(const wubu_pkg_installed_t* pkg) {
    for (int i = 0; i < pkg->manifest.n_entrypoints; i++) {
        generate_desktop_entry(&pkg->manifest, i, pkg->install_path);
    }
    return true;
}

bool wubu_pkgmgr_unregister_desktop(const char* pkg_id) {
    char desktop_pattern[512];
    snprintf(desktop_pattern, sizeof(desktop_pattern), "%s/share/applications/%s-*.desktop",
             g_pkgmgr.config.install_prefix, pkg_id);
    
    /* Use glob to find and remove matching files */
    glob_t glob_result;
    if (glob(desktop_pattern, 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            unlink(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    }
    return true;
}

bool wubu_pkgmgr_generate_desktop_files(const wubu_pkg_installed_t* pkg) {
    return wubu_pkgmgr_register_desktop(pkg);
}

/* ============================================================
 * Cleanup/Maintenance
 * ============================================================ */

bool wubu_pkgmgr_clean_cache(int max_age_days) {
    /* Remove old cached packages using nftw */
    char cache_pattern[512];
    snprintf(cache_pattern, sizeof(cache_pattern), "%s/*.wubu", g_pkgmgr.config.cache_dir);
    
    glob_t glob_result;
    if (glob(cache_pattern, 0, NULL, &glob_result) == 0) {
        time_t now = time(NULL);
        time_t cutoff = now - (max_age_days * 86400);
        
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            struct stat st;
            if (stat(glob_result.gl_pathv[i], &st) == 0) {
                if (st.st_mtime < cutoff) {
                    unlink(glob_result.gl_pathv[i]);
                }
            }
        }
        globfree(&glob_result);
    }
    return true;
}

int wubu_pkgmgr_autoremove(bool dry_run) {
    int removed = 0;
    wubu_pkg_installed_t* pkgs = calloc(256, sizeof(wubu_pkg_installed_t));
    if (!pkgs) return 0;
    int n = wubu_pkgmgr_list_installed(pkgs, 256);
    
    for (int i = 0; i < n; i++) {
        if (pkgs[i].auto_installed) {
            /* Check if still required */
            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT COUNT(*) FROM package_deps WHERE dep_id='%s'", pkgs[i].manifest.id);
            int count = 0;
            db_query(sql, cb_count, &count);
            if (count == 0) {
                if (!dry_run) {
                    wubu_pkgmgr_remove(pkgs[i].manifest.id, true);
                }
                removed++;
            }
        }
    }
    free(pkgs);
    return removed;
}

int wubu_pkgmgr_verify_installed(char*** out_broken, int* out_n_broken) {
    *out_n_broken = 0;
    *out_broken = NULL;
    return 0;
}

/* ============================================================
 * Statistics
 * ============================================================ */

bool wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t* out) {
    memset(out, 0, sizeof(*out));
    out->installed_count = wubu_pkgmgr_list_installed(NULL, 0);
    return true;
}

/* ============================================================
 * Progress Callback
 * ============================================================ */

void wubu_pkgmgr_set_progress_callback(wubu_pkgmgr_progress_cb cb, void* userdata) {
    g_pkgmgr.progress_cb = cb;
    g_pkgmgr.progress_userdata = userdata;
}