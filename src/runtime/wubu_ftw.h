/* wubu_ftw.h — WuBu-native nftw/dirent constants (POST-system-header)
 *
 * Include this AFTER <ftw.h> and <dirent.h> in source files that need
 * FTW_DEPTH, FTW_PHYS, DT_DIR, etc. without _GNU_SOURCE.
 *
 * The system <ftw.h> defines FTW_* as enum constants (not macros), so we
 * can't redefine them — but if <ftw.h> was NOT included (or didn't provide
 * them), we provide the ABI values ourselves.
 *
 * WHY THE FALLBACKS BELOW ARE ALWAYS ACTIVE (read before "simplifying"):
 * glibc exposes the FTW_* visit flags as ENUM CONSTANTS, not macros. An
 * `#ifndef FTW_DP` guard therefore ALWAYS evaluates true — the guard cannot
 * detect the enum — so these defines unconditionally shadow the platform enum.
 * That is safe ONLY because the values match glibc exactly. They are not
 * "a fallback for when the header is missing"; they are the values the
 * translation unit actually sees. Changing one without matching the platform
 * silently changes runtime behavior. That is precisely how FTW_DP came to be
 * 0 (== FTW_F): wubu_fs_rm_rf rmdir()'d regular files, got ENOTDIR, and
 * NtDeleteKey returned UNSUCCESSFUL. wubu_ftw_static_asserts() below pins the
 * layout so a future edit cannot reintroduce a silent mismatch.
 */

#ifndef WUBU_FTW_H
#define WUBU_FTW_H

/* Visit-flag values: must match the platform <ftw.h> enum exactly.
 *
 *   FTW_F=0 FTW_D=1 FTW_DNR=2 FTW_NS=3 FTW_SL=4 FTW_DP=5 FTW_SLN=6
 *
 * Every value is DISTINCT and nonzero for FTW_DP. wubu_fs_unlink_cb branches
 * on `typeflag == FTW_DP` to choose rmdir() over unlink(), so a collision
 * between FTW_DP and any file-ish flag turns every regular file into an rmdir
 * attempt. The static asserts in wubu_ftw_static_asserts() enforce that. */
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

/* Compile-time proof that the visit flags above are mutually distinct and
 * that FTW_DP is not a file-ish flag. If someone edits a value and it
 * collides, this FAILS THE BUILD instead of silently changing rm_rf at
 * runtime. Uses integer-constant-expression tricks so it works at file scope
 * in C11 without _Static_assert (which cannot appear at file scope). */
#define WUBU_FTW_CAT_(a, b) a##b
#define WUBU_FTW_CAT(a, b)  WUBU_FTW_CAT_(a, b)
/* Each unique value yields a distinct name; a collision makes two typedefs
 * name the same type, which is a redefinition error. */
typedef char wubu_ftw_unique_FTW_F  [FTW_F  == 0 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_D  [FTW_D  == 1 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_DNR[FTW_DNR == 2 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_NS [FTW_NS  == 3 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_SL [FTW_SL  == 4 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_DP [FTW_DP  == 5 ? 1 : -1];
typedef char wubu_ftw_unique_FTW_SLN[FTW_SLN == 6 ? 1 : -1];

#endif /* WUBU_FTW_H */
