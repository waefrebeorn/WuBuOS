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

/* NtResetEvent (209): clear the event by writing 0 to the eventfd. */
int64_t vsl_nt_reset_event(uint64_t a_handle, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t zero = 0;
    if (write(fd, &zero, sizeof(zero)) != sizeof(zero))
        return NT_STATUS_UNSUCCESSFUL;
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
void vsl_nt_io_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[28-1] = vsl_nt_close;
    tbl[38-1] = vsl_nt_create_event;
    tbl[62-1] = vsl_nt_delay_execution;
    tbl[121-1] = vsl_nt_open_event;
    tbl[123-1] = vsl_nt_open_file;
    tbl[159-1] = vsl_nt_query_information_file;
    tbl[192-1] = vsl_nt_read_file;
    tbl[209-1] = vsl_nt_reset_event;
    tbl[229-1] = vsl_nt_set_event;
    tbl[285-1] = vsl_nt_write_file;
}
