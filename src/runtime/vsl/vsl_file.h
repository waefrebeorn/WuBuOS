/*
 * vsl_file.h  --  VSL File Operations API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_FILE_H
#define WUBUOS_VSL_FILE_H

#include <stdint.h>
#include <stddef.h>

/* Forward declare opaque type */
struct VSL_FD;
typedef struct VSL_FD VSL_FD;

/* Open a file in VSL.
 * Returns FD number (>=3), or -1 on error. */
int vsl_open(const char *path, int flags, int mode);

/* Close a file in VSL.
 * Returns 0 on success, -1 on error. */
int vsl_close(int fd);

/* Read from a file in VSL.
 * Returns bytes read, or -1 on error. */
int64_t vsl_read(int fd, void *buf, size_t count);

/* Write to a file in VSL.
 * Returns bytes written, or -1 on error. */
int64_t vsl_write(int fd, const void *buf, size_t count);

/* Seek in a file.
 * Returns new offset, or -1 on error. */
int64_t vsl_lseek(int fd, int64_t offset, int whence);

/* File descriptor accessors */
int vsl_fd_get_fd(const VSL_FD *vfd);
uint32_t vsl_fd_get_flags(const VSL_FD *vfd);
uint32_t vsl_fd_get_mode(const VSL_FD *vfd);
int vsl_fd_get_host_fd(const VSL_FD *vfd);
const char *vsl_fd_get_path(const VSL_FD *vfd);

/* Get VSL_FD by FD number */
VSL_FD *vsl_get_fd(int fd);

#endif /* WUBUOS_VSL_FILE_H */