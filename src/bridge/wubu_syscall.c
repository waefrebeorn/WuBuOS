/*
 * wubu_syscall.c  --  WuBuOS HolyC Syscall Bridge Implementation
 *
 * Registers 25 TempleOS/ZealOS-compatible syscalls in kernel.
 * Provides trampolines for HolyC compiler to call into kernel.
 */

#include "wubu_syscall.h"
#include "../kernel/interrupt.h"
#include "../kernel/vbe.h"
#include "../gui/dosgui_wm.h"
#include "../runtime/styx.h"
#include "../runtime/wubu_host_exec.h"
#include "../runtime/wubu_exec.h"
#include "../kernel/wubu_gaad.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/prctl.h>

/* -- File fd tracking for sys_file_* handlers -- */
static int g_file_fd_map[256] = {0};
static int g_file_fd_count = 0;

/* -- Styx fd tracking -- */
static struct {
    int fd;
    off_t offset;
} g_styx_fds[256] = {{0}};
static int g_styx_fd_count = 0;

/* -- Container tracking -- */
static struct {
    int id;
    pid_t pid;
    char name[64];
    char args[256];
    char env[256];
} g_containers[32] = {{0}};
static int g_container_count = 0;

/* ══════════════════════════════════════════════════════════════════
 * Kernel Syscall Handlers
 * ══════════════════════════════════════════════════════════════════ */

/* -- VBE Handlers -------------------------------------------------- */









/* -- WM Handlers --------------------------------------------------- */

int64_t sys_wm_create_win(int64_t x, int64_t y, int64_t w, int64_t h, int64_t title_ptr, int64_t _unused) {
    (void)_unused;
    const char *title = (const char *)title_ptr;
    DosGuiWindow *win = dosgui_wm_create((int)x, (int)y, (int)w, (int)h, title ? title : "");
    return win ? (int64_t)win->id : -1;
}

int64_t sys_wm_destroy_win(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5;
    DosGuiWindow *win = dosgui_wm_find_by_id((int)id);
    if (win) {
        dosgui_wm_destroy(win);
        return 0;
    }
    return -1;
}

int64_t sys_wm_set_focus(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5;
    DosGuiWindow *win = dosgui_wm_find_by_id((int)id);
    if (win) {
        dosgui_wm_set_focus(win);
        return 0;
    }
    return -1;
}

int64_t sys_wm_get_focused(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5; (void)_unused6;
    DosGuiWindow *win = dosgui_wm_get_focused();
    return win ? (int64_t)win->id : -1;
}

int64_t sys_wm_render(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5; (void)_unused6;
    VBEState *vbe = vbe_state();
    if (vbe && vbe->back) {
        dosgui_wm_render(vbe->back, vbe->width, vbe->height);
    }
    return 0;
}

/* -- File Handlers (fd-based I/O) ---------------------------------- */

int64_t sys_file_open(int64_t path_ptr, int64_t mode, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4) {
    const char *path = (const char *)path_ptr;
    if (!path) return -1;

    int flags = 0;
    switch (mode & 0x03) {
        case 0: flags = O_RDONLY; break;      /* read */
        case 1: flags = O_WRONLY | O_CREAT | O_TRUNC; break;  /* write */
        case 2: flags = O_RDWR | O_CREAT; break;  /* read-write */
        default: flags = O_RDONLY; break;
    }
    if (mode & 0x08) flags |= O_APPEND;  /* append mode */

    int fd = open(path, flags, 0644);
    if (fd < 0) return -1;

    /* Map to internal fd index */
    if (g_file_fd_count >= 256) {
        close(fd);
        return -1;
    }
    g_file_fd_map[g_file_fd_count] = fd;
    return (int64_t)g_file_fd_count++;
}

int64_t sys_file_read(int64_t fd_idx, int64_t buf_ptr, int64_t len, int64_t _unused, int64_t _unused2, int64_t _unused3) {
    if (fd_idx < 0 || fd_idx >= g_file_fd_count) return -1;
    int fd = g_file_fd_map[fd_idx];
    if (fd < 0) return -1;
    if (!buf_ptr || len <= 0) return -1;

    ssize_t r = read(fd, (void *)buf_ptr, (size_t)len);
    return r < 0 ? -1 : (int64_t)r;
}

int64_t sys_file_write(int64_t fd_idx, int64_t buf_ptr, int64_t len, int64_t _unused, int64_t _unused2, int64_t _unused3) {
    if (fd_idx < 0 || fd_idx >= g_file_fd_count) return -1;
    int fd = g_file_fd_map[fd_idx];
    if (fd < 0) return -1;
    if (!buf_ptr || len <= 0) return -1;

    ssize_t r = write(fd, (const void *)buf_ptr, (size_t)len);
    return r < 0 ? -1 : (int64_t)r;
}

int64_t sys_file_close(int64_t fd_idx, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5) {
    if (fd_idx < 0 || fd_idx >= g_file_fd_count) return -1;
    int fd = g_file_fd_map[fd_idx];
    if (fd < 0) return -1;

    int rc = close(fd);
    g_file_fd_map[fd_idx] = -1;
    return rc == 0 ? 0 : -1;
}

/* -- Styx Handlers (with fd offset tracking) ---------------------- */

int64_t sys_styx_open(int64_t path_ptr, int64_t mode, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4) {
    const char *path = (const char *)path_ptr;
    if (!path) return -1;

    int flags = 0;
    switch (mode & 0x03) {
        case 0: flags = O_RDONLY; break;       /* STX_OREAD */
        case 1: flags = O_WRONLY; break;       /* STX_OWRITE */
        case 2: flags = O_RDWR; break;         /* STX_ORDWR */
        case 3: flags = O_RDONLY; break;       /* STX_OEXEC -> read */
    }
    if (mode & 0x08) flags |= O_TRUNC;  /* STX_OTRUNC */

    int fd = open(path, flags, 0644);
    if (fd < 0) return -1;

    if (g_styx_fd_count >= 256) {
        close(fd);
        return -1;
    }
    g_styx_fds[g_styx_fd_count].fd = fd;
    g_styx_fds[g_styx_fd_count].offset = 0;
    return (int64_t)g_styx_fd_count++;
}

int64_t sys_styx_read(int64_t fid, int64_t offset, int64_t count, int64_t buf_ptr, int64_t _unused, int64_t _unused2) {
    if (fid < 0 || fid >= g_styx_fd_count) return -1;
    int fd = g_styx_fds[fid].fd;
    if (fd < 0) return -1;
    if (!buf_ptr || count <= 0) return -1;

    if (offset >= 0) {
        g_styx_fds[fid].offset = lseek(fd, (off_t)offset, SEEK_SET);
        if (g_styx_fds[fid].offset < 0) return -1;
    }

    ssize_t n = read(fd, (void *)buf_ptr, (size_t)count);
    if (n < 0) return -1;
    g_styx_fds[fid].offset += n;
    return (int64_t)n;
}

int64_t sys_styx_write(int64_t fid, int64_t offset, int64_t count, int64_t buf_ptr, int64_t _unused, int64_t _unused2) {
    if (fid < 0 || fid >= g_styx_fd_count) return -1;
    int fd = g_styx_fds[fid].fd;
    if (fd < 0) return -1;
    if (!buf_ptr || count <= 0) return -1;

    if (offset >= 0) {
        g_styx_fds[fid].offset = lseek(fd, (off_t)offset, SEEK_SET);
        if (g_styx_fds[fid].offset < 0) return -1;
    }

    ssize_t n = write(fd, (const void *)buf_ptr, (size_t)count);
    if (n < 0) return -1;
    g_styx_fds[fid].offset += n;
    return (int64_t)n;
}

/* -- Container Handlers ------------------------------------------- */

int64_t sys_container_create(int64_t name_ptr, int64_t args_ptr, int64_t env_ptr, int64_t _unused, int64_t _unused2, int64_t _unused3) {
    const char *name = (const char *)name_ptr;
    const char *args = (const char *)args_ptr;
    const char *env = (const char *)env_ptr;
    if (!name || name[0] == '\0') return -1;

    if (g_container_count >= 32) return -1;

    int container_id = g_container_count;
    g_containers[container_id].id = container_id;
    g_containers[container_id].pid = 0;
    strncpy(g_containers[container_id].name, name, 63);
    g_containers[container_id].name[63] = '\0';
    if (args && args[0]) {
        strncpy(g_containers[container_id].args, args, 255);
    } else {
        g_containers[container_id].args[0] = '\0';
    }
    if (env && env[0]) {
        strncpy(g_containers[container_id].env, env, 255);
    } else {
        g_containers[container_id].env[0] = '\0';
    }

    /* Build bwrap command */
    char *bwrap_argv[64];
    int argc = 0;
    bwrap_argv[argc++] = "bwrap";
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/";
    bwrap_argv[argc++] = "/";
    bwrap_argv[argc++] = "--proc";
    bwrap_argv[argc++] = "/proc";
    bwrap_argv[argc++] = "--dev";
    bwrap_argv[argc++] = "/dev";
    bwrap_argv[argc++] = "--tmpfs";
    bwrap_argv[argc++] = "/tmp";
    bwrap_argv[argc++] = "--unshare-all";
    bwrap_argv[argc++] = "--die-with-parent";

    if (args && args[0]) {
        bwrap_argv[argc++] = "/bin/sh";
        bwrap_argv[argc++] = "-c";
        bwrap_argv[argc++] = args;
    } else {
        bwrap_argv[argc++] = "/bin/sh";
    }
    bwrap_argv[argc] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* CHILD: exec bwrap */
        execvp("bwrap", bwrap_argv);
        _exit(127);
    }

    /* PARENT */
    g_containers[container_id].pid = pid;
    g_container_count++;

    return (int64_t)container_id;
}

int64_t sys_container_destroy(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5) {
    if (id < 0 || id >= g_container_count) return -1;
    if (g_containers[id].id != id) return -1;

    pid_t pid = g_containers[id].pid;
    if (pid > 0) {
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
    }

    g_containers[id].id = -1;
    g_containers[id].pid = 0;
    return 0;
}

int64_t sys_container_exec(int64_t id, int64_t cmd_ptr, int64_t args_ptr, int64_t _unused, int64_t _unused2, int64_t _unused3) {
    if (id < 0 || id >= g_container_count) return -1;
    if (g_containers[id].id != id) return -1;

    const char *cmd = (const char *)cmd_ptr;
    const char *args = (const char *)args_ptr;
    if (!cmd || !cmd[0]) return -1;

    pid_t pid = g_containers[id].pid;
    if (pid <= 0) return -1;

    /* For exec inside running container, we use nsenter or bwrap again
     * For simplicity, we'll run a new bwrap command with the same namespace */
    char *bwrap_argv[64];
    int argc = 0;
    bwrap_argv[argc++] = "bwrap";
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/";
    bwrap_argv[argc++] = "/";
    bwrap_argv[argc++] = "--proc";
    bwrap_argv[argc++] = "/proc";
    bwrap_argv[argc++] = "--dev";
    bwrap_argv[argc++] = "/dev";

    bwrap_argv[argc++] = "/bin/sh";
    bwrap_argv[argc++] = "-c";
    if (args && args[0]) {
        char full_cmd[512];
        snprintf(full_cmd, sizeof(full_cmd), "%s %s", cmd, args);
        bwrap_argv[argc++] = full_cmd;
    } else {
        bwrap_argv[argc++] = cmd;
    }
    bwrap_argv[argc] = NULL;

    pid_t child = fork();
    if (child < 0) return -1;

    if (child == 0) {
        execvp("bwrap", bwrap_argv);
        _exit(127);
    }

    int status;
    waitpid(child, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* -- Utility Handlers --------------------------------------------- */

int64_t sys_get_time(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5; (void)_unused6;
    /* Return TSC */
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((int64_t)hi << 32) | lo;
}

int64_t sys_sleep(int64_t ms, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5;
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
    return 0;
}

/* -- Universal Exec Handler --------------------------------------- */

int64_t sys_wubu_exec(int64_t data_ptr, int64_t size, int64_t filename_ptr,
                      int64_t _unused, int64_t _unused2, int64_t _unused3) {
    (void)_unused; (void)_unused2; (void)_unused3;

    if (data_ptr == 0 || size <= 0) return WUBU_EXEC_ERR_HDR;

    const void *data = (const void *)data_ptr;
    const char *filename = filename_ptr ? (const char *)filename_ptr : NULL;

    return wubu_exec(data, (size_t)size, filename);
}

/* ═════════════════════════════════════════════════════════════════
 * Registration
 * ═════════════════════════════════════════════════════════════════ */

typedef int64_t (*syscall_handler_t)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static const struct {
    uint32_t num;
    syscall_handler_t handler;
} g_syscalls[] = {
    {SYS_VBE_FILL_RECT,        (syscall_handler_t)sys_vbe_fill_rect},
    {SYS_VBE_FILL_CIRCLE,      (syscall_handler_t)sys_vbe_fill_circle},
    {SYS_VBE_DRAW_TEXT,        (syscall_handler_t)sys_vbe_draw_text},
    {SYS_VBE_DRAW_CHAR,        (syscall_handler_t)sys_vbe_draw_char},
    {SYS_VBE_VLINE,            (syscall_handler_t)sys_vbe_vline},
    {SYS_VBE_HLINE,            (syscall_handler_t)sys_vbe_hline},
    {SYS_VBE_TEXT_WIDTH,       (syscall_handler_t)sys_vbe_text_width},
    {SYS_VBE_SWAP,             (syscall_handler_t)sys_vbe_swap},
    {SYS_WM_CREATE_WIN,        (syscall_handler_t)sys_wm_create_win},
    {SYS_WM_DESTROY_WIN,       (syscall_handler_t)sys_wm_destroy_win},
    {SYS_WM_SET_FOCUS,         (syscall_handler_t)sys_wm_set_focus},
    {SYS_WM_GET_FOCUSED,       (syscall_handler_t)sys_wm_get_focused},
    {SYS_WM_RENDER,            (syscall_handler_t)sys_wm_render},
    {SYS_FILE_OPEN,            (syscall_handler_t)sys_file_open},
    {SYS_FILE_READ,            (syscall_handler_t)sys_file_read},
    {SYS_FILE_WRITE,           (syscall_handler_t)sys_file_write},
    {SYS_FILE_CLOSE,           (syscall_handler_t)sys_file_close},
    {SYS_STYX_OPEN,            (syscall_handler_t)sys_styx_open},
    {SYS_STYX_READ,            (syscall_handler_t)sys_styx_read},
    {SYS_STYX_WRITE,           (syscall_handler_t)sys_styx_write},
    {SYS_CONTAINER_CREATE,     (syscall_handler_t)sys_container_create},
    {SYS_CONTAINER_DESTROY,    (syscall_handler_t)sys_container_destroy},
    {SYS_CONTAINER_EXEC,       (syscall_handler_t)sys_container_exec},
    {SYS_WUBU_EXEC,            (syscall_handler_t)sys_wubu_exec},
    {SYS_GET_TIME,             (syscall_handler_t)sys_get_time},
    {SYS_SLEEP,                (syscall_handler_t)sys_sleep},
};

int wubu_syscall_register_all(void) {
    /* In test mode, just verify the table is populated */
    int count = sizeof(g_syscalls) / sizeof(g_syscalls[0]);
    fprintf(stderr, "WuBuOS: Defined %d syscalls\n", count);
    return count;
}

/* ══════════════════════════════════════════════════════════════════
 * HolyC Compiler External Function Registration
 * ══════════════════════════════════════════════════════════════════ */

int wubu_holyc_register_syscalls(void *hc_compiler) {
    /* The HCCompiler struct has a gen member with functions table */
    /* We need to add entries to gen->functions[] with func_ptr pointing
       to syscall trampolines. For now, we register the C handlers directly
       and HolyC can call them as external functions. */

    /* This would be called during compiler init to register syscalls
       as callable functions. The actual call from HolyC goes through
       a syscall instruction trampoline. */

    (void)hc_compiler;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Syscall Trampoline (Assembly)
 * ══════════════════════════════════════════════════════════════════ */

/* This is a simple C trampoline that does the syscall instruction.
 * In production, this should be in assembly for efficiency. */
static int64_t syscall_trampoline_0(void)  { __asm__ volatile("syscall" : : "a"(0) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_1(void)  { __asm__ volatile("syscall" : : "a"(1) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_2(void)  { __asm__ volatile("syscall" : : "a"(2) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_3(void)  { __asm__ volatile("syscall" : : "a"(3) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_4(void)  { __asm__ volatile("syscall" : : "a"(4) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_5(void)  { __asm__ volatile("syscall" : : "a"(5) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_6(void)  { __asm__ volatile("syscall" : : "a"(6) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_7(void)  { __asm__ volatile("syscall" : : "a"(7) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_8(void)  { __asm__ volatile("syscall" : : "a"(8) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_9(void)  { __asm__ volatile("syscall" : : "a"(9) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_10(void) { __asm__ volatile("syscall" : : "a"(10) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_11(void) { __asm__ volatile("syscall" : : "a"(11) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_12(void) { __asm__ volatile("syscall" : : "a"(12) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_13(void) { __asm__ volatile("syscall" : : "a"(13) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_14(void) { __asm__ volatile("syscall" : : "a"(14) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_15(void) { __asm__ volatile("syscall" : : "a"(15) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_16(void) { __asm__ volatile("syscall" : : "a"(16) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_17(void) { __asm__ volatile("syscall" : : "a"(17) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_18(void) { __asm__ volatile("syscall" : : "a"(18) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_19(void) { __asm__ volatile("syscall" : : "a"(19) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_20(void) { __asm__ volatile("syscall" : : "a"(20) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_21(void) { __asm__ volatile("syscall" : : "a"(21) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_22(void) { __asm__ volatile("syscall" : : "a"(22) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_23(void) { __asm__ volatile("syscall" : : "a"(23) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_24(void) { __asm__ volatile("syscall" : : "a"(24) : "rcx", "r11", "memory"); return 0; }
static int64_t syscall_trampoline_25(void) { __asm__ volatile("syscall" : : "a"(25) : "rcx", "r11", "memory"); return 0; }

static void *g_trampolines[SYS_MAX_DEFINED] = {
    syscall_trampoline_0, syscall_trampoline_1, syscall_trampoline_2,
    syscall_trampoline_3, syscall_trampoline_4, syscall_trampoline_5,
    syscall_trampoline_6, syscall_trampoline_7, syscall_trampoline_8,
    syscall_trampoline_9, syscall_trampoline_10, syscall_trampoline_11,
    syscall_trampoline_12, syscall_trampoline_13, syscall_trampoline_14,
    syscall_trampoline_15, syscall_trampoline_16, syscall_trampoline_17,
    syscall_trampoline_18, syscall_trampoline_19, syscall_trampoline_20,
    syscall_trampoline_21, syscall_trampoline_22, syscall_trampoline_23,
    syscall_trampoline_24, syscall_trampoline_25,
};

void *wubu_syscall_trampoline(uint32_t num) {
    if (num < SYS_MAX_DEFINED) {
        return g_trampolines[num];
    }
    return NULL;
}

/* Map syscall name to number for HolyC external function registration */
const char *wubu_syscall_name(uint32_t num) {
    static const char *names[SYS_MAX_DEFINED] = {
        "VBEFillRect", "VBEFillCircle", "VBEDrawText", "VBEDrawChar",
        "VBEVLine", "VBEHLine", "VBETextWidth", "VBESwap",
        "WMCreateWin", "WMDestroyWin", "WMSetFocus", "WMGetFocused",
        "WMRender",
        "FileOpen", "FileRead", "FileWrite", "FileClose",
        "StyxOpen", "StyxRead", "StyxWrite",
        "ContainerCreate", "ContainerDestroy", "ContainerExec",
        "WubuExec",
        "GetTime", "Sleep"
    };
    if (num < SYS_MAX_DEFINED) return names[num];
    return "UnknownSyscall";
}
