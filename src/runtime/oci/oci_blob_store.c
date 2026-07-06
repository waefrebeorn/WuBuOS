/*
 * oci_blob_store.c  --  OCI Content-Addressable Blob Storage
 * 
 * Extracted from wubu_oci.c (lines 983-1053).
 */

#include "oci_internal.h"

/* -- Blob Store ------------------------------------------------------ */

int oci_blob_store_init(const char *root_path) {
    if (!root_path) return -1;
    char path[1024];
    /* Create root directory */
    if (mkdir(root_path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create blobs subdirectory */
    snprintf(path, sizeof(path), "%s/blobs", root_path);
    if (mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create sha256 algorithm directory */
    snprintf(path, sizeof(path), "%s/blobs/sha256", root_path);
    if (mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create oci-layout file to mark this as a valid OCI layout */
    snprintf(path, sizeof(path), "%s/oci-layout", root_path);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *layout = "{\"imageLayoutVersion\":\"1.0.0\"}";
        write(fd, layout, strlen(layout));
        close(fd);
    }
    return 0;
}

int oci_blob_put(const char *root_path, const char *digest, const void *data, size_t size) {
    if (!root_path || !digest || !data) return -1;

    char blob_path[1024];
    snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", root_path, digest + 7);

    char *slash = strrchr(blob_path, '/');
    if (slash) { *slash = '\0'; mkdir(blob_path, 0755); *slash = '/'; }

    int fd = open(blob_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    ssize_t n = write(fd, data, size);
    close(fd);

    return n == (ssize_t)size ? 0 : -1;
}

int oci_blob_get(const char *root_path, const char *digest, void *out_data, size_t *out_size) {
    if (!root_path || !digest) return -1;

    char blob_path[1024];
    snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", root_path, digest + 7);

    int fd = open(blob_path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    fstat(fd, &st);

    if (out_size) *out_size = st.st_size;
    if (out_data) {
        ssize_t n = read(fd, out_data, st.st_size);
        close(fd);
        return n == st.st_size ? 0 : -1;
    }

    close(fd);
    return 0;
}

bool oci_blob_exists(const char *root_path, const char *digest) {
    if (!root_path || !digest) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/blobs/sha256/%s", root_path, digest + 7);
    return access(path, F_OK) == 0;
}