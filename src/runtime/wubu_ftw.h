/* wubu_ftw.h — WuBu-native nftw/dirent constants (POST-system-header)
 *
 * Include this AFTER <ftw.h> and <dirent.h> in source files that need
 * FTW_DEPTH, FTW_PHYS, DT_DIR, etc. without _GNU_SOURCE.
 *
 * The system <ftw.h> defines FTW_* as enum constants (not macros),
 * so we can't redefine them — but if <ftw.h> was NOT included (or
 * didn't provide them), we provide the ABI values ourselves.
 */

#ifndef WUBU_FTW_H
#define WUBU_FTW_H

/* FTW_* visit flags (second callback arg) and nftw(2) control flags.
 *
 * glibc <ftw.h> exposes the visit flags as enum constants (NOT macros) under
 * __USE_XOPEN_EXTENDED:
 *     FTW_F=0 FTW_D=1 FTW_DNR=2 FTW_NS=3 FTW_SL=4 FTW_DP=5 FTW_SLN=6
 * and the nftw control flags as enum constants:
 *     FTW_PHYS=1 FTW_MOUNT=2 FTW_CHDIR=4 FTW_DEPTH=8
 *
 * Because enum constants are NOT macro symbols, a naive `#ifndef FTW_DP`
 * guard is ALWAYS true and would #define FTW_DP over the real value,
 * shadowing it. That previously set FTW_DP=0 (== FTW_F), making wubu_fs_rm_rf
 * rmdir() regular files, breaking NtDeleteKey (see test_vsl_nt). The values
 * below match glibc exactly so a fallback define never disagrees with the
 * platform enum, even when it shadows it. */

/* Visit-flag fallbacks: match glibc enum order/value exactly. */
#ifndef FTW_F
#  define FTW_F       0   /* Regular file */
#endif
#ifndef FTW_D
#  define FTW_D       1   /* Directory */
#endif
#ifndef FTW_DNR
#  define FTW_DNR     2   /* Unreadable directory */
#endif
#ifndef FTW_NS
#  define FTW_NS      3   /* Unstatable file */
#endif
#ifndef FTW_SL
#  define FTW_SL      4   /* Symbolic link */
#endif
#ifndef FTW_DP
#  define FTW_DP      5   /* Directory, all subdirs visited (post-order) */
#endif
#ifndef FTW_SLN
#  define FTW_SLN     6   /* Symbolic link naming non-existing file */
#endif

/* nftw(2) control-flag fallbacks: match glibc enum values. These ARE emitted
 * as macros by glibc when __USE_XOPEN_EXTENDED is set, so #ifndef correctly
 * skips the fallback in that case. */
#ifndef FTW_DEPTH
#  define FTW_DEPTH   8   /* Report files before directory itself (post-order) */
#endif
#ifndef FTW_PHYS
#  define FTW_PHYS    1   /* Don't follow symlinks */
#endif
#ifndef FTW_MOUNT
#  define FTW_MOUNT   2   /* Stay within same filesystem */
#endif
#ifndef FTW_CHDIR
#  define FTW_CHDIR   4   /* chdir to each dir before processing it */
#endif

/* dirent d_type values (from linux/dirent.h) */
#ifndef DT_UNKNOWN
#  define DT_UNKNOWN 0
#endif
#ifndef DT_FIFO
#  define DT_FIFO    1
#endif
#ifndef DT_CHR
#  define DT_CHR     2
#endif
#ifndef DT_DIR
#  define DT_DIR     4
#endif
#ifndef DT_BLK
#  define DT_BLK     6
#endif
#ifndef DT_REG
#  define DT_REG     8
#endif
#ifndef DT_LNK
#  define DT_LNK    10
#endif
#ifndef DT_SOCK
#  define DT_SOCK   12
#endif
#ifndef DT_WHT
#  define DT_WHT    14
#endif

#endif /* WUBU_FTW_H */
