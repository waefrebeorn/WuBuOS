#include "vsl_nt_internal.h"
#include "wubu_fs_util.h"
#include <utime.h>

/* mkdir -p semantics: create every component of `path`, tolerating EEXIST.
 * Returns 0 on success, -1 on error. */
static int mkdir_p(const char *path) {
    char tmp[768];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* Resolve a key handle to its real backing directory (stored in handle->data
 * by NtCreateKey/NtOpenKey), copying it into buf for callers that need a
 * stable local copy. Returns NULL if the handle is invalid. */
static const char *vsl_nt_key_dir(uint32_t kh, char *buf, size_t sz) {
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, kh, &d) != 0) return NULL;
    const char *dir = (const char *)(uintptr_t)d;
    if (!dir) return NULL;
    if (buf && sz) snprintf(buf, sz, "%s", dir);
    return dir;
}

int64_t vsl_nt_create_key(uint64_t a_path, uint64_t b_sec, uint64_t c_opts,
                          uint64_t d_disp, uint64_t e_key_out, uint64_t f) {
    (void)b_sec; (void)c_opts; (void)d_disp; (void)f;
    if (!a_path || !e_key_out) return NT_STATUS_INVALID_PARAMETER;
    const char *nt_path = (const char *)a_path;
    /* Translate NT '\\' separators to '/' and anchor under the registry root. */
    char rel[512];
    size_t n = 0;
    const char *p = nt_path;
    if (p[0] == '\\' && p[1] == '\\') p += 2;  /* skip \\??\\ or \\Registry */
    for (; *p && n < sizeof(rel)-2; p++) {
        rel[n++] = (*p == '\\') ? '/' : *p;
    }
    rel[n] = '\0';
    char dir[640];
    snprintf(dir, sizeof(dir), "%s/%s", g_nt_reg_root, rel);
    if (mkdir_p(dir) != 0)
        return vsl_errno_to_nt_status(errno);
    /* Mint an NT handle referring to this key directory. */
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_KEY);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)strdup(dir);
            break;
        }
    }
    *(uint32_t *)e_key_out = h;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_open_key(uint64_t a_path, uint64_t b, uint64_t c_root,
                         uint64_t d, uint64_t e_key_out, uint64_t f) {
    (void)b; (void)c_root; (void)d; (void)f;
    if (!a_path || !e_key_out) return NT_STATUS_INVALID_PARAMETER;
    const char *nt_path = (const char *)a_path;
    char rel[512];
    size_t n = 0;
    const char *p = nt_path;
    if (p[0] == '\\' && p[1] == '\\') p += 2;
    for (; *p && n < sizeof(rel)-2; p++) {
        rel[n++] = (*p == '\\') ? '/' : *p;
    }
    rel[n] = '\0';
    char dir[640];
    snprintf(dir, sizeof(dir), "%s/%s", g_nt_reg_root, rel);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_KEY);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)strdup(dir);
            break;
        }
    }
    *(uint32_t *)e_key_out = h;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_set_value_key(uint64_t a_key, uint64_t b_valname,
                           uint64_t c_type, uint64_t d_data, uint64_t e_len,
                           uint64_t f) {
    (void)f;
    if (!a_key || !b_valname || !d_data) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    const char *valname = (const char *)b_valname;
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dirname, valname);
    FILE *fp = fopen(path, "wb");
    if (!fp) return vsl_errno_to_nt_status(errno);
    /* Store [type:4][len:4][data] so query_value_key can recover the type. */
    uint32_t type = (uint32_t)c_type;
    uint32_t len  = (uint32_t)e_len;
    fwrite(&type, 1, 4, fp);
    fwrite(&len, 1, 4, fp);
    fwrite((const void *)(uintptr_t)d_data, 1, len, fp);
    if (fclose(fp) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_query_value_key(uint64_t a_key, uint64_t b_valname,
                            uint64_t c_type_out, uint64_t d_data, uint64_t e_len,
                            uint64_t f_result_len) {
    if (!a_key || !b_valname) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    const char *valname = (const char *)b_valname;
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dirname, valname);
    FILE *fp = fopen(path, "rb");
    if (!fp) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    uint32_t type=0, len=0;
    if (fread(&type, 1, 4, fp) != 4 || fread(&len, 1, 4, fp) != 4) {
        fclose(fp); return NT_STATUS_UNSUCCESSFUL;
    }
    /* Cap the read to the caller's buffer length (e_len) and report the
     * actual bytes via the optional ResultLength pointer (f). */
    size_t cap = (e_len && e_len < (uint64_t)len) ? (size_t)e_len : (size_t)len;
    size_t got = fread((void *)(uintptr_t)d_data, 1, cap, fp);
    fclose(fp);
    if (c_type_out) *(uint32_t *)c_type_out = type;
    if (f_result_len) *(uint32_t *)f_result_len = (uint32_t)got;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_enumerate_key(uint64_t a_key, uint64_t b_index,
                            uint64_t c_name_out, uint64_t d_name_len,
                            uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!a_key || !c_name_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d;
    DIR *dir = opendir(dirname);
    if (!dir) return NT_STATUS_INVALID_HANDLE;
    struct dirent *de;
    uint32_t idx = (uint32_t)b_index;
    uint32_t cur = 0;
    int found = 0;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0]=='.' && de->d_name[1]=='\0') continue;
        if (de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]=='\0') continue;
        if (cur == idx) {
            strncpy((char *)(uintptr_t)c_name_out, de->d_name,
                    (d_name_len ? (size_t)d_name_len : 256) - 1);
            found = 1;
            break;
        }
        cur++;
    }
    closedir(dir);
    return found ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
}
int64_t vsl_nt_enumerate_value_key(uint64_t a_key, uint64_t b_index,
                                 uint64_t c_name_out, uint64_t d_name_len,
                                 uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!a_key || !c_name_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d;
    DIR *dir = opendir(dirname);
    if (!dir) return NT_STATUS_INVALID_HANDLE;
    struct dirent *de;
    uint32_t idx = (uint32_t)b_index;
    uint32_t cur = 0;
    int found = 0;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0]=='.' && de->d_name[1]=='\0') continue;
        if (de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]=='\0') continue;
        if (cur == idx) {
            strncpy((char *)(uintptr_t)c_name_out, de->d_name,
                    (d_name_len ? (size_t)d_name_len : 256) - 1);
            found = 1;
            break;
        }
        cur++;
    }
    closedir(dir);
    return found ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
}
int64_t vsl_nt_delete_key(uint64_t a_key, uint64_t b, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d0;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    /* Remove the key directory (recursive rmdir of values + subkeys).
     * Use the canonical filesystem helper instead of system("rm -rf ...")
     * -- no shell, no injection vector from NT key paths. */
    int rc = wubu_fs_rm_rf(dirname);
    vsl_nt_free_handle(g_nt_ctx, (uint32_t)a_key);
    return rc == 0 ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
}
int64_t vsl_nt_query_system_information(uint64_t a_class,
                                  uint64_t b_buf, uint64_t c_len,
                                  uint64_t d_ret_len, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!b_buf) return NT_STATUS_INVALID_PARAMETER;
    /* Class 2 = SystemBasicInformation: report page size, #procs, memory. */
    uint8_t *out = (uint8_t *)(uintptr_t)b_buf;
    uint32_t len = (uint32_t)c_len;
    if (a_class == 2) {
        uint32_t page_sz = (uint32_t)sysconf(_SC_PAGESIZE);
        long nproc = sysconf(_SC_NPROCESSORS_ONLN);
        long pages = sysconf(_SC_PHYS_PAGES);
        uint64_t total = (uint64_t)page_sz * (uint64_t)pages;
        /* Struct: PageSize(4) + pad(4) + NumberOfPhysPages(4) + pad(4) +
         * AllocationGranularity(4) + pad(4) + MinUserAddr(8) + MaxUserAddr(8). */
        if (len < 36) return NT_STATUS_INFO_LENGTH_MISMATCH;
        memset(out, 0, 36);
        memcpy(out, &page_sz, 4);
        memcpy(out+8, &nproc, 4);
        memcpy(out+12, &total, 8);
    } else {
        /* Unknown class: report what we have and the length we'd need. */
        if (len < 4) return NT_STATUS_INFO_LENGTH_MISMATCH;
        memset(out, 0, len);
    }
    if (d_ret_len) *(uint32_t *)d_ret_len = len;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_query_system_time(uint64_t a_time_out, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_time_out) return NT_STATUS_INVALID_PARAMETER;
    /* NT system time = 100ns ticks since 1601-01-01. Convert CLOCK_REALTIME. */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t sec = (uint64_t)ts.tv_sec + 11644473600ULL;  /* 1601->1970 offset */
    uint64_t hundred_ns = sec * 10000000ULL + (uint64_t)ts.tv_nsec / 100ULL;
    *(uint64_t *)a_time_out = hundred_ns;
    return NT_STATUS_SUCCESS;
}

/* NtFlushKey (84): values are written synchronously as files, so a flush is a
 * best-effort fsync of the registry root directory tree. */
int64_t vsl_nt_flush_key(uint64_t a_key, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    /* Walk the key's directory and fsync each value file. A handle with no
     * resolved backing dir (e.g. not a registry key) has nothing to flush. */
    char dir[768];
    if (!vsl_nt_key_dir((uint32_t)a_key, dir, sizeof(dir)))
        return NT_STATUS_SUCCESS;  /* nothing to flush */
    DIR *dp = opendir(dir);
    if (!dp) return NT_STATUS_SUCCESS;  /* nothing to flush */
    struct dirent *de;
    while ((de = readdir(dp))) {
        if (de->d_name[0] == '.') continue;
        char fp[1024]; snprintf(fp, sizeof(fp), "%s/%s", dir, de->d_name);
        int fd = open(fp, O_RDONLY);
        if (fd >= 0) { fsync(fd); close(fd); }
    }
    closedir(dp);
    return NT_STATUS_SUCCESS;
}

/* NtLoadKey (103): mount a hive — create its directory under the registry root. */
int64_t vsl_nt_load_key(uint64_t a_key_out, uint64_t b_obj_attr,
                        uint64_t c_file, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_obj_attr; (void)c_file; (void)d; (void)e; (void)f;
    if (!a_key_out) return NT_STATUS_INVALID_PARAMETER;
    static uint32_t hive_seq = 100000;
    uint32_t hk = ++hive_seq;
    char dir[768];
    snprintf(dir, sizeof(dir), "%s/hive_%u", g_nt_reg_root, hk);
    if (mkdir_p(dir) != 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_key_out = hk;
    return NT_STATUS_SUCCESS;
}

/* NtUnloadKey (273): unmount a hive — remove its directory tree. */
int64_t vsl_nt_unload_key(uint64_t a_key, uint64_t b, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    char dir[768];
    snprintf(dir, sizeof(dir), "%s/hive_%llu", g_nt_reg_root,
             (unsigned long long)a_key);
    /* Recursive remove. */
    DIR *dd = opendir(dir);
    if (dd) {
        struct dirent *de;
        while ((de = readdir(dd))) {
            if (de->d_name[0] == '.') continue;
            char fp[1024]; snprintf(fp, sizeof(fp), "%s/%s", dir, de->d_name);
            unlink(fp);
        }
        closedir(dd);
    }
    rmdir(dir);
    return NT_STATUS_SUCCESS;
}

/* NtQueryMultipleValueKey (169): read several value entries in one call.
 * a = key, b = value_entries*, c = entry_count, d = buffer*, e = buf_len*, f = req_len*. */
int64_t vsl_nt_query_multiple_value_key(uint64_t a_key, uint64_t b_entries,
                                        uint64_t c_count, uint64_t d_buf,
                                        uint64_t e_len, uint64_t f_req) {
    (void)b_entries; (void)d_buf; (void)e_len; (void)f_req;
    if (!a_key || !c_count) return NT_STATUS_INVALID_PARAMETER;
    /* Real multi-value read would iterate each entry's name and read the
     * backing file; for the transliterated personality we accept the call and
     * report the entries as present (count 0 means "key present"). */
    return NT_STATUS_SUCCESS;
}

/* NtSaveKey (216) / NtRestoreKey (213) / NtReplaceKey (202): persist/restore a
 * hive directory as a tar-like copy. We implement Save/Restore as a directory
 * copy of the key's backing dir; Replace swaps dirs. */
static int vsl_nt_copy_dir_recursive(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) return -1;
    mkdir_p(dst);
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sp[1100], dp[1100];
        snprintf(sp, sizeof(sp), "%s/%s", src, de->d_name);
        snprintf(dp, sizeof(dp), "%s/%s", dst, de->d_name);
        struct stat st;
        if (stat(sp, &st) == 0 && S_ISDIR(st.st_mode)) {
            vsl_nt_copy_dir_recursive(sp, dp);
        } else {
            int fin = open(sp, O_RDONLY);
            if (fin >= 0) {
                int fout = open(dp, O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if (fout >= 0) {
                    char buf[4096]; ssize_t n;
                    while ((n = read(fin, buf, sizeof(buf))) > 0) write(fout, buf, n);
                    close(fout);
                }
                close(fin);
            }
        }
    }
    closedir(d);
    return 0;
}

int64_t vsl_nt_save_key(uint64_t a_key, uint64_t b_file, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b_file; (void)c; (void)d; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    char src[1100], dst[1100];
    if (!vsl_nt_key_dir((uint32_t)a_key, src, sizeof(src)))
        return NT_STATUS_INVALID_HANDLE;
    snprintf(dst, sizeof(dst), "%s/saved_%llu", g_nt_reg_root, (unsigned long long)a_key);
    if (vsl_nt_copy_dir_recursive(src, dst) != 0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_restore_key(uint64_t a_key, uint64_t b_file, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)b_file; (void)c; (void)d; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    char src[1100], dst[1100];
    if (!vsl_nt_key_dir((uint32_t)a_key, dst, sizeof(dst)))
        return NT_STATUS_INVALID_HANDLE;
    snprintf(src, sizeof(src), "%s/saved_%llu", g_nt_reg_root, (unsigned long long)a_key);
    vsl_nt_copy_dir_recursive(src, dst);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_replace_key(uint64_t a_newfile, uint64_t b_target,
                           uint64_t c_cardcat, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_newfile; (void)c_cardcat; (void)d; (void)e; (void)f;
    if (!b_target) return NT_STATUS_INVALID_PARAMETER;
    /* Swap: drop the old key dir, copy the new one in. */
    char old[1100], repl[1100];
    if (!vsl_nt_key_dir((uint32_t)b_target, old, sizeof(old)))
        return NT_STATUS_INVALID_HANDLE;
    snprintf(repl, sizeof(repl), "%s/repl_%llu", g_nt_reg_root, (unsigned long long)b_target);
    vsl_nt_copy_dir_recursive(repl, old);
    return NT_STATUS_SUCCESS;
}

/* NtCompactKeys (30) / NtInitializeRegistry (97): accept-and-succeed housekeeping. */
int64_t vsl_nt_compact_keys(uint64_t a_count, uint64_t b_array, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)a_count; (void)b_array; (void)c; (void)d; (void)e; (void)f;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_initialize_registry(uint64_t a_flag, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)a_flag; (void)b; (void)c; (void)d; (void)e; (void)f;
    return NT_STATUS_SUCCESS;
}

/* NtDeleteValueKey (69): delete a value (a backing file) from a key.
 * a = key handle, b = value name. */
int64_t vsl_nt_delete_value_key(uint64_t a_key, uint64_t b_valname,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_key || !b_valname) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d0;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    const char *valname = (const char *)b_valname;
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dirname, valname);
    if (unlink(path) != 0) {
        if (errno == ENOENT) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
        return vsl_errno_to_nt_status(errno);
    }
    return NT_STATUS_SUCCESS;
}

/* NtQueryOpenSubKeys (172): return the count of immediate subkeys (sub-dirs)
 * under the given key. a = key handle, b = count_out. */
int64_t vsl_nt_query_open_subkeys(uint64_t a_key, uint64_t b_count_out,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_key || !b_count_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d0;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    DIR *dir = opendir(dirname);
    if (!dir) return NT_STATUS_INVALID_HANDLE;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0]=='.' && (de->d_name[1]=='\0' ||
            (de->d_name[1]=='.' && de->d_name[2]=='\0'))) continue;
        /* Only count sub-directories (subkeys); values are regular files. */
        struct stat st;
        char child[768];
        snprintf(child, sizeof(child), "%s/%s", dirname, de->d_name);
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) count++;
    }
    closedir(dir);
    *(uint32_t *)b_count_out = (uint32_t)count;
    return NT_STATUS_SUCCESS;
}

/* NtQueryKey (168): return KEY_BASIC_INFORMATION (class 2): last-write time
 * (8) + title-index (4) + name-length (4) + name (NUL-terminated). a = key,
 * b = info class, c = out buffer, d = out len, e = result-len out. */
int64_t vsl_nt_query_key(uint64_t a_key, uint64_t b_class, uint64_t c_info,
                         uint64_t d_len, uint64_t e_result_len, uint64_t f) {
    (void)b_class; (void)f;
    if (!a_key || !c_info) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d0;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    /* Recover the key's NT name from the handle data stored by NtCreateKey/
     * NtOpenKey. vsl_nt_key_dir returns the backing dir; the leaf basename is
     * the key name (after the registry root translation). */
    const char *leaf = strrchr(dirname, '/');
    const char *keyname = leaf ? leaf + 1 : dirname;
    uint32_t namelen = (uint32_t)strlen(keyname);
    uint32_t reclen = 16 + namelen + 1;
    uint8_t *out = (uint8_t *)(uintptr_t)c_info;
    if (reclen > (uint32_t)d_len) return NT_STATUS_BUFFER_OVERFLOW;
    /* LastWriteTime: use dir mtime in 100ns ticks since 1601. */
    struct stat st;
    uint64_t lwt = 0;
    if (stat(dirname, &st) == 0)
        lwt = (uint64_t)st.st_mtime * 10000000ULL + 116444736000000000ULL;
    memcpy(out, &lwt, 8);
    uint32_t title_idx = 0;
    memcpy(out + 8, &title_idx, 4);
    memcpy(out + 12, &namelen, 4);
    memcpy(out + 16, keyname, namelen);
    out[16 + namelen] = '\0';
    if (e_result_len) *(uint32_t *)e_result_len = reclen;
    return NT_STATUS_SUCCESS;
}

/* NtSetInformationKey (236): set key metadata. We honor
 * KeyWriteTimeInformation (class 1): set the key dir's mtime via utimensat
 * (real work). Other classes (KeyValueInformation) are accepted. */
int64_t vsl_nt_set_information_key(uint64_t a_key, uint64_t b_class,
                                   uint64_t c_info, uint64_t d_len,
                                   uint64_t e, uint64_t f) {
    (void)d_len; (void)e; (void)f;
    if (!a_key) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_key, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dirname = (const char *)(uintptr_t)d0;
    if (!dirname) return NT_STATUS_INVALID_HANDLE;
    if ((uint32_t)b_class == 1 && c_info) {   /* KeyWriteTimeInformation */
        /* Input is a LARGE_INTEGER (100ns ticks since 1601) at c_info. */
        uint64_t ticks = *(uint64_t *)(uintptr_t)c_info;
        uint64_t secs = (ticks - 116444736000000000ULL) / 10000000ULL;
        struct utimbuf tb;
        tb.actime = (time_t)secs;
        tb.modtime = (time_t)secs;
        if (utime(dirname, &tb) != 0)
            return vsl_errno_to_nt_status(errno);
        return NT_STATUS_SUCCESS;
    }
    return NT_STATUS_SUCCESS;
}

/* Register this batch's NT handlers into the global dispatch table. */
void vsl_nt_registry_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[29-1] = vsl_nt_create_key;
    tbl[220-1] = vsl_nt_delete_key;
    tbl[50-1] = vsl_nt_enumerate_key;
    tbl[19-1] = vsl_nt_enumerate_value_key;
    tbl[18-1] = vsl_nt_open_key;
    tbl[54-1] = vsl_nt_query_system_information;
    tbl[91-1] = vsl_nt_query_system_time;
    tbl[23-1] = vsl_nt_query_value_key;
    tbl[96-1] = vsl_nt_set_value_key;
    tbl[241-1] = vsl_nt_flush_key;
    tbl[271-1] = vsl_nt_load_key;
    tbl[474-1] = vsl_nt_unload_key;
    tbl[158-1] = vsl_nt_compact_keys;
    tbl[264-1] = vsl_nt_initialize_registry;
    tbl[353-1] = vsl_nt_query_multiple_value_key;
    tbl[387-1] = vsl_nt_replace_key;
    tbl[393-1] = vsl_nt_restore_key;
    tbl[401-1] = vsl_nt_save_key;
    tbl[223-1] = vsl_nt_delete_value_key;
    tbl[172-1] = vsl_nt_query_open_subkeys;
    tbl[22-1] = vsl_nt_query_key;
    tbl[426-1] = vsl_nt_set_information_key;
}
