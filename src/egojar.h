/*
 * egojar.h -- the ego jar: every include flag in the project, in one file.
 *
 *   "e goes in the e jar, that's where the ego."
 *
 * This is a TROJAN HORSE SUIT. It looks like an ordinary header. It is the
 * only place in the tree that says POSIX, Linux, or gcc. Every other file
 * includes this and gets the whole world handed to it -- so the 5,000 files
 * that draw, encode, decode and animate never mention an OS by name.
 *
 * WHY: the goal is to build the same tree with wubunos instead of gcc. The
 * thing that blocks that is not logic, it is 520 #include <unistd.h> lines
 * and a handful of -D_POSIX_C_SOURCE flags scattered across the makefiles.
 * Put them all in here and the seam is exactly one file wide: reimplement
 * this header for WuBuOS (or let wubunos provide these names) and nothing
 * else moves.
 *
 * WHAT BELONGS HERE:
 *   - feature-test macros (they must precede every system header)
 *   - every POSIX / Linux / libc extension header the tree uses
 *   - nothing else. No declarations, no code, no static inline.
 *
 * WHAT DOES NOT: <stdio.h>, <string.h>, <stdint.h> -- standard C, any
 * compiler has them, they do not belong in a suit.
 *
 * Include it FIRST in any file that needs POSIX, because the feature-test
 * macros only take effect before the first system header.
 */

#ifndef WUBU_EGOJAR_H
#define WUBU_EGOJAR_H

/* ---- the flags (they must come before any system header) ------------- */
#if !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#  define _DEFAULT_SOURCE 1
#endif
#if !defined(_GNU_SOURCE)
#  define _GNU_SOURCE 1
#endif

/* ---- the gratification ---------------------------------------------- */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#include <sys/sendfile.h>
#include <sys/file.h>
#include <sys/sysmacros.h>
#include <sys/statfs.h>
#include <sys/fsuid.h>
#include <sys/uio.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <signal.h>
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <termios.h>
#include <getopt.h>
#include <strings.h>
#include <zlib.h>
#include <sys/mount.h>
#include <linux/mount.h>
#include <linux/fanotify.h>
#include <math.h>

#endif /* WUBU_EGOJAR_H */
