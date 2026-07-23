/*
 * vsl_syscall_cpm_internal.h  --  VSL CP/M BDOS Syscall Personality (Internal)
 *
 * CP/M (1974, Digital Research) is the OLDEST "toast"/abandoned OS personality
 * WuBuOS adopts. Its BDOS (Basic Disk Operating System) is the direct ancestor of
 * DOS INT 21h. BDOS functions are invoked in 8-bit CP/M via `CALL 5` (C = fn,
 * DE = param) and in CP/M-86 via `INT 0E0h`. WuBuOS already emulates DOS; CP/M is
 * the older layer beneath it.
 *
 * Handlers are backed by the SAME Linux/VSL syscalls the DOS and POSIX layers use
 * (open/read/write/close/mmap/stat via vsl_sys_*), so CP/M binaries run as first
 * class VSL processes with real OS effects — no stubs.
 *
 * CP/M constraints reflected here:
 *   - 16-byte directory entries, 8.3 filenames (space-padded, not NUL-terminated)
 *   - Drive letters A:..P: mapped to ./cpm_disk/<drive>/
 *   - BDOS function numbers are stable across CP/M 1/2/3 (we implement the CP/M 2.2
 *     set, functions 0..44, plus the common CP/M 3 additions where trivial).
 */

#ifndef VSL_SYSCALL_CPM_INTERNAL_H
#define VSL_SYSCALL_CPM_INTERNAL_H

#include "vsl/vsl_syscall_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

/* -- BDOS function numbers (CP/M 2.2, the de-facto standard set) ---- */
#define CPM_BDOS_P_TERM       0   /* System reset / program terminate */
#define CPM_BDOS_CONIN        1   /* Console input (echo) */
#define CPM_BDOS_CONOUT       2   /* Console output */
#define CPM_BDOS_RDRIN        3   /* Reader input (aux) */
#define CPM_BDOS_PUNCH        4   /* Punch output (aux) */
#define CPM_BDOS_LIST         5   /* List output (printer) */
#define CPM_BDOS_DIRIO        6   /* Direct console I/O */
#define CPM_BDOS_DIRCON       7   /* Get console status */
#define CPM_BDOS_C_READ       8   /* Read char from console (no echo) */
#define CPM_BDOS_C_WRITE      9   /* Write char + filter (console) */
#define CPM_BDOS_R_BUFF      10   /* Read console buffer */
#define CPM_BDOS_G_BUFF      11   /* Get console buffer status */
#define CPM_BDOS_S_BUFF      12   /* Return list/console status */
#define CPM_BDOS_RNDSEED     13   /* Return random seed (CP/M 3) */
#define CPM_BDOS_SNDSEED     14   /* Set random seed (CP/M 3) */
#define CPM_BDOS_WARM        15   /* Warm start (deprecated) */
#define CPM_BDOS_LOGIN       16   /* Login (CP/M 3) */
#define CPM_BDOS_C_STAT      17   /* Console status (CP/M 3) */
#define CPM_BDOS_C_READ_E    18   /* Read console char w/ echo (CP/M 3) */
#define CPM_BDOS_C_WRITE_E   19   /* Write console char w/ echo (CP/M 3) */
#define CPM_BDOS_R_OBUFF     20   /* Read reader buffer (CP/M 3) */
#define CPM_BDOS_O_BUFF      21   /* Return output buffer status (CP/M 3) */
#define CPM_BDOS_MAKE         22  /* Make (select) disk */
#define CPM_BDOS_DFLT_FCB    23   /* Return default FCB (login vector) */
#define CPM_BDOS_DFLT_DMA    26   /* Return default DMA address (login vector) */
#define CPM_BDOS_BUFSIZ      24   /* Return BDOS buffer size (CP/M 3) */
#define CPM_BDOS_DRIVE        25  /* Return current drive (login vector) */
#define CPM_BDOS_SETDMA      26   /* Set DMA address */
#define CPM_BDOS_ALLOC       27   /* Get disk allocation info */
#define CPM_BDOS_SETCR        28  /* Set file attributes */
#define CPM_BDOS_ROLLF        29  /* Read random with zero fill (CP/M 3) */
#define CPM_BDOS_OPEN         15  /* (alias note: 15 is warm start; OPEN=15 in some docs) */
#define CPM_BDOS_OPEN_FCB    15   /* placeholder */
#define CPM_BDOS_RAWIO        31  /* Raw console I/O (CP/M 3) */
#define CPM_BDOS_SETSEC      32   /* Set BDOS user number (CP/M 3) */
#define CPM_BDOS_GETUSR      32   /* Get/set user number (CP/M 3 uses fn 32) */
#define CPM_BDOS_READ         33  /* (alias) */
#define CPM_BDOS_WRITE        34   /* (alias) */
/* The canonical file I/O functions (CP/M 2.2): */
#define CPM_BDOS_OPEN_FILE   15   /* Open file (FCB) */
#define CPM_BDOS_CLOSE_FILE  16   /* Close file (FCB) */
#define CPM_BDOS_SFIRST      17   /* Search first (FCB) */
#define CPM_BDOS_SNEXT       18   /* Search next */
#define CPM_BDOS_DELETE      19   /* Delete file (FCB) */
#define CPM_BDOS_READ_SEQ    20   /* Read sequential (FCB) */
#define CPM_BDOS_WRITE_SEQ   21   /* Write sequential (FCB) */
#define CPM_BDOS_MAKE_FILE   22   /* Create file (FCB) */
#define CPM_BDOS_RENAME      23   /* Rename file (FCB) */
#define CPM_BDOS_LOGIN_VEC   24   /* Return login vector */
#define CPM_BDOS_CUR_DRIVE   25   /* Return current drive */
#define CPM_BDOS_DMA_ADDR    26   /* Set/get DMA address */
#define CPM_BDOS_ALLOC_MAP   27   /* Get disk allocation map */
#define CPM_BDOS_SET_ATTR    28   /* Set file attributes */
#define CPM_BDOS_RND_READ    33   /* Random read (FCB) */
#define CPM_BDOS_RND_WRITE   34   /* Random write (FCB) */
#define CPM_BDOS_RND_SIZE    35   /* Compute file size (FCB) */
#define CPM_BDOS_RND_ZFILL   36   /* Random read with zero fill */

#define CPM_BDOS_MAX_FN      45   /* highest implemented function number */

/* -- FCB (File Control Block) layout (CP/M 2.2, 36 bytes) ---------- */
/* We parse the in-guest FCB buffer (pointed at by the DMA/drive arg). */
#define CPM_FCB_SIZE         36
#define CPM_FNAME_LEN         8
#define CPM_FTYPE_LEN         3

/* -- Error / return codes (BDOS convention: A=0xFF or 0xFFFF on error) */
#define CPM_ERR               0xFF   /* generic BDOS error (A register) */
#define CPM_ERR_FNF         0xFFFE  /* file not found */
#define CPM_EOF              0x1A   /* ^Z end-of-file marker */

/* -- Handler type (matches vsl_syscall_fn signature) -------------- */
typedef int64_t (*cpm_syscall_fn)(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f);

/* -- BDOS dispatch table (defined in vsl_syscall_cpm.c) ----------- */
extern const cpm_syscall_fn cpm_bdos_table[CPM_BDOS_MAX_FN + 1];

/* -- Entry point (called by the VSL facade when the CP/M class bit is set) */
int64_t vsl_cpm_syscall_dispatch(uint64_t fn, uint64_t de, uint64_t hl,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f);

/* -- FCB helpers (shared by file handlers) ----------------------- */
/* Parse an 8.3 filename from a CP/M FCB (dr byte + 8 + 3) into a NUL-terminated
 * host path under the CP/M disk root for the given drive letter. Returns the
 * malloc'd path (caller frees) or NULL. Honors the '*' and '?' wildcards by
 * mapping to the host '*' for search functions. */
char *cpm_fcb_to_path(uint8_t drive, const uint8_t *fcb);

/* Default CP/M disk root (a host dir); drive A: -> <root>/A/, etc. */
const char *cpm_disk_root(void);

/* DMA address state (CP/M programs set a 16-bit DMA offset + segment; we keep a
 * host pointer set via SETDMA). For the VSL personality we model the DMA as a
 * host buffer pointer the guest supplies through the dispatch registers. */
typedef struct {
    uint8_t cur_drive;     /* 0=A..15=P */
    uint8_t user;          /* CP/M 3 user number */
    void    *dma;          /* current DMA buffer (host pointer) */
    size_t  dma_size;
} cpm_state_t;

extern cpm_state_t g_cpm;

/* Map a BDOS error to a VSL errno-style negative return (ENOSYS for unimpl). */
static inline int64_t cpm_errno(int err) { return -(int64_t)err; }

#endif /* VSL_SYSCALL_CPM_INTERNAL_H */
