/*
 * styx_names.c -- Styx/9P2000 message-name table + lookup.
 *
 * Self-contained: only depends on styx.h for the type range. Minimal
 * includes. Originally part of the styx.c monolith; split out so the
 * protocol's debug-name surface lives in its own translation unit.
 */

#include "styx.h"
#include <string.h>
#include <errno.h>

/* -- Message name table ------------------------------------------- */

static const char *g_msg_names[] = {
    [100] = "Tversion", [101] = "Rversion",
    [102] = "Tauth",    [103] = "Rauth",
    [104] = "Tattach",  [105] = "Rattach",
    [106] = "Terror",   [107] = "Rerror",
    [108] = "Tflush",   [109] = "Rflush",
    [110] = "Twalk",    [111] = "Rwalk",
    [112] = "Topen",    [113] = "Ropen",
    [114] = "Tcreate",  [115] = "Rcreate",
    [116] = "Tread",    [117] = "Rread",
    [118] = "Twrite",   [119] = "Rwrite",
    [120] = "Tclunk",   [121] = "Rclunk",
    [122] = "Tremove",  [123] = "Rremove",
    [124] = "Tstat",    [125] = "Rstat",
    [126] = "Twstat",   [127] = "Rwstat",
};

/* -- Message-name registry (the Revolver Doctrine) -----------------
 * The 9P2000 standard table above is the SEED; the live registry is a
 * runtime copy so extension message types (9P2000.e, custom server
 * dialects) can be named without a recompile. styx_msg_name reads the
 * LIVE copy, so a register is visible immediately. */
#define STYX_MSG_REGISTRY_MAX 256
static const char *g_styx_msg_registry[STYX_MSG_REGISTRY_MAX];
static int g_styx_msg_registry_seeded = 0;

static void styx_msg_registry_seed(void)
{
    if (g_styx_msg_registry_seeded) return;
    for (int i = 0; i < 128 && i < STYX_MSG_REGISTRY_MAX; i++)
        g_styx_msg_registry[i] = (i < 100 || i > 127) ? NULL : g_msg_names[i];
    g_styx_msg_registry_seeded = 1;
}

int styx_msg_name_register(uint8_t type, const char *name)
{
    if (!name) return -1;
    if (type >= STYX_MSG_REGISTRY_MAX) return -1;
    styx_msg_registry_seed();
    g_styx_msg_registry[type] = name;
    return 0;
}

/* -- errno -> Rerror ename (the namespace failure vocabulary) --------
 * NAMESPACE #590: every namespace failure a callback reports via errno
 * maps to stable 9P2000 ename text, so a client can show (and match on)
 * the REASON, not just the failure. Unmapped errnos get a fixed generic
 * string -- never NULL, never strerror()'s locale-dependent text. */
const char *styx_errno_ename(int err)
{
    switch (err) {
    case 0:           return "no error";
    case EPERM:
    case EACCES:      return "permission denied";
    case ENOENT:      return "no such file or directory";
    case EEXIST:      return "file already exists";
    case ENOTDIR:     return "not a directory";
    case EISDIR:      return "is a directory";
    case EROFS:       return "read-only filesystem";
    case ENOSPC:      return "no space left on device";
    case ENAMETOOLONG:return "file name too long";
    case ELOOP:       return "too many symbolic links";
    case EXDEV:       return "cross-device link";
    case ENOTEMPTY:   return "directory not empty";
    case EBUSY:       return "resource busy";
    case EINTR:       return "interrupted";
    case EAGAIN:      return "resource temporarily unavailable";
    case EBADF:       return "bad file descriptor";
    case EMFILE:      return "too many open files in process";
    case ENFILE:      return "too many open files in system";
    case EINVAL:      return "invalid argument";
    case EIO:         return "input/output error";
    case EOPNOTSUPP:  return "operation not supported";   /* ENOTSUP == EOPNOTSUPP on Linux */
    case EOVERFLOW:   return "value too large";
    default:          return "unknown error";
    }
}

const char *styx_msg_name(uint8_t type) {
    styx_msg_registry_seed();
    if (type < STYX_MSG_REGISTRY_MAX && g_styx_msg_registry[type])
        return g_styx_msg_registry[type];
    return "Unknown";
}
