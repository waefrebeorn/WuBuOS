/*
 * vsl_syscall_internal.h  --  Internal header for VSL syscall modules
 * Shared declarations for syscall handler submodules.
 */

#ifndef VSL_SYSCALL_INTERNAL_H
#define VSL_SYSCALL_INTERNAL_H

#include "vsl/vsl_syscall_numbers.h"
#include "vsl/vsl_internal.h"
#include "vsl/vsl_syscall.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/select.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <dlfcn.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/landlock.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <sys/fanotify.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/random.h>
#include <sched.h>
#include <grp.h>

/* -- Global VSL state (extern) ----------------------------------- */
extern VSL_STATE g_vsl;

/* -- Helper function declarations --------------------------------- */
int vsl_get_host_fd(int vsl_fd);
int find_free_vsl_pid(void);
int register_child_pid(pid_t child_host_pid, uint32_t parent_vsl_pid);
int vsl_openat(int dirfd, const char *pathname, int flags, mode_t mode);

/* -- Process management (vsl_syscall_proc.c) --------------------- */
int64_t vsl_sys_nosys(uint64_t a, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_exit(uint64_t code, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getpid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getppid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fork(uint64_t a, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid,
                       uint64_t ctid, uint64_t tls, uint64_t f);
int64_t vsl_sys_vfork(uint64_t a, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_execve(uint64_t path, uint64_t argv, uint64_t envp,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_wait4(uint64_t pid, uint64_t status, uint64_t options,
                       uint64_t rusage, uint64_t e, uint64_t f);
int64_t vsl_sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_kill(uint64_t pid, uint64_t sig, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_exit_group(uint64_t code, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f);

/* -- Identity & credentials (vsl_syscall_proc.c) ----------------- */
int64_t vsl_sys_getuid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getgid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_geteuid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getegid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setuid(uint64_t uid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setgid(uint64_t gid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setreuid(uint64_t ruid, uint64_t euid, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setregid(uint64_t rgid, uint64_t egid, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getresuid(uint64_t ruid, uint64_t euid, uint64_t suid,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getresgid(uint64_t rgid, uint64_t egid, uint64_t sgid,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setresuid(uint64_t ruid, uint64_t euid, uint64_t suid,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setresgid(uint64_t rgid, uint64_t egid, uint64_t sgid,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getgroups(uint64_t size, uint64_t list, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setgroups(uint64_t size, uint64_t list, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setpgid(uint64_t pid, uint64_t pgid, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getpgid(uint64_t pid, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setsid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getsid(uint64_t pid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_umask(uint64_t mask, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);

/* -- System info (vsl_syscall_proc.c) ----------------------------- */
int64_t vsl_sys_uname(uint64_t buf, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_sysinfo(uint64_t info, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getrandom(uint64_t buf, uint64_t buflen, uint64_t flags,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getrlimit(uint64_t resource, uint64_t rlim, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setrlimit(uint64_t resource, uint64_t rlim, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_prlimit64(uint64_t pid, uint64_t resource, uint64_t new_limit,
                           uint64_t old_limit, uint64_t e, uint64_t f);
int64_t vsl_sys_alarm(uint64_t seconds, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_sched_yield(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f);

/* -- File I/O (vsl_syscall_fileio.c) ----------------------------- */
int64_t vsl_sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_open(uint64_t path, uint64_t flags, uint64_t mode,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_close(uint64_t fd, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_stat(uint64_t path, uint64_t buf, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fstat(uint64_t fd, uint64_t buf, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_access(uint64_t path, uint64_t mode, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_pipe(uint64_t pipefd, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_dup(uint64_t fd, uint64_t b, uint64_t c,
                     uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fsync(uint64_t fd, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_unlink(uint64_t path, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_mkdir(uint64_t path, uint64_t mode, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_rmdir(uint64_t path, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_rename(uint64_t oldpath, uint64_t newpath, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getcwd(uint64_t buf, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_chdir(uint64_t path, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_clock_gettime(uint64_t clk_id, uint64_t tp, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_pread64(uint64_t fd, uint64_t buf, uint64_t count,
                         uint64_t pos_low, uint64_t pos_high, uint64_t f);
int64_t vsl_sys_pwrite64(uint64_t fd, uint64_t buf, uint64_t count,
                          uint64_t pos_low, uint64_t pos_high, uint64_t f);
int64_t vsl_sys_readv(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_writev(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_truncate(uint64_t path, uint64_t length,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_ftruncate(uint64_t fd, uint64_t length,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_creat(uint64_t pathname, uint64_t mode,
                      uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_symlink(uint64_t target, uint64_t linkpath,
                         uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_readlink(uint64_t path, uint64_t buf, uint64_t bufsiz,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_chmod(uint64_t path, uint64_t mode,
                       uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fchmod(uint64_t fd, uint64_t mode,
                        uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_chown(uint64_t path, uint64_t owner, uint64_t group,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_lchown(uint64_t path, uint64_t owner, uint64_t group,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fchown(uint64_t fd, uint64_t owner, uint64_t group,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_lstat(uint64_t path, uint64_t buf, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_openat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                        uint64_t mode, uint64_t e, uint64_t f);
int64_t vsl_sys_newfstatat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
                            uint64_t flags, uint64_t e, uint64_t f);
int64_t vsl_sys_unlinkat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_faccessat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                           uint64_t flags, uint64_t e, uint64_t f);
int64_t vsl_sys_getdents64(uint64_t fd, uint64_t dirp, uint64_t count,
                            uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_statx(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                       uint64_t mask, uint64_t statxbuf, uint64_t f);
int64_t vsl_sys_poll(uint64_t ufds, uint64_t nfds, uint64_t timeout,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_epoll_create(uint64_t size, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_epoll_ctl(uint64_t epfd, uint64_t op, uint64_t fd,
                           uint64_t event, uint64_t e, uint64_t f);
int64_t vsl_sys_epoll_wait(uint64_t epfd, uint64_t events, uint64_t maxevents,
                            uint64_t timeout, uint64_t e, uint64_t f);

/* -- Memory management (vsl_syscall_memory.c) -------------------- */
int64_t vsl_sys_mmap(uint64_t addr, uint64_t size, uint64_t prot,
                      uint64_t flags, uint64_t fd, uint64_t offset);
int64_t vsl_sys_munmap(uint64_t addr, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_brk(uint64_t new_brk, uint64_t b, uint64_t c,
                     uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_msync(uint64_t addr, uint64_t len, uint64_t flags,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_mremap(uint64_t old_addr, uint64_t old_size, uint64_t new_size,
                        uint64_t flags, uint64_t new_addr, uint64_t f);
int64_t vsl_sys_madvise(uint64_t addr, uint64_t len, uint64_t advice,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_mlock(uint64_t addr, uint64_t len,
                       uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_munlock(uint64_t addr, uint64_t len,
                         uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_mlockall(uint64_t flags, uint64_t b, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_munlockall(uint64_t a, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_shmget(uint64_t key, uint64_t size, uint64_t flags,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_shmat(uint64_t shmid, uint64_t addr, uint64_t flags,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_shmdt(uint64_t addr, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_shmctl(uint64_t shmid, uint64_t cmd, uint64_t buf,
                        uint64_t d, uint64_t e, uint64_t f);

/* -- Signals (vsl_syscall_net.c) ---------------------------------- */
int64_t vsl_sys_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
                             uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_sigreturn(uint64_t a, uint64_t b, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_rt_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
                              uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_rt_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
                                uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_futex(uint64_t uaddr, uint64_t op, uint64_t val,
                      uint64_t timeout, uint64_t uaddr2, uint64_t val3);
int64_t vsl_sys_rt_sigsuspend(uint64_t mask, uint64_t d, uint64_t e,
                               uint64_t f, uint64_t g, uint64_t h);
int64_t vsl_sys_rt_sigpending(uint64_t set, uint64_t sigsetsize,
                               uint64_t d, uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_rt_sigtimedwait(uint64_t uthese, uint64_t uinfo, uint64_t uts,
                                 uint64_t usize, uint64_t e, uint64_t f);
int64_t vsl_sys_rt_sigqueueinfo(uint64_t pid, uint64_t sig, uint64_t uinfo,
                                 uint64_t d, uint64_t e, uint64_t f);

/* -- Sockets (vsl_syscall_net.c) --------------------------------- */
int64_t vsl_sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_socketpair(uint64_t domain, uint64_t type, uint64_t protocol,
                            uint64_t sv, uint64_t e, uint64_t f);
int64_t vsl_sys_connect(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_bind(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                      uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_listen(uint64_t sockfd, uint64_t backlog, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_accept(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_sendto(uint64_t sockfd, uint64_t buf, uint64_t len,
                        uint64_t flags, uint64_t dest_addr, uint64_t addrlen);
int64_t vsl_sys_recvfrom(uint64_t sockfd, uint64_t buf, uint64_t len,
                          uint64_t flags, uint64_t src_addr, uint64_t addrlen);
int64_t vsl_sys_shutdown(uint64_t sockfd, uint64_t how,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getsockname(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                             uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_getpeername(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                             uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setsockopt(uint64_t sockfd, uint64_t level, uint64_t optname,
                            uint64_t optval, uint64_t optlen, uint64_t f);

/* -- Timers (vsl_syscall_net.c) ---------------------------------- */
int64_t vsl_sys_timer_create(uint64_t clockid, uint64_t evp, uint64_t timerid,
                              uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_timer_settime(uint64_t timerid, uint64_t flags, uint64_t newval,
                               uint64_t oldval, uint64_t e, uint64_t f);
int64_t vsl_sys_timer_gettime(uint64_t timerid, uint64_t value,
                               uint64_t d, uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_timer_delete(uint64_t timerid, uint64_t d, uint64_t e,
                              uint64_t f, uint64_t g, uint64_t h);
int64_t vsl_sys_timerfd_create(uint64_t clockid, uint64_t flags,
                                uint64_t d, uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_timerfd_settime(uint64_t fd, uint64_t flags, uint64_t newval,
                                 uint64_t oldval, uint64_t e, uint64_t f);
int64_t vsl_sys_timerfd_gettime(uint64_t fd, uint64_t value,
                                 uint64_t d, uint64_t e, uint64_t f, uint64_t g);

/* -- Eventfd / inotify / signalfd (vsl_syscall_net.c) ------------- */
int64_t vsl_sys_eventfd(uint64_t count, uint64_t d, uint64_t e,
                         uint64_t f, uint64_t g, uint64_t h);
int64_t vsl_sys_eventfd2(uint64_t count, uint64_t flags,
                          uint64_t d, uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_inotify_init(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_inotify_add_watch(uint64_t fd, uint64_t pathname, uint64_t mask,
                                   uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_inotify_rm_watch(uint64_t fd, uint64_t wd,
                                  uint64_t d, uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_signalfd(uint64_t fd, uint64_t mask, uint64_t sizemask,
                          uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_signalfd4(uint64_t fd, uint64_t mask, uint64_t sizemask,
                           uint64_t flags, uint64_t e, uint64_t f);

/* -- Select / ppoll (vsl_syscall_net.c) -------------------------- */
int64_t vsl_sys_pselect6(uint64_t nfds, uint64_t readfds, uint64_t writefds,
                          uint64_t exceptfds, uint64_t timeout, uint64_t sigmask);
int64_t vsl_sys_ppoll(uint64_t fds, uint64_t nfds, uint64_t timeout,
                       uint64_t sigmask, uint64_t sigsetsize, uint64_t f);
int64_t vsl_sys_select(uint64_t nfds, uint64_t readfds, uint64_t writefds,
                        uint64_t exceptfds, uint64_t timeout, uint64_t f);

/* -- pipe2 / clone3 / io_uring (vsl_syscall_net.c) --------------- */
int64_t vsl_sys_pipe2(uint64_t pipefd, uint64_t flags, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_clone3(uint64_t cl_args, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_io_uring_setup(uint64_t entries, uint64_t params,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_io_uring_enter(uint64_t fd, uint64_t to_submit,
                                uint64_t min_complete, uint64_t flags,
                                uint64_t arg, uint64_t sz);
int64_t vsl_sys_io_uring_register(uint64_t fd, uint64_t opcode,
                                   uint64_t arg, uint64_t nr_args,
                                   uint64_t e, uint64_t f);

/* -- *AT family + namespace/security (vsl_syscall_net.c) --------- */
int64_t vsl_sys_readlinkat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
                            uint64_t bufsiz, uint64_t e, uint64_t f);
int64_t vsl_sys_fchmodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                          uint64_t flags, uint64_t e, uint64_t f);
int64_t vsl_sys_fchownat(uint64_t dirfd, uint64_t pathname, uint64_t owner,
                          uint64_t group, uint64_t flags, uint64_t f);
int64_t vsl_sys_utimensat(uint64_t dirfd, uint64_t pathname, uint64_t times,
                           uint64_t flags, uint64_t e, uint64_t f);
int64_t vsl_sys_futimesat(uint64_t dirfd, uint64_t pathname, uint64_t times,
                           uint64_t e, uint64_t f, uint64_t g);
int64_t vsl_sys_renameat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                          uint64_t newpath, uint64_t e, uint64_t f);
int64_t vsl_sys_mkdirat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_symlinkat(uint64_t target, uint64_t newdirfd,
                           uint64_t linkpath, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_linkat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                        uint64_t newpath, uint64_t flags, uint64_t f);
int64_t vsl_sys_mknodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                        uint64_t dev, uint64_t e, uint64_t f);
int64_t vsl_sys_getwd(uint64_t buf, uint64_t size, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fchdir(uint64_t fd, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_unshare(uint64_t flags, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_setns(uint64_t fd, uint64_t nstype, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fanotify_init(uint64_t flags, uint64_t event_f_flags,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f);
int64_t vsl_sys_fanotify_mark(uint64_t fanotify_fd, uint64_t flags, uint64_t mask,
                               uint64_t dirfd, uint64_t pathname, uint64_t f);
int64_t vsl_sys_landlock(uint64_t cmd, uint64_t attr, uint64_t flags,
                          uint64_t c, uint64_t d, uint64_t e);
int64_t vsl_sys_bpf(uint64_t cmd, uint64_t attr, uint64_t size,
                    uint64_t c, uint64_t d, uint64_t e);
int64_t vsl_sys_perf_event_open(uint64_t attr, uint64_t pid, uint64_t cpu,
                                 uint64_t group_fd, uint64_t flags, uint64_t f);

/* -- Syscall bridge (vsl_syscall.c facade) ----------------------- */
int64_t vsl_syscall(uint64_t num, uint64_t rdi, uint64_t rsi,
                    uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9);
int64_t vsl_syscall_dispatch(uint64_t num, uint64_t *regs);
void vsl_get_syscall_stats(uint64_t *out_count, uint64_t *out_errors);

#endif /* VSL_SYSCALL_INTERNAL_H */
