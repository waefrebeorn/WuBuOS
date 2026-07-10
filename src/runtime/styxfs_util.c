/* styxfs_util.c -- StyxFS utility subsystem (mount/file resolution,
 * container load). Self-contained: uses styxfs_server_t/mount_t/file_t types
 * (styxfs.h/styxfs_server.h) and normalize_path (declared in styxfs_internal.h).
 * Minimal includes.
 */

#include "styxfs_internal.h"
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

styxfs_mount_t *styxfs_find_mount(styxfs_server_t *srv, const char *path, char *rel_path) {
    if (!srv || !path) return NULL;
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0) {
            if (rel_path) {
                const char *rest = path + mlen;
                if (*rest == '/') rest++;
                strncpy(rel_path, rest, STYXFS_MAX_PATH - 1);
            }
            return m;
        }
    }
    return NULL;
}

styxfs_file_t *styxfs_resolve(styxfs_server_t *srv, const char *path, int create_if_missing) {
    if (!srv || !path) return NULL;
    (void)create_if_missing;
    char norm[STYXFS_MAX_PATH];
    strncpy(norm, path, STYXFS_MAX_PATH - 1);
    norm[STYXFS_MAX_PATH - 1] = '\0';
    normalize_path(norm);
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && strcmp(srv->open_files[i].path, norm) == 0)
            return &srv->open_files[i];
    }
    return NULL;
}

int styxfs_load_container(const char *path, WUBU_HEADER *out_hdr, uint8_t **out_payload, size_t *out_size) {
    if (!path || !out_hdr || !out_payload || !out_size) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < (long)WUBU_HEADER_SIZE) {
        fclose(f);
        return -1;
    }

    /* Read entire file */
    uint8_t *buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t read = fread(buf, 1, fsize, f);
    fclose(f);
    if (read != (size_t)fsize) {
        free(buf);
        return -1;
    }

    /* Validate container */
    if (wubu_container_validate(buf, fsize) != 0) {
        free(buf);
        return -1;
    }

    /* Parse header and payload */
    WUBU_HEADER hdr;
    const void *payload = NULL;
    size_t payload_size = 0;
    if (wubu_container_parse(buf, fsize, &hdr, &payload, &payload_size) != 0) {
        free(buf);
        return -1;
    }

    *out_hdr = hdr;
    *out_payload = (uint8_t *)malloc(payload_size);
    if (!*out_payload) {
        free(buf);
        return -1;
    }
    memcpy(*out_payload, payload, payload_size);
    *out_size = payload_size;
    free(buf);

    return 0;
}

int styxfs_is_wubu_container(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    return len >= 5 && strcmp(path + len - 5, ".wubu") == 0;
}
