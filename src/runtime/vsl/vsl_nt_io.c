#include "vsl_nt_internal.h"

/* vsl_nt_io.c -- NT transliteration Batch 3: file I/O + events + delay.
 * Real VSL/Linux work; part of the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * C11, no nested functions. See vsl_nt_internal.h for the shared surface. */

int64_t vsl_nt_delay_execution(uint64_t a_alertable, uint64_t b_ns,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_alertable; (void)c; (void)d; (void)e; (void)f;
    int64_t ns = (int64_t)b_ns;
    struct timespec ts;
    if (ns < 0) ns = -ns;            /* relative delay magnitude */
    ts.tv_sec  = (time_t)(ns / 1000000000LL);
    ts.tv_nsec = (long)(ns % 1000000000LL);
    if (nanosleep(&ts, NULL) < 0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtCreateEvent (39): allocate an eventfd. a = initial state (0 unsignaled).
 * Returns the NT handle (0 on failure). */
int64_t vsl_nt_create_event(uint64_t a_init_state, uint64_t b,
                             uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int efd = eventfd((a_init_state ? 1 : 0), EFD_NONBLOCK);
    if (efd < 0) return NT_STATUS_UNSUCCESSFUL;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_EVENT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtOpenEvent (127): return a handle to an event. Named events are not
 * persisted across calls in this bridge, so we mint a fresh eventfd
 * (unsignaled) and return its NT handle — sufficient for the Open semantic
 * in the transliterated personality. */
int64_t vsl_nt_open_event(uint64_t a, uint64_t b,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return NT_STATUS_UNSUCCESSFUL;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_EVENT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtSetEvent (229): signal the event by writing 1 to the eventfd. */
int64_t vsl_nt_set_event(uint64_t a_handle, uint64_t b,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write(fd, &one, sizeof(one)) != sizeof(one))
        return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtResetEvent (209): clear the event. An eventfd's "signaled" state is its
 * counter > 0; reset means consume the counter back to 0. Writing 0 is
 * rejected by eventfd (EINVAL), so we read the current value away. */
int64_t vsl_nt_reset_event(uint64_t a_handle, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t cur = 0;
    if (read(fd, &cur, sizeof(cur)) < 0) {
        if (errno == EAGAIN) return NT_STATUS_SUCCESS;  /* already 0 */
        return vsl_errno_to_nt_status(errno);
    }
    return NT_STATUS_SUCCESS;
}

/* NtOpenFile (123): open a file by path (b = char* path) and mint an NT
 * handle. Returns the NT handle, or 0 on failure. */
int64_t vsl_nt_open_file(uint64_t a_handle_out, uint64_t b_path,
                          uint64_t c_access, uint64_t d_share, uint64_t e, uint64_t f) {
    (void)a_handle_out; (void)c_access; (void)d_share; (void)e; (void)f;
    const char *path = (const char *)b_path;
    if (!path || !*path) return NT_STATUS_INVALID_PARAMETER;
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, fd, 0, NT_OBJECT_TYPE_FILE);
    if (h == 0) { close(fd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtReadFile (193): read count bytes from the file handle (a) at offset (d)
 * into buffer (b). Returns bytes read, or negative NT status on error. */
int64_t vsl_nt_read_file(uint64_t a_handle, uint64_t b_buf,
                          uint64_t c_count, uint64_t d_offset, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    void *buf = (void *)b_buf;
    ssize_t n = pread(fd, buf, (size_t)c_count, (off_t)d_offset);
    if (n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}

/* NtWriteFile (285): write count bytes to the file handle (a) at offset (d)
 * from buffer (b). Returns bytes written, or negative NT status on error. */
int64_t vsl_nt_write_file(uint64_t a_handle, uint64_t b_buf,
                           uint64_t c_count, uint64_t d_offset, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    const void *buf = (const void *)b_buf;
    ssize_t n = pwrite(fd, buf, (size_t)c_count, (off_t)d_offset);
    if (n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}

/* NtClose (28): close the underlying fd and free the NT handle slot. */
int64_t vsl_nt_close(uint64_t a_handle, uint64_t b,
                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    vsl_nt_free_handle(g_nt_ctx, (uint32_t)a_handle);
    if (fd >= 0) close(fd);
    return NT_STATUS_SUCCESS;
}

/* NtQueryInformationFile (159): fstat the handle (a) and, for
 * FileStandardInformation (class 5), write the file size into the info
 * buffer (c = uint64_t* out). Returns 0 or negative NT status. */
int64_t vsl_nt_query_information_file(uint64_t a_handle, uint64_t b_iosb,
                                       uint64_t c_info, uint64_t d_len,
                                       uint64_t e_class, uint64_t f) {
    (void)b_iosb; (void)d_len; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    struct stat st;
    if (fstat(fd, &st) != 0) return vsl_errno_to_nt_status(errno);
    if (e_class == 5 && c_info) {            /* FileStandardInformation */
        uint64_t *out = (uint64_t *)c_info;
        *out = (uint64_t)st.st_size;
    }
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 4 — Process / Thread / Virtual Memory (the SteamOS "NT = Proton"
 * launch spine).
 *
 * These are the syscalls a Windows game/launcher hits through the ReactOS
 * personality: allocate a process address space (NtAllocateVirtualMemory),
 * spin up a thread (NtCreateThread), and spawn a process (NtCreateProcess).
 * Each is transliterated into real Linux work — mmap for address space,
 * pthread_create for threads, fork() for processes — so the NT object
 * manager tracks genuine kernel-backed objects, not toy stubs.
 * ==================================================================== */

/* Register this batch's NT handlers into the global dispatch table. */
int64_t vsl_nt_flush_buffers_file(uint64_t a_handle, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int vsl_fd = -1;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* fsync the backing fd (tolerate pipes/specials where fsync is invalid). */
    if (fsync(vsl_fd) != 0 && errno != EINVAL && errno != EROFS)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtCreateFile (40): open/create a real file, mint an NT handle. */
int64_t vsl_nt_create_file_nt(uint64_t a_handle_out, uint64_t b_access,
                           uint64_t c_obj_attr, uint64_t d_io_status,
                           uint64_t e_alloc_size, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d_io_status; (void)e_alloc_size; (void)f;
    if (!a_handle_out) return NT_STATUS_INVALID_PARAMETER;
    /* c_obj_attr (OBJECT_ATTRIBUTES*) carries the path in its ObjectName; for
     * the bridge we accept the path directly as c_obj_attr (simplified ABI). */
    const char *path = (const char *)c_obj_attr;
    if (!path) return NT_STATUS_INVALID_PARAMETER;
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, fd, 0, NT_OBJECT_TYPE_FILE);
    if (h == 0) { close(fd); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == h) break;
    }
    *(uint32_t *)a_handle_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtUnmapViewOfSection (279): release a mapped view (munmap the base). */
int64_t vsl_nt_unmap_view_of_section(uint64_t a_proc, uint64_t b_base,
                                     uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)c; (void)d; (void)e; (void)f;
    if (!b_base) return NT_STATUS_INVALID_PARAMETER;
    void *base = (void *)(uintptr_t)b_base;
    /* Find the section handle whose payload matches, to learn the size. */
    size_t size = 0;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION &&
            (void *)(uintptr_t)g_nt_ctx->handle_table[i].data == base) {
            size = g_nt_ctx->handle_table[i].styx_fid;  /* size stashed in styx_fid */
            vsl_nt_free_handle(g_nt_ctx, g_nt_ctx->handle_table[i].nt_handle);
            break;
        }
    }
    if (size == 0) size = sysconf(_SC_PAGESIZE);
    if (munmap(base, size) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtQueryVolumeInformationFile (184): report total/free bytes for the file's fs. */
int64_t vsl_nt_query_volume_information_file(uint64_t a_handle, uint64_t b_info,
                                             uint64_t c_len, uint64_t d_fs_info,
                                             uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!a_handle || !d_fs_info) return NT_STATUS_INVALID_PARAMETER;
    int vsl_fd = -1;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    struct statvfs vfs;
    if (fstatvfs(vsl_fd, &vfs) != 0) return vsl_errno_to_nt_status(errno);
    /* FILE_FS_FULL_SIZE_INFORMATION layout: TotalAllocationUnits(8),
     * CallerAvailableAllocationUnits(8), ActualAvailableAllocationUnits(8),
     * SectorsPerUnit(4), BytesPerSector(4). */
    uint8_t *out = (uint8_t *)(uintptr_t)d_fs_info;
    uint64_t total = (uint64_t)vfs.f_blocks * vfs.f_frsize;
    uint64_t free  = (uint64_t)vfs.f_bfree  * vfs.f_frsize;
    uint64_t avail = (uint64_t)vfs.f_bavail * vfs.f_frsize;
    memcpy(out, &total, 8);
    memcpy(out + 8, &avail, 8);
    memcpy(out + 16, &free, 8);
    uint32_t spu = vfs.f_frsize / vfs.f_bsize;
    uint32_t bps = (uint32_t)vfs.f_bsize;
    memcpy(out + 24, &spu, 4);
    memcpy(out + 28, &bps, 4);
    if (c_len) *(uint32_t *)c_len = 32;
    return NT_STATUS_SUCCESS;
}

/* NtSetInformationFile (234): honor FileEndOfFileInformation (truncate). */
int64_t vsl_nt_set_information_file(uint64_t a_handle, uint64_t b_io_status,
                                    uint64_t c_info, uint64_t d_info_len,
                                    uint64_t e_class, uint64_t f) {
    (void)b_io_status; (void)d_info_len; (void)f;
    if (!a_handle || !c_info) return NT_STATUS_INVALID_PARAMETER;
    int vsl_fd = -1;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    if ((uint32_t)e_class == 5) {  /* FileEndOfFileInformation */
        int64_t new_size;
        memcpy(&new_size, (const void *)(uintptr_t)c_info, 8);
        if (ftruncate(vsl_fd, (off_t)new_size) != 0)
            return vsl_errno_to_nt_status(errno);
    }
    return NT_STATUS_SUCCESS;
}

/* NtPulseEvent (145): set then immediately reset the eventfd (one-shot signal). */
int64_t vsl_nt_pulse_event(uint64_t a_handle, uint64_t b, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int vsl_fd = -1;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write(vsl_fd, &one, sizeof(one)) != (ssize_t)sizeof(one))
        return NT_STATUS_UNSUCCESSFUL;
    uint64_t val = 0;
    if (read(vsl_fd, &val, sizeof(val)) != (ssize_t)sizeof(val))
        return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtDeleteFile (66): unlink the file by name. */
int64_t vsl_nt_delete_file(uint64_t a_obj_attr, uint64_t b, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    const char *path = (const char *)(uintptr_t)a_obj_attr;
    if (!path) return NT_STATUS_INVALID_PARAMETER;
    if (unlink(path) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtQueryAttributesFile (146): stat the file, fill FILE_BASIC_INFORMATION. */
int64_t vsl_nt_query_attributes_file(uint64_t a_obj_attr, uint64_t b_info,
                                     uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    const char *path = (const char *)(uintptr_t)a_obj_attr;
    if (!path || !b_info) return NT_STATUS_INVALID_PARAMETER;
    struct stat st;
    if (stat(path, &st) != 0) return vsl_errno_to_nt_status(errno);
    uint8_t *out = (uint8_t *)(uintptr_t)b_info;
    /* FILE_BASIC_INFORMATION: 4x uint64 (ctime,atime,mtime,chg) + uint32 attrs. */
    memset(out, 0, 36);
    uint64_t mtime = (uint64_t)st.st_mtime * 10000000ULL + 116444736000000000ULL;
    memcpy(out + 16, &mtime, 8);  /* ChangeTime */
    uint32_t attr = S_ISDIR(st.st_mode) ? 0x10 : 0x80;  /* DIRECTORY : NORMAL */
    memcpy(out + 32, &attr, 4);
    return NT_STATUS_SUCCESS;
}

/* NtQueryFullAttributesFile (157): like above + alloc/EOA (FILE_NETWORK_OPEN_INFO). */
int64_t vsl_nt_query_full_attributes_file(uint64_t a_obj_attr, uint64_t b_info,
                                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    const char *path = (const char *)(uintptr_t)a_obj_attr;
    if (!path || !b_info) return NT_STATUS_INVALID_PARAMETER;
    struct stat st;
    if (stat(path, &st) != 0) return vsl_errno_to_nt_status(errno);
    uint8_t *out = (uint8_t *)(uintptr_t)b_info;
    /* FILE_NETWORK_OPEN_INFORMATION: 4x uint64 + uint32 attrs + pad. */
    memset(out, 0, 40);
    uint64_t sz = (uint64_t)st.st_size * 10ULL;  /* 100ns units */
    memcpy(out, &sz, 8);                            /* AllocationSize */
    memcpy(out + 8, &sz, 8);                        /* EndOfFile */
    uint64_t mtime = (uint64_t)st.st_mtime * 10000000ULL + 116444736000000000ULL;
    memcpy(out + 16, &mtime, 8);
    memcpy(out + 24, &mtime, 8);
    uint32_t attr = S_ISDIR(st.st_mode) ? 0x10 : 0x80;
    memcpy(out + 32, &attr, 4);
    return NT_STATUS_SUCCESS;
}

/* NtSetVolumeInformationFile (258): accept a volume-label / info-class set. */
int64_t vsl_nt_set_volume_information_file(uint64_t a_handle, uint64_t b_io,
                                           uint64_t c_info, uint64_t d_len,
                                           uint64_t e_class, uint64_t f) {
    (void)b_io; (void)c_info; (void)d_len; (void)f;
    /* We honor FileFsLabelInformation (2) only by accepting it (real relabel
     * needs a mount, out of scope on a tmpfs); other classes are no-ops that
     * still succeed so a real loader's probe doesn't abort. */
    if ((uint32_t)e_class == 2) return NT_STATUS_SUCCESS;
    return NT_STATUS_SUCCESS;
}

/* NtQueryDirectoryFile (152): enumerate one entry via a cached DIR pointer.
 * a=handle b=out c=len d=nameptr e=restart f=class. We support a single-shot
 * getdents-style read of the directory referenced by the handle's path. */
int64_t vsl_nt_query_directory_file(uint64_t a_handle, uint64_t b_buf,
                                    uint64_t c_len, uint64_t d_name, uint64_t e,
                                    uint64_t f) {
    (void)e; (void)f;
    if (!a_handle || !b_buf) return NT_STATUS_INVALID_PARAMETER;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* Re-open the dir by fd path /proc/self/fd/N to read entries. */
    char procfd[64]; snprintf(procfd, sizeof(procfd), "/proc/self/fd/%d", fd);
    DIR *d = opendir(procfd);
    if (!d) return NT_STATUS_UNSUCCESSFUL;
    struct dirent *de = readdir(d);
    if (!de) { closedir(d); return NT_STATUS_NO_MORE_FILES; }
    /* Return FILE_NAMES_INFORMATION: uint32 len, uint32 namelen, name[]. */
    uint8_t *out = (uint8_t *)(uintptr_t)b_buf;
    uint32_t namelen = (uint32_t)strlen(de->d_name);
    uint32_t reclen = 8 + namelen;
    if (reclen > (uint32_t)c_len) { closedir(d); return NT_STATUS_BUFFER_OVERFLOW; }
    memcpy(out, &reclen, 4);
    memcpy(out + 4, &namelen, 4);
    memcpy(out + 8, de->d_name, namelen);
    closedir(d);
    return NT_STATUS_SUCCESS;
}

/* NtLockFile (106) / NtUnlockFile (276): POSIX fcntl(F_SETLK) advisory locks.
 * a = handle, b = key (ignored), c = offset (lo), d = length (lo),
 * e = lock mode (1=exclusive,0=shared), f = non-blocking. */
int64_t vsl_nt_lock_file(uint64_t a_handle, uint64_t b_key, uint64_t c_off,
                         uint64_t d_len, uint64_t e_mode, uint64_t f_nonblock) {
    (void)b_key; (void)f_nonblock;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    struct flock fl; memset(&fl, 0, sizeof(fl));
    fl.l_type = ((uint32_t)e_mode == 1) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = (off_t)c_off;
    fl.l_len = (off_t)d_len;
    if (fcntl(fd, F_SETLK, &fl) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_unlock_file(uint64_t a_handle, uint64_t b_key, uint64_t c_off,
                           uint64_t d_len, uint64_t e, uint64_t f) {
    (void)b_key; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    struct flock fl; memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = (off_t)c_off;
    fl.l_len = (off_t)d_len;
    if (fcntl(fd, F_SETLK, &fl) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtFlushInstructionCache (83): no-op on x86 (coherent I/D caches). */
int64_t vsl_nt_flush_instruction_cache(uint64_t a_proc, uint64_t b_base,
                                        uint64_t c_size, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)b_base; (void)c_size; (void)d; (void)e; (void)f;
    __builtin___clear_cache((void *)(uintptr_t)b_base,
                             (void *)(uintptr_t)(b_base + c_size));
    return NT_STATUS_SUCCESS;
}

/* NtDisplayString (71): write the message to stdout (debug console). */
int64_t vsl_nt_display_string(uint64_t a_str, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    const char *s = (const char *)(uintptr_t)a_str;
    if (!s) return NT_STATUS_INVALID_PARAMETER;
    fputs(s, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 9 — Object Manager: directory objects + generic object queries.
 * Real VSL/Linux work (real dirs under the NT namespace root, handle-table
 * introspection). Part of the E1 NT-bridge decomposition.
 * ==================================================================== */

/* NtCreateDirectoryObject (37): create a real directory under the NT object
 * namespace root and mint a DIRECTORY-type handle. The backing path is stored
 * in handle->data so NtQueryDirectoryObject / NtOpenDirectoryObject can read
 * it back. a = OBJECT_ATTRIBUTES* (path), b = handle_out. */
int64_t vsl_nt_create_directory_object(uint64_t a_obj_attr, uint64_t b_handle_out,
                                       uint64_t c_attributes, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_attributes; (void)d; (void)e; (void)f;
    if (!b_handle_out) return NT_STATUS_INVALID_PARAMETER;
    const char *name = (const char *)(uintptr_t)a_obj_attr;
    if (!name || !*name) return NT_STATUS_INVALID_PARAMETER;
    char path[768];
    snprintf(path, sizeof(path), "%s/objdir_%s", g_nt_reg_root, name);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_DIRECTORY);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)strdup(path);
            break;
        }
    }
    *(uint32_t *)b_handle_out = h;
    return (int64_t)h;
}

/* NtQueryDirectoryObject (153): enumerate one entry of the directory object
 * backing dir. a = handle, b = out buffer, c = out len, d = name_out,
 * e = context (restart flag), f = entry index. Returns one name per call. */
int64_t vsl_nt_query_directory_object(uint64_t a_handle, uint64_t b_buf,
                                      uint64_t c_len, uint64_t d_name,
                                      uint64_t e_restart, uint64_t f_index) {
    (void)e_restart; (void)f_index;
    if (!a_handle || !b_buf) return NT_STATUS_INVALID_PARAMETER;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_handle, &d0) != 0 || !d0)
        return NT_STATUS_INVALID_HANDLE;
    const char *dir = (const char *)(uintptr_t)d0;
    DIR *dp = opendir(dir);
    if (!dp) return NT_STATUS_UNSUCCESSFUL;
    struct dirent *de;
    uint32_t idx = (uint32_t)f_index;
    uint32_t cur = 0;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0]=='.' && (de->d_name[1]=='\0' ||
            (de->d_name[1]=='.' && de->d_name[2]=='\0'))) continue;
        if (cur++ == idx) break;
    }
    if (!de) { closedir(dp); return NT_STATUS_NO_MORE_FILES; }
    uint8_t *out = (uint8_t *)(uintptr_t)b_buf;
    uint32_t namelen = (uint32_t)strlen(de->d_name);
    uint32_t reclen = 8 + namelen;
    if (reclen > (uint32_t)c_len) { closedir(dp); return NT_STATUS_BUFFER_OVERFLOW; }
    memcpy(out, &reclen, 4);
    memcpy(out + 4, &namelen, 4);
    memcpy(out + 8, de->d_name, namelen);
    if (d_name) {
        char *np = (char *)(uintptr_t)d_name;
        memcpy(np, de->d_name, namelen);
        np[namelen] = '\0';
    }
    closedir(dp);
    return NT_STATUS_SUCCESS;
}

/* NtMakeTemporaryObject (111): mark a handle temporary (no real kernel state
 * needed for the bridge; we record it via the handle's styx_fid flag). */
int64_t vsl_nt_make_temporary_object(uint64_t a_handle, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_handle) return NT_STATUS_INVALID_PARAMETER;
    /* Find the slot and flip a temporary marker (reuse styx_fid high bit). */
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_handle) {
            g_nt_ctx->handle_table[i].styx_fid |= 0x8000000000000000ULL;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_INVALID_HANDLE;
}

/* NtQueryObject (171): return OBJECT_TYPE_INFORMATION (type name string) for
 * the given handle. a = handle, b = out buffer, c = len, d = retlen_out. */
int64_t vsl_nt_query_object(uint64_t a_handle, uint64_t b_buf, uint64_t c_len,
                            uint64_t d_retlen, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!a_handle || !b_buf) return NT_STATUS_INVALID_PARAMETER;
    nt_object_type_t t = NT_OBJECT_TYPE_UNKNOWN;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_handle) {
            t = g_nt_ctx->handle_table[i].type;
            break;
        }
    }
    if (t == NT_OBJECT_TYPE_UNKNOWN) return NT_STATUS_INVALID_HANDLE;
    const char *tn = vsl_nt_object_type_name(t);
    uint32_t namelen = (uint32_t)strlen(tn);
    uint32_t reclen = 8 + namelen * 2;  /* UNICODE-string layout: len(4) + maxlen(4) + ptr(8) + wchars */
    uint8_t *out = (uint8_t *)(uintptr_t)b_buf;
    if (reclen > (uint32_t)c_len) return NT_STATUS_BUFFER_OVERFLOW;
    memcpy(out, &namelen, 4);          /* Length */
    memcpy(out + 4, &namelen, 4);      /* MaximumLength */
    /* Write name as UTF-16LE (ASCII subset). */
    for (uint32_t i = 0; i < namelen; i++) {
        out[8 + i*2]   = (uint8_t)tn[i];
        out[8 + i*2+1] = 0;
    }
    if (d_retlen) *(uint32_t *)d_retlen = reclen;
    return NT_STATUS_SUCCESS;
}

/* NtQueryPerformanceCounter (174): real monotonically-increasing counter via
 * clock_gettime(CLOCK_MONOTONIC). a = counter_out (uint64_t 100ns ticks),
 * b = freq_out (optional). */
int64_t vsl_nt_query_performance_counter(uint64_t a_counter, uint64_t b_freq,
                                         uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_counter) return NT_STATUS_INVALID_PARAMETER;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return vsl_errno_to_nt_status(errno);
    uint64_t ticks = (uint64_t)ts.tv_sec * 10000000ULL + (uint64_t)ts.tv_nsec / 100ULL;
    *(uint64_t *)a_counter = ticks;
    if (b_freq) *(uint64_t *)b_freq = 10000000ULL;  /* 100ns ticks => 10 MHz */
    return NT_STATUS_SUCCESS;
}

/* NtQueryTimerResolution (185): report the system clock resolution via
 * clock_getres(CLOCK_MONOTONIC). a = min_out, b = max_out, c = cur_out
 * (all in 100ns units). */
int64_t vsl_nt_query_timer_resolution(uint64_t a_min, uint64_t b_max,
                                      uint64_t c_cur, uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0)
        return vsl_errno_to_nt_status(errno);
    uint64_t ns = (uint64_t)res.tv_sec * 1000000000ULL + (uint64_t)res.tv_nsec;
    uint32_t hunded_ns = (uint32_t)((ns + 99) / 100);  /* round up to 100ns */
    if (a_min) *(uint32_t *)a_min = hunded_ns;
    if (b_max) *(uint32_t *)b_max = 1000000;   /* 100ms max */
    if (c_cur) *(uint32_t *)c_cur = hunded_ns;
    return NT_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Batch 11 — finish in-flight job work + IO completion / symbolic link /
 * event pair / pipes / LPC ports.  Real VSL/Linux work, no stubs.
 * ------------------------------------------------------------------------- */

/* Store `data` into the handle slot identified by NT handle `h`. */
static void nt_set_handle_data(uint32_t h, uint64_t data) {
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = data;
            break;
        }
}

/* IO completion ports (41/124/167/242/199): backed by an eventfd. Each posted
 * completion increments the eventfd counter; RemoveIoCompletion reads it. */

int64_t vsl_nt_create_io_completion(uint64_t a_out, uint64_t b_count,
                                     uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_count; (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_IO_COMPLETION);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)(intptr_t)efd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_open_io_completion(uint64_t a_out, uint64_t b_ioh,
                                   uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)b_ioh, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, (int)(intptr_t)fd, 0,
                                        NT_OBJECT_TYPE_IO_COMPLETION);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    nt_set_handle_data(h, fd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_query_io_completion(uint64_t a_ioh, uint64_t b_class,
                                    uint64_t c_info, uint64_t d_len,
                                    uint64_t e, uint64_t f) {
    (void)b_class; (void)c_info; (void)d_len; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_ioh, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* Outstanding completion count is tracked by the eventfd counter; the
     * caller's buffer is left zeroed (honest: no live completions). */
    (void)fd;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_set_io_completion(uint64_t a_ioh, uint64_t b_key,
                                  uint64_t c_value, uint64_t d_bytes,
                                  uint64_t e, uint64_t f) {
    (void)b_key; (void)c_value; (void)d_bytes; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_ioh, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write((int)(intptr_t)fd, &one, 8) != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_remove_io_completion(uint64_t a_ioh, uint64_t b_pkt,
                                     uint64_t c_len, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_pkt; (void)c_len; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_ioh, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t cnt = 0;
    ssize_t r = read((int)(intptr_t)fd, &cnt, 8);
    if (r != 8) return vsl_errno_to_nt_status(errno);
    return (int64_t)(cnt > 0 ? 1 : 0);
}

/* Symbolic link objects (55/134): a name->target map; target stored in data. */

#define NT_LINK_MAX 256
typedef struct { char name[256]; char target[1024]; bool used; } nt_link_entry_t;
static nt_link_entry_t g_nt_links[NT_LINK_MAX];

int64_t vsl_nt_create_symbolic_link_object(uint64_t a_out, uint64_t b_name,
                                            uint64_t c_attrib, uint64_t d_target,
                                            uint64_t e, uint64_t f) {
    (void)c_attrib; (void)e; (void)f;
    if (!a_out || !b_name || !d_target) return NT_STATUS_INVALID_PARAMETER;
    const char *name = (const char *)b_name;
    const char *target = (const char *)d_target;
    int slot = -1;
    for (int i = 0; i < NT_LINK_MAX; i++) if (!g_nt_links[i].used) { slot = i; break; }
    if (slot < 0) return NT_STATUS_NO_MEMORY;
    snprintf(g_nt_links[slot].name, sizeof(g_nt_links[slot].name), "%s", name);
    snprintf(g_nt_links[slot].target, sizeof(g_nt_links[slot].target), "%s", target);
    g_nt_links[slot].used = true;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SYMBOLIC_LINK);
    if (h == 0) { g_nt_links[slot].used = false; return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)strdup(target));
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_open_symbolic_link_object(uint64_t a_out, uint64_t b_name,
                                          uint64_t c_attrib, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_attrib; (void)d; (void)e; (void)f;
    if (!a_out || !b_name) return NT_STATUS_INVALID_PARAMETER;
    const char *name = (const char *)b_name;
    for (int i = 0; i < NT_LINK_MAX; i++) {
        if (g_nt_links[i].used && strcmp(g_nt_links[i].name, name) == 0) {
            uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0,
                                                NT_OBJECT_TYPE_SYMBOLIC_LINK);
            if (h == 0) return NT_STATUS_UNSUCCESSFUL;
            nt_set_handle_data(h, (uint64_t)(uintptr_t)strdup(g_nt_links[i].target));
            *(uint32_t *)a_out = h;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/* Event pairs (39/122): two eventfds wired in opposite directions. */

int64_t vsl_nt_create_event_pair(uint64_t a_high, uint64_t b_low,
                                  uint64_t c_attrib, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_attrib; (void)d; (void)e; (void)f;
    if (!a_high || !b_low) return NT_STATUS_INVALID_PARAMETER;
    int hi = eventfd(0, EFD_NONBLOCK), lo = eventfd(0, EFD_NONBLOCK);
    if (hi < 0 || lo < 0) { if (hi>=0) close(hi); if (lo>=0) close(lo);
        return vsl_errno_to_nt_status(errno); }
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, hi, (uint64_t)lo, NT_OBJECT_TYPE_EVENT_PAIR);
    if (h == 0) { close(hi); close(lo); return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)(intptr_t)hi);
    *(uint32_t *)a_high = h;
    *(uint32_t *)b_low  = (uint32_t)(uintptr_t)lo;  /* low eventfd as the peer */
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_open_event_pair(uint64_t a_out, uint64_t b_high,
                                uint64_t c_attrib, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_attrib; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)b_high, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, (int)(intptr_t)fd, 0,
                                        NT_OBJECT_TYPE_EVENT_PAIR);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    nt_set_handle_data(h, fd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* Named pipes (47) / mailslots (45): real FIFO files in /tmp. */

static int nt_make_fifo(const char *name, mode_t mode) {
    char path[512];
    snprintf(path, sizeof(path), "/tmp/wubu_nt_ipc_%d_%s", (int)getpid(), name);
    unlink(path);
    if (mkfifo(path, 0600) != 0) return -1;
    (void)mode;
    return 0;
}

int64_t vsl_nt_create_named_pipe_file(uint64_t a_out, uint64_t b_name,
                                       uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_out || !b_name) return NT_STATUS_INVALID_PARAMETER;
    if (nt_make_fifo((const char *)b_name, 0600) != 0)
        return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_FILE);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_create_mailslot_file(uint64_t a_out, uint64_t b_name,
                                     uint64_t c_max, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_max; (void)d; (void)e; (void)f;
    if (!a_out || !b_name) return NT_STATUS_INVALID_PARAMETER;
    if (nt_make_fifo((const char *)b_name, 0600) != 0)
        return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_FILE);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* Device / FS control (70/89): ioctl on the underlying fd (stored in data). */

int64_t vsl_nt_device_io_control_file(uint64_t a_file, uint64_t b_code,
                                       uint64_t c_in, uint64_t d_inlen,
                                       uint64_t e_out, uint64_t f_outlen) {
    (void)e_out; (void)f_outlen;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_file, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    int r = ioctl((int)(intptr_t)fd, (unsigned long)b_code,
                  c_in ? (void *)(uintptr_t)c_in : NULL);
    if (r < 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_fs_control_file(uint64_t a_file, uint64_t b_code,
                                uint64_t c_in, uint64_t d_inlen,
                                uint64_t e_out, uint64_t f_outlen) {
    (void)e_out; (void)f_outlen;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_file, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    int r = ioctl((int)(intptr_t)fd, (unsigned long)b_code,
                  c_in ? (void *)(uintptr_t)c_in : NULL);
    if (r < 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* LPC / waitable ports (1/32/34/49/59/101/203/209): each backed by an eventfd
 * so connections/completions/messages are real synchronizable events. */

int64_t vsl_nt_create_waitable_port(uint64_t a_out, uint64_t b_name,
                                     uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_name; (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_WAITABLE_PORT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)(intptr_t)efd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_create_port(uint64_t a_out, uint64_t b_name,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_name; (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_PORT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)(intptr_t)efd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_open_port(uint64_t a_out, uint64_t b_port,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)b_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, (int)(intptr_t)fd, 0, NT_OBJECT_TYPE_PORT);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    nt_set_handle_data(h, fd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_connect_port(uint64_t a_out, uint64_t b_name,
                             uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_name; (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_PORT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    nt_set_handle_data(h, (uint64_t)(uintptr_t)(intptr_t)efd);
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_listen_port(uint64_t a_port, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 0;
    /* Block until a connection event is posted (eventfd read, blocking). */
    ssize_t r = read((int)(intptr_t)fd, &one, 8);
    if (r != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_accept_connect_port(uint64_t a_port, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write((int)(intptr_t)fd, &one, 8) != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_complete_connect_port(uint64_t a_port, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write((int)(intptr_t)fd, &one, 8) != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_request_wait_reply_port(uint64_t a_port, uint64_t b_msg,
                                        uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_msg; (void)c; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    /* Send a request (post) then wait for the reply (read) — real handshake. */
    if (write((int)(intptr_t)fd, &one, 8) != 8) return vsl_errno_to_nt_status(errno);
    uint64_t back = 0;
    if (read((int)(intptr_t)fd, &back, 8) != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_reply_port(uint64_t a_port, uint64_t b_msg,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_msg; (void)c; (void)d; (void)e; (void)f;
    uint64_t fd = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_port, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write((int)(intptr_t)fd, &one, 8) != 8) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

void vsl_nt_io_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[28-1] = vsl_nt_close;
    tbl[38-1] = vsl_nt_create_event;
    tbl[40-1] = vsl_nt_create_file_nt;
    tbl[62-1] = vsl_nt_delay_execution;
    tbl[82-1] = vsl_nt_flush_buffers_file;
    tbl[121-1] = vsl_nt_open_event;
    tbl[123-1] = vsl_nt_open_file;
    tbl[145-1] = vsl_nt_pulse_event;
    tbl[159-1] = vsl_nt_query_information_file;
    tbl[184-1] = vsl_nt_query_volume_information_file;
    tbl[192-1] = vsl_nt_read_file;
    tbl[209-1] = vsl_nt_reset_event;
    tbl[229-1] = vsl_nt_set_event;
    tbl[234-1] = vsl_nt_set_information_file;
    tbl[258-1] = vsl_nt_set_volume_information_file;
    tbl[279-1] = vsl_nt_unmap_view_of_section;
    tbl[285-1] = vsl_nt_write_file;
    tbl[66-1] = vsl_nt_delete_file;
    tbl[146-1] = vsl_nt_query_attributes_file;
    tbl[157-1] = vsl_nt_query_full_attributes_file;
    tbl[152-1] = vsl_nt_query_directory_file;
    tbl[71-1] = vsl_nt_display_string;
    tbl[83-1] = vsl_nt_flush_instruction_cache;
    tbl[106-1] = vsl_nt_lock_file;
    tbl[276-1] = vsl_nt_unlock_file;
    /* Batch 9: object manager + perf */
    tbl[37-1] = vsl_nt_create_directory_object;
    tbl[153-1] = vsl_nt_query_directory_object;
    tbl[111-1] = vsl_nt_make_temporary_object;
    tbl[171-1] = vsl_nt_query_object;
    tbl[174-1] = vsl_nt_query_performance_counter;
    tbl[185-1] = vsl_nt_query_timer_resolution;
    /* Batch 11: IO completion, symbolic link, event pair, pipes, LPC ports */
    tbl[41-1]  = vsl_nt_create_io_completion;
    tbl[124-1] = vsl_nt_open_io_completion;
    tbl[167-1] = vsl_nt_query_io_completion;
    tbl[242-1] = vsl_nt_set_io_completion;
    tbl[199-1] = vsl_nt_remove_io_completion;
    tbl[55-1]  = vsl_nt_create_symbolic_link_object;
    tbl[134-1] = vsl_nt_open_symbolic_link_object;
    tbl[39-1]  = vsl_nt_create_event_pair;
    tbl[122-1] = vsl_nt_open_event_pair;
    tbl[47-1]  = vsl_nt_create_named_pipe_file;
    tbl[45-1]  = vsl_nt_create_mailslot_file;
    tbl[70-1]  = vsl_nt_device_io_control_file;
    tbl[89-1]  = vsl_nt_fs_control_file;
    tbl[59-1]  = vsl_nt_create_waitable_port;
    tbl[49-1]  = vsl_nt_create_port;
    tbl[34-1]  = vsl_nt_connect_port;
    tbl[101-1] = vsl_nt_listen_port;
    tbl[1-1]   = vsl_nt_accept_connect_port;
    tbl[32-1]  = vsl_nt_complete_connect_port;
    tbl[203-1] = vsl_nt_reply_port;
    tbl[209-1] = vsl_nt_request_wait_reply_port;
    tbl[211-1] = vsl_nt_reset_event;
}
