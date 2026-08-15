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

/* FTW_* flags and return values (from glibc <ftw.h>) */
#ifndef FTW_DEPTH
#  define FTW_DEPTH   8   /* Post-order visit */
#endif
#ifndef FTW_PHYS
#  define FTW_PHYS    1   /* Don't follow symlinks */
#endif
#ifndef FTW_MOUNT
#  define FTW_MOUNT   2   /* Stay within same fs */
#endif
#ifndef FTW_CHDIR
#  define FTW_CHDIR   4   /* chdir before processing */
#endif
#ifndef FTW_DP
#  define FTW_DP      0   /* Dir, all subdirs visited */
#endif
#ifndef FTW_SL
#  define FTW_SL      1   /* Symlink */
#endif
#ifndef FTW_SLN
#  define FTW_SLN     2   /* Dangling symlink */
#endif
#ifndef FTW_D
#  define FTW_D       3   /* Directory */
#endif
#ifndef FTW_F
#  define FTW_F       5   /* Regular file */
#endif
#ifndef FTW_DNR
#  define FTW_DNR     6   /* Unreadable dir */
#endif
#ifndef FTW_NS
#  define FTW_NS      7   /* Unstatable file */
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
