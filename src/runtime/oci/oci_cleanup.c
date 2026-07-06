/*
 * oci_cleanup.c  --  OCI Layer Cleanup and Garbage Collection
 * 
 * Extracted from wubu_oci.c (lines 1634-1772).
 */

#include "oci_internal.h"

/* -- Cleanup Stub ---------------------------------------------------- */

int oci_cleanup_old_layers(const char *root_path, time_t max_age_days, bool dry_run) {
    if (!root_path || max_age_days <= 0) return -1;
    char blob_dir[1024];
    snprintf(blob_dir, sizeof(blob_dir), "%s/blobs/sha256", root_path);
    DIR *dir = opendir(blob_dir);
    if (!dir) {
        /* No blob directory means nothing to clean — not an error */
        if (errno == ENOENT) return 0;
        return -1;
    }
    time_t now = time(NULL);
    time_t max_age_secs = (time_t)max_age_days * 86400;
    int removed = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char fpath[1280];
        snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, entry->d_name);
        struct stat st;
        if (stat(fpath, &st) < 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        time_t age = now - st.st_mtime;
        if (age > max_age_secs) {
            if (!dry_run) {
                if (unlink(fpath) == 0) removed++;
            } else {
                removed++;
            }
        }
    }
    closedir(dir);
    return removed;
}

int oci_gc_unreferenced_blobs(const char *root_path, bool dry_run) {
    if (!root_path) return -1;
    char blob_dir[1024];
    snprintf(blob_dir, sizeof(blob_dir), "%s/blobs/sha256", root_path);
    /* Collect all blob digests */
    DIR *dir = opendir(blob_dir);
    if (!dir) {
        if (errno == ENOENT) return 0;
        return -1;
    }
    /* Collect all blob digests */
    typedef struct { char name[128]; time_t mtime; } blob_info_t;
    blob_info_t blobs[4096];
    int blob_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && blob_count < 4096) {
        if (entry->d_name[0] == '.') continue;
        char fpath[1280];
        snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, entry->d_name);
        struct stat st;
        if (stat(fpath, &st) < 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        strncpy(blobs[blob_count].name, entry->d_name, 127);
        blobs[blob_count].mtime = st.st_mtime;
        blob_count++;
    }
    closedir(dir);
    /* Mark referenced blobs: scan index.json and manifest files */
    char referenced[4096] = {0};
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.json", root_path);
    FILE *f = fopen(index_path, "r");
    if (f) {
        char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        /* Find all sha256: digests in index.json */
        const char *scan = buf;
        while ((scan = strstr(scan, "sha256:")) != NULL) {
            scan += 7;
            char digest[65];
            int i;
            for (i = 0; i < 64 && scan[i] && isxdigit((unsigned char)scan[i]); i++)
                digest[i] = scan[i];
            digest[i] = 0;
            if (i == 64) {
                for (int j = 0; j < blob_count; j++) {
                    if (strcmp(blobs[j].name, digest) == 0) {
                        referenced[j] = 1;
                        break;
                    }
                }
            }
        }
    }
    /* Also check manifest blobs in the blobs directory */
    for (int i = 0; i < blob_count; i++) {
        if (referenced[i]) continue;
        char mf_path[1280];
        snprintf(mf_path, sizeof(mf_path), "%s/%s", blob_dir, blobs[i].name);
        f = fopen(mf_path, "r");
        if (!f) continue;
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        /* If this blob contains "sha256:" references, it's a manifest — mark its refs */
        if (strstr(buf, "\"sha256:") || strstr(buf, "\"digest\"")) {
            const char *scan = buf;
            while ((scan = strstr(scan, "sha256:")) != NULL) {
                scan += 7;
                char digest[65];
                int k;
                for (k = 0; k < 64 && scan[k] && isxdigit((unsigned char)scan[k]); k++)
                    digest[k] = scan[k];
                digest[k] = 0;
                if (k == 64) {
                    for (int j = 0; j < blob_count; j++) {
                        if (strcmp(blobs[j].name, digest) == 0) {
                            referenced[j] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    /* Delete unreferenced blobs */
    int removed = 0;
    for (int i = 0; i < blob_count; i++) {
        if (!referenced[i]) {
            char fpath[1280];
            snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, blobs[i].name);
            if (!dry_run) {
                if (unlink(fpath) == 0) removed++;
            } else {
                removed++;
            }
        }
    }
    return removed;
}