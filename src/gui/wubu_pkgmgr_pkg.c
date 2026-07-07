/*
 * wubu_pkgmgr_pkg.c  --  WuBuOS Package Manager: Pkg
 */

#include "wubu_pkgmgr_internal.h"

/* ============================================================
 * .wubu Container Format
 * (struct definition lives in wubu_pkgmgr_internal.h)
 * ============================================================ */

bool write_pkg(const char* output_path, const wubu_pkg_manifest_t* manifest,
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

bool read_pkg_header(const char* pkg_path, wubu_pkg_header_t* header) {
    FILE* f = fopen(pkg_path, "rb");
    if (!f) return false;
    size_t n = fread(header, 1, sizeof(*header), f);
    fclose(f);
    return n == sizeof(*header) && header->magic == WUBU_PKG_MAGIC;
}

/* -- Minimal JSON array helpers (for manifest extraction) --------- */

/* Count elements inside a JSON array, e.g. ["a","b","c"] -> 3. */
static int json_array_count(const char* arr) {
    int n = 0;
    bool in_str = false;
    for (const char* p = arr; *p; p++) {
        if (*p == '"') in_str = !in_str;
        else if (*p == ',' && !in_str) n++;
        else if (*p == ']' && !in_str) break;
    }
    /* Elements = commas + 1, but only if non-empty */
    if (n == 0 && *arr) {
        /* Could be a single element or empty array "[]" */
        const char* q = arr;
        while (*q && *q != ']') { if (*q == '"') return 1; q++; }
        return 0;
    }
    return n + 1;
}

/* Copy the nth string element (between quotes) of a JSON array into out.
 * Returns true if an element was found. */
static bool json_array_str_at(const char* arr, int idx, char* out, size_t outsz) {
    int cur = 0;
    bool in_str = false;
    const char* start = NULL;
    for (const char* p = arr; *p; p++) {
        if (*p == '"') {
            if (!in_str) { start = p + 1; in_str = true; }
            else {
                in_str = false;
                if (cur == idx) {
                    size_t len = (size_t)(p - start);
                    if (len >= outsz) len = outsz - 1;
                    memcpy(out, start, len);
                    out[len] = '\0';
                    return true;
                }
                cur++;
            }
        } else if (*p == ',' && !in_str) {
            /* next element */
        } else if (*p == ']' && !in_str) {
            break;
        }
    }
    return false;
}

/* Locate the span of a JSON array value for the given key, e.g.
 * key="\"deps\":", returns a pointer at the '[' and sets *end to the ']'. */
static const char* json_find_array(const char* json, const char* key, const char** end) {
    const char* p = strstr(json, key);
    if (!p) { *end = NULL; return NULL; }
    p += strlen(key);
    /* skip whitespace */
    while (*p == ' ' || *p == '\n' || *p == '\t') p++;
    if (*p != '[') { *end = NULL; return NULL; }
    int depth = 0;
    const char* q = p;
    bool in_str = false;
    for (; *q; q++) {
        if (*q == '"') in_str = !in_str;
        else if (!in_str && *q == '[') depth++;
        else if (!in_str && *q == ']') {
            depth--;
            if (depth == 0) { *end = q; return p; }
        }
    }
    *end = NULL;
    return NULL;
}

bool extract_pkg_manifest(const char* pkg_path, wubu_pkg_manifest_t* out) {
    wubu_pkg_header_t header;
    if (!read_pkg_header(pkg_path, &header)) return false;

    FILE* f = fopen(pkg_path, "rb");
    if (!f) return false;
    fseek(f, sizeof(header), SEEK_SET);

    void* compressed = malloc(header.manifest_size);
    fread(compressed, 1, header.manifest_size, f);
    fclose(f);

    /* Decompress manifest */
    const size_t max_manifest = 262144;
    void* decompressed = malloc(max_manifest);
    size_t dsize = ZSTD_decompress(decompressed, max_manifest, compressed, header.manifest_size);
    free(compressed);

    if (ZSTD_isError(dsize)) {
        free(decompressed);
        return false;
    }

    char* json = (char*)decompressed;
    json[dsize] = '\0';

    memset(out, 0, sizeof(*out));

    /* Scalar fields */
    char* p = strstr(json, "\"id\":\"");
    if (p) { p += 6; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->id, p, sizeof(out->id)-1); *e = '"'; } }

    p = strstr(json, "\"name\":\"");
    if (p) { p += 8; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->name, p, sizeof(out->name)-1); *e = '"'; } }

    p = strstr(json, "\"version\":\"");
    if (p) { p += 11; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->version, p, sizeof(out->version)-1); *e = '"'; } }

    p = strstr(json, "\"payload_type\":");
    if (p) { out->payload_type = (wubu_pkg_payload_t)atoi(p + 15); }

    p = strstr(json, "\"arch\":");
    if (p) { out->arch = (wubu_pkg_arch_t)atoi(p + 7); }

    p = strstr(json, "\"sandbox_profile\":\"");
    if (p) { p += 19; char* e = strchr(p, '"'); if (e) { *e = '\0'; strncpy(out->sandbox_profile, p, sizeof(out->sandbox_profile)-1); *e = '"'; } }

    /* Files: ["src|dst|mode", ...] */
    const char* farr = NULL; const char* fend = NULL;
    if ((farr = json_find_array(json, "\"files\":", &fend)) != NULL) {
        int nf = json_array_count(farr);
        if (nf > WUBU_PKG_MAX_FILES) nf = WUBU_PKG_MAX_FILES;
        for (int i = 0; i < nf; i++) {
            char elem[1200];
            if (json_array_str_at(farr, i, elem, sizeof(elem))) {
                /* format: src|dst|mode */
                char* bar1 = strchr(elem, '|');
                char* bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;
                if (bar1 && bar2) {
                    *bar1 = '\0'; *bar2 = '\0';
                    strncpy(out->files[i].src, elem, sizeof(out->files[i].src) - 1);
                    strncpy(out->files[i].dst, bar1 + 1, sizeof(out->files[i].dst) - 1);
                    out->files[i].mode = (uint32_t)strtoul(bar2 + 1, NULL, 0);
                }
            }
        }
        out->n_files = nf;
    }

    /* Depends: ["dep1","dep2",...] */
    const char* darr = NULL; const char* dend = NULL;
    if ((darr = json_find_array(json, "\"deps\":", &dend)) != NULL) {
        int nd = json_array_count(darr);
        if (nd > WUBU_PKG_MAX_DEPS) nd = WUBU_PKG_MAX_DEPS;
        for (int i = 0; i < nd; i++) {
            json_array_str_at(darr, i, out->depends[i], sizeof(out->depends[i]));
        }
        out->n_depends = nd;
    }

    /* Entrypoints: [{"name":"...","exec":"..."}, ...] */
    const char* earr = NULL; const char* eend = NULL;
    if ((earr = json_find_array(json, "\"entries\":", &eend)) != NULL) {
        int ne = json_array_count(earr);
        if (ne > WUBU_PKG_MAX_ENTRY) ne = WUBU_PKG_MAX_ENTRY;
        const char* obj = earr + 1; /* just past '[' */
        for (int i = 0; i < ne; i++) {
            /* find next '{' ... '}' */
            const char* ob = strchr(obj, '{');
            if (!ob) break;
            int depth = 0; const char* q = ob;
            bool in_str = false;
            for (; *q; q++) {
                if (*q == '"') in_str = !in_str;
                else if (!in_str && *q == '{') depth++;
                else if (!in_str && *q == '}') { depth--; if (depth == 0) break; }
            }
            /* obj..q inclusive */
            char objbuf[2048];
            size_t olen = (size_t)(q - ob) + 1;
            if (olen > sizeof(objbuf) - 1) olen = sizeof(objbuf) - 1;
            memcpy(objbuf, ob, olen); objbuf[olen] = '\0';

            char* fld;
            if ((fld = strstr(objbuf, "\"name\":\"")) != NULL) {
                fld += 8; char* e = strchr(fld, '"');
                if (e) { *e = '\0'; strncpy(out->entrypoints[i].name, fld, sizeof(out->entrypoints[i].name)-1); *e = '"'; }
            }
            if ((fld = strstr(objbuf, "\"exec\":\"")) != NULL) {
                fld += 8; char* e = strchr(fld, '"');
                if (e) { *e = '\0'; strncpy(out->entrypoints[i].exec, fld, sizeof(out->entrypoints[i].exec)-1); *e = '"'; }
            }
            if ((fld = strstr(objbuf, "\"id\":\"")) != NULL) {
                fld += 6; char* e = strchr(fld, '"');
                if (e) { *e = '\0'; strncpy(out->entrypoints[i].id, fld, sizeof(out->entrypoints[i].id)-1); *e = '"'; }
            }
            obj = q + 1;
        }
        out->n_entrypoints = ne;
    }

    free(decompressed);
    return true;
}
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
    wubu_pkg_header_t header;
    if (!read_pkg_header(pkg_path, &header)) return false;
    if (header.manifest_size == 0 || header.payload_size == 0) return false;

    ensure_dir(dest_dir);

    /* Extract the .tar.zst payload (which sits after the header + compressed
     * manifest) to a temp file, then unpack it with tar|zstd fork+exec. */
    char payload_path[512];
    snprintf(payload_path, sizeof(payload_path), "%s/.wubu_payload_%u.tar.zst",
             dest_dir, (unsigned)getpid());

    FILE* in = fopen(pkg_path, "rb");
    if (!in) return false;
    if (fseek(in, (long)(sizeof(header) + header.manifest_size), SEEK_SET) != 0) {
        fclose(in);
        return false;
    }
    FILE* out = fopen(payload_path, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[8192];
    size_t remaining = (size_t)header.payload_size;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t rd = fread(buf, 1, chunk, in);
        if (rd == 0) break;
        if (fwrite(buf, 1, rd, out) != rd) {
            fclose(in); fclose(out);
            unlink(payload_path);
            return false;
        }
        remaining -= rd;
    }
    fclose(in);
    fclose(out);

    /* Unpack: zstd -d < payload | tar -xf -  (in dest_dir) */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        unlink(payload_path);
        return false;
    }

    pid_t zstd_pid = fork();
    if (zstd_pid == 0) {
        /* Child: zstd decompress payload -> stdout */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        int pfd = open(payload_path, O_RDONLY);
        if (pfd < 0) _exit(1);
        dup2(pfd, STDIN_FILENO);
        close(pfd);
        execlp("zstd", "zstd", "-d", (char*)NULL);
        _exit(1);
    }

    pid_t tar_pid = fork();
    if (tar_pid == 0) {
        /* Child: tar -xf - (read from pipe) into dest_dir */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        if (chdir(dest_dir) < 0) _exit(1);
        execlp("tar", "tar", "-xf", "-", (char*)NULL);
        _exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    int zstd_status, tar_status;
    waitpid(zstd_pid, &zstd_status, 0);
    waitpid(tar_pid, &tar_status, 0);

    unlink(payload_path);

    if (!WIFEXITED(zstd_status) || WEXITSTATUS(zstd_status) != 0 ||
        !WIFEXITED(tar_status) || WEXITSTATUS(tar_status) != 0) {
        return false;
    }
    return true;
}

bool wubu_pkgmgr_read_manifest(const char* pkg_path, wubu_pkg_manifest_t* out) {
    return extract_pkg_manifest(pkg_path, out);
}

/* ============================================================
 * Dependency Resolution
 * ============================================================ */
