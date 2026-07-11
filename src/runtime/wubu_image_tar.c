/* wubu_image_tar.c -- WuBuOS image builder: TAR layer writer.
 * Extracted from wubu_image.c (separable leaf). Self-contained: only
 * filesystem + ftw. C11, minimal includes.
 */
#define _GNU_SOURCE
#include "wubu_image.h"
#include "wubu_image_internal.h"
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/wait.h>

/* Forward declarations (static helpers called before their definition) */
static int tar_write_header(int fd, const char *name, mode_t mode, uid_t uid, gid_t gid, off_t size, time_t mtime);
static int tar_write_file(int fd, const char *path, const struct stat *st);
int tar_write_dir(int fd, const char *src_dir);


static int remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)ftwbuf;
    int rv = remove(fpath);
    if (rv) {
        fprintf(stderr, "[wubu_image] failed to remove %s: %s\n", fpath, strerror(errno));
    }
    return rv;
}

int rmtree(const char *path) {
    if (!path || !path[0]) return -1;
    return nftw(path, remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

int create_layer_tar(const char *src_dir, const char *dest_tar, const char *instruction) {
    (void)instruction;
    /* Simple tar writer - creates a POSIX ustar tar archive */
    int fd = open(dest_tar, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    int ret = tar_write_dir(fd, src_dir);
    
    /* Write two 512-byte blocks of zeros (end of archive) */
    if (ret == 0) {
        char zeros[1024] = {0};
        write(fd, zeros, 1024);
    }
    
    close(fd);
    return ret;
}

static int tar_write_header(int fd, const char *name, mode_t mode, uid_t uid, gid_t gid, off_t size, time_t mtime) {
    char header[512] = {0};
    
    /* name (100 bytes) */
    strncpy(header, name, 99);
    /* mode (8 bytes, octal) */
    snprintf(header + 100, 8, "%07o", (unsigned int)mode);
    /* uid (8 bytes, octal) */
    snprintf(header + 108, 8, "%07o", (unsigned int)uid);
    /* gid (8 bytes, octal) */
    snprintf(header + 116, 8, "%07o", (unsigned int)gid);
    /* size (12 bytes, octal) */
    snprintf(header + 124, 12, "%011lo", (unsigned long)size);
    /* mtime (12 bytes, octal) */
    snprintf(header + 136, 12, "%011lo", (unsigned long)mtime);
    /* chksum (8 bytes) - initially spaces */
    memset(header + 148, ' ', 8);
    /* typeflag (1 byte) - '0' for regular file, '5' for directory */
    header[156] = S_ISDIR(mode) ? '5' : '0';
    /* linkname (100 bytes) - empty for regular files */
    /* magic (6 bytes) - "ustar" */
    memcpy(header + 257, "ustar", 5);
    /* version (2 bytes) - "00" */
    header[263] = '0';
    header[264] = '0';
    /* uname (32 bytes) */
    /* gname (32 bytes) */
    /* devmajor, devminor (8 bytes each) */
    
    /* Calculate checksum */
    unsigned int chksum = 0;
    for (int i = 0; i < 512; i++) {
        chksum += (unsigned char)header[i];
    }
    snprintf(header + 148, 8, "%06o\0", chksum);
    
    return write(fd, header, 512) == 512 ? 0 : -1;
}

static int tar_write_file(int fd, const char *path, const struct stat *st) {
    if (tar_write_header(fd, path, st->st_mode, st->st_uid, st->st_gid, st->st_size, st->st_mtime) < 0) {
        return -1;
    }
    
    if (S_ISREG(st->st_mode)) {
        int src_fd = open(path, O_RDONLY);
        if (src_fd < 0) return -1;
        
        char buf[8192];
        ssize_t n;
        while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
            if (write(fd, buf, n) != n) {
                close(src_fd);
                return -1;
            }
        }
        close(src_fd);
        
        /* Pad to 512-byte boundary */
        off_t size = st->st_size;
        size_t padding = (512 - (size % 512)) % 512;
        if (padding > 0) {
            char pad[512] = {0};
            if (write(fd, pad, padding) != (ssize_t)padding) {
                return -1;
            }
        }
    }
    return 0;
}

int tar_write_dir(int fd, const char *src_dir) {
    char path[WUBU_MAX_PATH];
    struct stat st;
    DIR *dir = opendir(src_dir);
    if (!dir) return -1;
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        snprintf(path, sizeof(path), "%s/%s", src_dir, ent->d_name);
        if (lstat(path, &st) < 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            /* Write directory header */
            if (tar_write_header(fd, path, st.st_mode, st.st_uid, st.st_gid, 0, st.st_mtime) < 0) {
                closedir(dir);
                return -1;
            }
            /* Recurse into subdirectory */
            if (tar_write_dir(fd, path) < 0) {
                closedir(dir);
                return -1;
            }
        } else {
            /* Write file */
            if (tar_write_file(fd, path, &st) < 0) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return 0;
}
