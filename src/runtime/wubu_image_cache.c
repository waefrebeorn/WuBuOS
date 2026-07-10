/* wubu_image_cache.c -- Layer cache subsystem (self-contained).
 *
 * wubu_layer_cache_get/put/exists: on-disk digest->blob cache under
 * CACHE_DIR. Uses WUBU_MAX_PATH (wubu_image.h). Minimal includes.
 */

#include "wubu_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef CACHE_DIR
#define CACHE_DIR "/var/cache/wubu/layers"
#endif

int wubu_layer_cache_get(const char *digest, void *out_data, size_t *out_size) {
    if (!digest || !out_data || !out_size) return -1;
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    fstat(fd, &st);
    *out_size = st.st_size;
    
    ssize_t n = read(fd, out_data, *out_size);
    close(fd);
    
    return n == (ssize_t)*out_size ? 0 : -1;
}

int wubu_layer_cache_put(const char *digest, const void *data, size_t size) {
    if (!digest || !data || size == 0) return -1;
    
    mkdir(CACHE_DIR, 0755);
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    ssize_t n = write(fd, data, size);
    close(fd);
    
    return n == (ssize_t)size ? 0 : -1;
}

bool wubu_layer_cache_exists(const char *digest) {
    if (!digest) return false;
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    return access(path, F_OK) == 0;
}
