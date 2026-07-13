#include "vsl_nt_internal.h"

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
    /* Remove the key directory (recursive rmdir of values + subkeys). */
    char cmd[768];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dirname);
    int rc = system(cmd);
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

/* Register this batch's NT handlers into the global dispatch table. */
void vsl_nt_registry_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[44-1] = vsl_nt_create_key;
    tbl[67-1] = vsl_nt_delete_key;
    tbl[76-1] = vsl_nt_enumerate_key;
    tbl[78-1] = vsl_nt_enumerate_value_key;
    tbl[126-1] = vsl_nt_open_key;
    tbl[182-1] = vsl_nt_query_system_information;
    tbl[183-1] = vsl_nt_query_system_time;
    tbl[186-1] = vsl_nt_query_value_key;
    tbl[257-1] = vsl_nt_set_value_key;
}
