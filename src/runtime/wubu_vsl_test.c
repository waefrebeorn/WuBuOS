/*
 * wubu_vsl_test.c  --  WuBuOS VSL (Virtualization Substrate Layer) Test Suite
 *
 * Tests the "Proton within Proton"  --  Linux ABI compatibility layer.
 */

#include "wubu_vsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s\n", msg); } \
} while(0)

int main(void) {
    printf("=== WuBuOS VSL Test Suite ===\n\n");

    /* -- Lifecycle -- */
    printf("[Lifecycle]\n");
    T(!vsl_active(), "VSL not active initially");
    T(vsl_init() == 0, "VSL init");
    T(vsl_active(), "VSL active after init");
    T(vsl_init() == 0, "VSL init idempotent");
    T(vsl_active(), "VSL still active");

    /* -- Process Management -- */
    printf("\n[Process Management]\n");
    T(vsl_get_process(1) != NULL, "init process (PID 1) exists");
    T(vsl_proc_get_pid(vsl_get_process(1)) == 1, "init PID is 1");
    T(vsl_proc_get_ppid(vsl_get_process(1)) == 0, "init PPID is 0");
    T(vsl_proc_get_state(vsl_get_process(1)) == VSL_PROC_READY, "init state is READY");
    T(vsl_get_process(999) == NULL, "non-existent PID returns NULL");

    /* Can't directly test VSL_PROC array with opaque type */
    /* int n = vsl_list_processes_processes_processes(procs, 32); */
    /* T(n == 1, "list processes returns 1 (init)"); */

    /* -- Syscall Bridge -- */
    printf("\n[Syscall Bridge]\n");

    int64_t r = vsl_syscall(VSL_SYS_GETPID, 0, 0, 0, 0, 0, 0);
    T(r == 1, "getpid() = 1");

    r = vsl_syscall(VSL_SYS_GETPPID, 0, 0, 0, 0, 0, 0);
    T(r == 0, "getppid() = 0");

    r = vsl_syscall(VSL_SYS_GETUID, 0, 0, 0, 0, 0, 0);
    T(r == 0, "getuid() = 0 (root)");

    r = vsl_syscall(VSL_SYS_GETGID, 0, 0, 0, 0, 0, 0);
    T(r == 0, "getgid() = 0 (root)");

    r = vsl_syscall(VSL_SYS_SCHED_YIELD, 0, 0, 0, 0, 0, 0);
    T(r == 0, "sched_yield() = 0");

    /* -- New Syscalls (clone3, io_uring) -- */
    printf("\n[New Syscalls]\n");

    /* Test clone3 - should return ENOSYS on kernels without clone3 support */
    r = vsl_syscall(VSL_SYS_CLONE3, 0, 0, 0, 0, 0, 0);
    T(r == -38 || r == -ENOSYS || r == -EINVAL, "clone3() returns ENOSYS/EINVAL (expected on some kernels)");

    /* Test io_uring_setup - should return EFAULT/EINVAL with NULL params */
    r = vsl_syscall(VSL_SYS_IO_URING_SETUP, 1, 0, 0, 0, 0, 0);
    T(r == -14 || r == -22 || r == -EFAULT || r == -EINVAL, "io_uring_setup() returns EFAULT/EINVAL with NULL params");

    /* Test io_uring_enter - should return EBADF with invalid fd */
    r = vsl_syscall(VSL_SYS_IO_URING_ENTER, -1, 0, 0, 0, 0, 0);
    T(r == -9 || r == -EBADF, "io_uring_enter() returns EBADF with invalid fd");

    /* Test io_uring_register - should return EBADF with invalid fd */
    r = vsl_syscall(VSL_SYS_IO_URING_REGISTER, -1, 0, 0, 0, 0, 0);
    T(r == -9 || r == -EBADF, "io_uring_register() returns EBADF with invalid fd");

    /* -- Memory Management -- */
    printf("\n[Memory Management]\n");
    int64_t brk_val = vsl_syscall(VSL_SYS_BRK, 0, 0, 0, 0, 0, 0);
    T(brk_val > 0, "brk(0) returns current brk");

    uint64_t mmap_addr = vsl_mmap(0, 4096, 0x3, 0x22, -1, 0);
    T(mmap_addr != 0, "mmap(4096) returns valid address");
    T(mmap_addr >= VSL_USER_BASE, "mmap address in user range");

    T(vsl_munmap(mmap_addr, 4096) == 0, "munmap() succeeds");
    T(vsl_munmap(mmap_addr, 4096) != 0, "double munmap fails");

    /* -- File Operations -- */
    printf("\n[File Operations]\n");
    int fd = vsl_open("/tmp/test.txt", 0x241, 0644);
    T(fd >= 3, "open() returns valid FD");
    T(vsl_close(fd) == 0, "close() succeeds");
    T(vsl_close(fd) != 0, "double close fails");
    T(vsl_close(0) != 0, "close(0) stdin fails");

    /* -- Seek Operations -- */
    printf("\n[Seek Operations]\n");
    int sfd = vsl_open("/tmp/vsl_lseek_test.txt", 0x241, 0644);
    T(sfd >= 3, "open() for seek test returns valid FD");

    /* Write some data first via host fd */
    const char *test_data = "Hello, VSL lseek!";
    int host_fd = -1;
    /* We can't directly access host fd from test, but we can test lseek on the VSL fd */
    /* Since vsl_open now opens real files, lseek should work */

    /* Test SEEK_SET: seek to beginning */
    int64_t pos = vsl_lseek(sfd, 0, SEEK_SET);
    T(pos == 0, "lseek(SEEK_SET, 0) returns 0");

    /* Test SEEK_CUR: seek forward */
    pos = vsl_lseek(sfd, 5, SEEK_CUR);
    T(pos == 5, "lseek(SEEK_CUR, 5) returns 5");

    /* Test SEEK_SET: seek to specific offset */
    pos = vsl_lseek(sfd, 10, SEEK_SET);
    T(pos == 10, "lseek(SEEK_SET, 10) returns 10");

    /* Test lseek on invalid fd */
    pos = vsl_lseek(999, 0, SEEK_SET);
    T(pos == -9, "lseek on invalid fd returns -9 (EBADF)");

    /* Test lseek on stdin (fd 0) - should work via host delegation */
    pos = vsl_lseek(0, 0, SEEK_SET);
    /* stdin is a real fd, lseek on it may fail (pipe) or succeed, just check it doesn't crash */
    (void)pos;

    T(vsl_close(sfd) == 0, "close seek test fd succeeds");

    /* Clean up test file */
    remove("/tmp/vsl_lseek_test.txt");

    /* -- Driver Management -- */
    printf("\n[Driver Management]\n");
    int drv = vsl_register_driver(VSL_DRV_GPU_VULKAN, 0, 0, 0, 0);
    T(drv >= 0, "register Vulkan driver");
    T(!vsl_driver_active(VSL_DRV_GPU_VULKAN), "Vulkan not active yet");
    T(vsl_activate_driver(drv) == 0, "activate Vulkan driver");
    T(vsl_driver_active(VSL_DRV_GPU_VULKAN), "Vulkan active");
    T(vsl_deactivate_driver(drv) == 0, "deactivate Vulkan");
    T(!vsl_driver_active(VSL_DRV_GPU_VULKAN), "Vulkan inactive");

    /* Register more drivers */
    int cuda = vsl_register_driver(VSL_DRV_GPU_CUDA, 0, 0, 0, 0);
    int net  = vsl_register_driver(VSL_DRV_NET, 0, 0, 0, 0);
    T(cuda >= 0, "register CUDA driver");
    T(net >= 0, "register NET driver");

    VSL_DRV *vk = vsl_get_driver(VSL_DRV_GPU_VULKAN);
    T(vk != NULL, "get Vulkan driver");
    T(vsl_drv_get_type(vk) == VSL_DRV_GPU_VULKAN, "Vulkan driver type correct");

    /* -- Shared Memory -- */
    printf("\n[Shared Memory]\n");
    T(vsl_send_cmd(42, 100) == 0, "send_cmd(42, 100)");
    T(vsl_get_status() == 1, "status = 1 (command pending)");

    /* -- ELF Validation -- */
    printf("\n[ELF Validation]\n");
    /* Minimal valid ELF64 header */
    uint8_t elf_header[] = {
        0x7F, 'E', 'L', 'F',  /* magic */
        2, 1, 1, 0,            /* 64-bit, little-endian, ELF v1 */
        0, 0, 0, 0, 0, 0, 0, 0, /* padding */
        2, 0,                  /* e_type = ET_EXEC */
        0x3E, 0,               /* e_machine = x86_64 */
        1, 0, 0, 0,            /* e_version */
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* e_entry = 0x40 */
        0, 0, 0, 0, 0, 0, 0, 0, /* e_phoff */
        0, 0, 0, 0, 0, 0, 0, 0, /* e_shoff */
        0, 0, 0, 0,             /* e_flags */
        0x40, 0,                /* e_ehsize */
        0x38, 0,                /* e_phentsize */
        0, 0,                   /* e_phnum */
        0, 0,                   /* e_shentsize */
        0, 0,                   /* e_shnum */
        0, 0,                   /* e_shstrndx */
    };

    uint64_t entry;
    T(vsl_elf_validate(elf_header, sizeof(elf_header), &entry) == 0,
      "validate minimal ELF64");
    T(entry == 0x40, "ELF entry point = 0x40");

    /* Invalid ELF */
    uint8_t bad_elf[] = {0x00, 0x00, 0x00, 0x00};
    T(vsl_elf_validate(bad_elf, sizeof(bad_elf), NULL) != 0,
      "reject invalid ELF");

    /* Wrong machine (ARM) */
    uint8_t arm_elf[64];
    memcpy(arm_elf, elf_header, sizeof(elf_header));
    arm_elf[18] = 0x28; /* EM_ARM */
    arm_elf[19] = 0x00;
    T(vsl_elf_validate(arm_elf, sizeof(arm_elf), NULL) != 0,
      "reject ARM ELF");

    /* -- Diagnostics -- */
    printf("\n[Diagnostics]\n");
    char info[512];
    vsl_info(info, sizeof(info));
    T(strlen(info) > 0, "vsl_info() returns non-empty string");

    uint64_t syscalls, errors;
    uint32_t procs_count, drivers_count;
    vsl_get_stats(&syscalls, &errors, &procs_count, &drivers_count);
    T(syscalls > 0, "syscall count > 0");
    T(procs_count >= 1, "process count >= 1");
    T(drivers_count == 3, "driver count = 3");

    /* -- Shutdown -- */
    printf("\n[Shutdown]\n");
    vsl_shutdown();
    T(!vsl_active(), "VSL not active after shutdown");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
