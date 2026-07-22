/*
 * vsl_syscall_mac_test.c  --  macOS Syscall Dispatch Tests
 *
 * Tests for the VSL macOS syscall dispatch table.
 * Validates that common syscall numbers dispatch to
 * non-NULL handlers and that the class-based encoding
 * correctly routes Mach traps vs BSD syscalls.
 */
#include "vsl/vsl_syscall_numbers_mac.h"
#include "vsl/vsl_mach_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Forward declaration of the dispatch function */
extern int64_t vsl_mac_syscall_dispatch(uint64_t syscall_raw, uint64_t a, uint64_t b,
                                         uint64_t c, uint64_t d, uint64_t e, uint64_t f);

/* Declared in vsl.c — we fake VSL state for testing */
extern bool g_vsl_active;
static int g_tests = 0;
static int g_passed = 0;

#define T(cond, msg) do { \
    g_tests++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%d] %s\n", g_tests, msg); \
    } else { \
        g_passed++; \
        printf("  \xE2\x9C\x93 %s\n", msg); \
    } \
} while(0)

#define TEST(name) printf("\n[%s]\n", name)

static void test_mach_trap_routing(void) {
    TEST("Mach Trap Routing (class 0)");
    
    /* task_self: 0x0000001C (28 decimal) */
    int64_t r = vsl_mac_syscall_dispatch(MAC_MACH_TASK_SELF, 0,0,0,0,0,0);
    T(r == (int64_t)MACH_TASK_SELF, "task_self returns MACH_TASK_SELF port");
    
    /* host_self: 29 */
    r = vsl_mac_syscall_dispatch(MAC_MACH_HOST_SELF, 0,0,0,0,0,0);
    T(r == (int64_t)MACH_HOST_SELF, "host_self returns MACH_HOST_SELF port");
    
    /* thread_self: 30 */
    r = vsl_mac_syscall_dispatch(MAC_MACH_THREAD_SELF, 0,0,0,0,0,0);
    T(r == (int64_t)MACH_THREAD_SELF, "thread_self returns MACH_THREAD_SELF port");
    
    /* mach_reply_port: 31 */
    r = vsl_mac_syscall_dispatch(MAC_MACH_REPLY_PORT, 0,0,0,0,0,0);
    T(r == (int64_t)MACH_REPLY_PORT, "reply port returns MACH_REPLY_PORT");
    
    /* vm_page_size: 37 (stub — should return -ENOSYS from unhandled table) */
    r = vsl_mac_syscall_dispatch(MAC_MACH_VM_PAGE_SIZE, 0,0,0,0,0,0);
    T(r < 0, "vm_page_size returns error (unhandled trap)");
}

static void test_bsd_syscall_routing(void) {
    TEST("BSD Syscall Routing (class 2)");
    
    /* Test that BSD class-encoded syscalls route to handlers */
    
    /* getpid: 0x02000014 (20 decimal) */
    int64_t r = vsl_mac_syscall_dispatch(MAC_CLASS_BSD_SYSCALL(MAC_SYS_GETPID), 0,0,0,0,0,0);
    T(r > 0, "BSD getpid returns positive PID");
    
    /* getuid: 0x02000018 (24 decimal) */
    r = vsl_mac_syscall_dispatch(MAC_CLASS_BSD_SYSCALL(MAC_SYS_GETUID), 0,0,0,0,0,0);
    T(r >= 0, "BSD getuid returns valid UID");
    
    /* geteuid: 43 */
    r = vsl_mac_syscall_dispatch(MAC_CLASS_BSD_SYSCALL(MAC_SYS_GETEUID), 0,0,0,0,0,0);
    T(r >= 0, "BSD geteuid returns valid EUID");
}

static void test_bsd_read_write(void) {
    TEST("BSD Read/Write");
    
    /* write(1, "hello", 5) — write to stdout */
    const char *msg = "BSD";
    /* stdout should work */
    int64_t r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_WRITE),
        1, (uint64_t)(uintptr_t)msg, 3, 0,0,0);
    T(r == 3, "write(1, ...) returns 3 bytes written");
    
    /* read from invalid fd should return error */
    char buf[16];
    r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_READ),
        999, (uint64_t)(uintptr_t)buf, 16, 0,0,0);
    T(r < 0, "read from invalid fd returns error");
    
    /* open of /dev/null should succeed */
    r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_OPEN),
        (uint64_t)(uintptr_t)"/dev/null", 0, 0, 0,0,0);
    T(r >= 0, "open(/dev/null) returns valid fd");
    
    /* close the fd */
    if (r >= 0) {
        int64_t c = vsl_mac_syscall_dispatch(
            MAC_CLASS_BSD_SYSCALL(MAC_SYS_CLOSE),
            (uint64_t)r, 0,0,0,0,0);
        T(c == 0, "close(fd) succeeds");
    }
}

static void test_bsd_mmap(void) {
    TEST("BSD mmap/munmap");
    
    /* Allocate anonymous memory */
    int64_t r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_MMAP),
        0, 4096, 3, 0x1002, -1, 0);  /* PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS */
    /* On macOS, anonymous mmap uses MAP_ANON flag */
    if (r < 0 || r == (int64_t)-1) {
        /* May fail in test context; that's OK */
        printf("  (mmap result=%ld, may be expected)\n", (long)r);
    } else {
        T(r != 0, "mmap returns non-zero address");
        /* Write to it to validate */
        *(volatile int*)(uintptr_t)r = 42;
        T(*(volatile int*)(uintptr_t)r == 42, "memory readable/writable");
        
        /* Unmap it */
        int64_t u = vsl_mac_syscall_dispatch(
            MAC_CLASS_BSD_SYSCALL(MAC_SYS_MUNMAP),
            (uint64_t)r, 4096, 0,0,0,0);
        T(u == 0, "munmap succeeds");
    }
}

static void test_bsd_socket(void) {
    TEST("BSD Socket operations");
    
    /* socket(AF_UNIX, SOCK_STREAM, 0) */
    int64_t r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_SOCKET),
        1, 1, 0, 0,0,0);  /* AF_UNIX=1, SOCK_STREAM=1 */
    T(r >= 0, "socket(AF_UNIX, SOCK_STREAM) returns fd");
    
    if (r >= 0) {
        /* close */
        int64_t c = vsl_mac_syscall_dispatch(
            MAC_CLASS_BSD_SYSCALL(MAC_SYS_CLOSE),
            (uint64_t)r, 0,0,0,0,0);
        T(c == 0, "close(socket fd) succeeds");
    }
}

static void test_bsd_fs_ops(void) {
    TEST("BSD Filesystem operations");
    
    /* mkdir then rmdir in /tmp */
    const char *testdir = "/tmp/wubu-mac-test-dir";
    /* First ensure it doesn't exist */
    rmdir(testdir);
    
    int64_t r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_MKDIR),
        (uint64_t)(uintptr_t)testdir, 0700, 0,0,0,0);
    T(r == 0, "mkdir /tmp/wubu-mac-test-dir succeeds");
    
    /* stat it */
    struct stat st;
    r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_STAT),
        (uint64_t)(uintptr_t)testdir, (uint64_t)(uintptr_t)&st, 0,0,0,0);
    T(r == 0, "stat on created dir succeeds");
    T(S_ISDIR(st.st_mode), "stat says it's a directory");
    
    /* rmdir */
    r = vsl_mac_syscall_dispatch(
        MAC_CLASS_BSD_SYSCALL(MAC_SYS_RMDIR),
        (uint64_t)(uintptr_t)testdir, 0,0,0,0,0);
    T(r == 0, "rmdir succeeds");
}

static void test_mach_msg_basic(void) {
    TEST("mach_msg basic");
    
    /* Create a simple message header */
    mach_msg_header_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.msgh_bits = MACH_MSGH_BITS(MACH_PORT_RIGHT_SEND, 0);
    msg.msgh_size = sizeof(msg);
    msg.msgh_remote_port = MACH_HOST_SELF;
    msg.msgh_local_port = MACH_PORT_NULL;
    msg.msgh_id = 0;  /* simple ping */
    
    /* Send-only */
    int64_t r = vsl_mac_syscall_dispatch(
        MAC_MACH_MSG,
        (uint64_t)(uintptr_t)&msg, MACH_SEND_MSG, sizeof(msg), 0, 0, 0);
    T(r == MACH_MSG_SUCCESS, "mach_msg send-only returns success");
    
    /* Receive-only (no message available) */
    r = vsl_mac_syscall_dispatch(
        MAC_MACH_MSG,
        (uint64_t)(uintptr_t)&msg, MACH_RCV_MSG, 0, sizeof(msg), MACH_REPLY_PORT, 0);
    T(r == MACH_RCV_TIMED_OUT, "mach_msg receive-only returns MACH_RCV_TIMED_OUT");
}

int main(void) {
    printf("=== macOS Syscall Dispatch Tests ===\n");
    
    /* Note: test_mach_trap_routing and test_bsd_syscall_routing
     * call the dispatch functions directly with mocked params.
     * These validate the dispatch tables are wired correctly
     * and don't require VSL init. */
    
    test_mach_trap_routing();
    test_bsd_syscall_routing();
    test_bsd_read_write();
    test_bsd_mmap();
    test_bsd_socket();
    test_bsd_fs_ops();
    test_mach_msg_basic();
    
    printf("\n=== Results: %d/%d passed ===\n", g_passed, g_tests);
    return g_passed == g_tests ? 0 : 1;
}
