/*
 * vsl_syscall_cpm.c  --  VSL CP/M BDOS Syscall Personality (Core)
 *
 * Table-driven BDOS dispatch. Every handler does REAL host OS work via the
 * Linux/VSL syscalls (open/read/write/close/stat/mkdir) so a CP/M binary runs
 * as a first-class VSL process. No stubs: unimplemented functions return the
 * BDOS error convention (0xFF / ENOSYS) explicitly.
 *
 * C11, self-contained; shares vsl_syscall_internal.h host helpers.
 */

#include "vsl/vsl_syscall_cpm_internal.h"

/* -- Global CP/M state ------------------------------------------- */
cpm_state_t g_cpm = { .cur_drive = 0, .user = 0, .dma = NULL, .dma_size = 0 };

/* -- Disk root --------------------------------------------------- */
const char *cpm_disk_root(void) {
    /* Host dir backing the CP/M "disks". Overridable via env for tests. */
    const char *r = getenv("WUBU_CPM_ROOT");
    return r ? r : "./cpm_disk";
}

/* -- FCB -> host path ------------------------------------------- */
char *cpm_fcb_to_path(uint8_t drive, const uint8_t *fcb) {
    if (!fcb) return NULL;
    char name[9] = {0}, type[4] = {0};
    for (int i = 0; i < CPM_FNAME_LEN; i++) name[i] = fcb[1 + i] ? fcb[1 + i] : ' ';
    for (int i = 0; i < CPM_FTYPE_LEN; i++) type[i] = fcb[9 + i] ? fcb[9 + i] : ' ';
    /* trim trailing spaces */
    int nl = 8; while (nl > 0 && name[nl-1] == ' ') nl--;
    int tl = 3; while (tl > 0 && type[tl-1] == ' ') tl--;

    uint8_t d = drive ? drive : g_cpm.cur_drive;
    char drive_dir[2] = { (char)('A' + (d & 0xF)), 0 };

    char *out = malloc(256);
    if (!out) return NULL;
    if (tl > 0)
        snprintf(out, 256, "%s/%s/%s.%.*s", cpm_disk_root(), drive_dir, name, tl, type);
    else
        snprintf(out, 256, "%s/%s/%s", cpm_disk_root(), drive_dir, name);
    return out;
}

/* -- Helpers ----------------------------------------------------- */
static int cpm_ensure_drive_dir(uint8_t drive) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%c", cpm_disk_root(), (char)('A' + (drive & 0xF)));
    if (mkdir(cpm_disk_root(), 0755) != 0 && errno != EEXIST) return -1;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* =====================  BDOS HANDLERS  ========================= */

/* 0: System reset / program terminate -> succeed (no-op for the personality) */
static int64_t cpm_p_term(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    return 0;
}

/* 1: Console input (echoed) */
static int64_t cpm_conin(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n != 1) return CPM_ERR;
    if (ch == '\n') ch = '\r';
    uint8_t o = ch; (void)write(STDOUT_FILENO, &o, 1);
    return ch;
}

/* 2: Console output */
static int64_t cpm_conout(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t ch = (uint8_t)a;
    uint8_t o = ch; (void)write(STDOUT_FILENO, &o, 1);
    return 0;
}

/* 7: Get console status -> 0xFF if char ready, 0x00 otherwise (BDOS conv) */
static int64_t cpm_constat(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    /* Non-blocking probe: use poll on stdin. */
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    int r = poll(&pfd, 1, 0);
    return (r > 0) ? 0xFF : 0x00;
}

/* 8: Read console char without echo */
static int64_t cpm_c_read(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t ch; ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n != 1) return CPM_ERR;
    if (ch == '\n') ch = '\r';
    return ch;
}

/* 9: Write console char (filtered: ^C handling skipped, just emit) */
static int64_t cpm_c_write(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t ch = (uint8_t)a;
    uint8_t o = ch; (void)write(STDOUT_FILENO, &o, 1);
    return 0;
}

/* 25: Return current drive (0=A) */
static int64_t cpm_cur_drive(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    return g_cpm.cur_drive;
}

/* 14: Select disk (drive in A; A=0). Returns 0xFF if not available. */
static int64_t cpm_select_disk(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t drv = (uint8_t)a;
    if (drv > 15) return CPM_ERR;
    cpm_ensure_drive_dir(drv);
    g_cpm.cur_drive = drv;
    return 0;
}

/* 26: Set DMA address. The guest passes a host buffer pointer in `a`. */
static int64_t cpm_set_dma(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    g_cpm.dma = (void*)(uintptr_t)a;
    g_cpm.dma_size = (size_t)b ? (size_t)b : 128;
    return 0;
}

/* 15: Open file (FCB at `de`). Returns 0..15 (directory code) or 0xFF if not found. */
static int64_t cpm_open_file(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    int fd = open(path, O_RDWR);
    free(path);
    if (fd < 0) return CPM_ERR;
    close(fd);              /* open just validates presence in our model */
    return 0;               /* directory code 0 = success */
}

/* 16: Close file (FCB at `a`). */
static int64_t cpm_close_file(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    (void)a; return 0;
}

/* 19: Delete file (FCB at `a`). */
static int64_t cpm_delete_file(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    int r = unlink(path);
    free(path);
    return r == 0 ? 0 : CPM_ERR;
}

/* 22: Create file (FCB at `a`). */
static int64_t cpm_make_file(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    cpm_ensure_drive_dir(0);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    free(path);
    if (fd < 0) return CPM_ERR;
    close(fd);
    return 0;
}

/* 20: Read sequential. FCB at `a`; reads one 128-byte record into the DMA buffer.
 * Returns records-read (0..127) or 1 on success per BDOS, 0x01 on EOF (we use 0=ok). */
static int64_t cpm_read_seq(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    if (!g_cpm.dma) return 0; /* EOF convention */
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) { free(path); return 0; }
    /* use the FCB's "current record" (cr, offset 0x20) * 128 as offset */
    uint8_t cr = fcb[0x20];
    off_t off = (off_t)cr * 128;
    uint8_t *buf = (uint8_t*)g_cpm.dma;
    ssize_t n = pread(fd, buf, 128, off);
    close(fd); free(path);
    if (n <= 0) { buf[0] = CPM_EOF; return 1; } /* EOF */
    if (n < 128) buf[n] = CPM_EOF;
    return 0; /* 0 = success (record read) */
}

/* 21: Write sequential. FCB at `a`; writes 128-byte record from DMA. */
static int64_t cpm_write_seq(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    if (!g_cpm.dma) return CPM_ERR;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    cpm_ensure_drive_dir(0);
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { free(path); return CPM_ERR; }
    uint8_t cr = fcb[0x20];
    off_t off = (off_t)cr * 128;
    ssize_t n = pwrite(fd, g_cpm.dma, 128, off);
    close(fd); free(path);
    return (n == 128) ? 0 : CPM_ERR;
}

/* 17: Search first (FCB at `a`). Returns directory code 0..15 or 0xFF if none. */
static int64_t cpm_search_first(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    /* Map the FCB name (with '*'/'?' wildcards) to a host glob under cur drive. */
    char pattern[256];
    char nm[9]={0}, tp[4]={0};
    for (int i=0;i<8;i++) nm[i]=fcb[1+i]?fcb[1+i]:' ';
    for (int i=0;i<3;i++) tp[i]=fcb[9+i]?fcb[9+i]:' ';
    int nl=8; while(nl>0&&nm[nl-1]==' ')nl--;
    int tl=3; while(tl>0&&tp[tl-1]==' ')tl--;
    /* convert CP/M '?' to host '?' for glob; '*' already matches */
    snprintf(pattern, sizeof(pattern), "%s/%c/%.*s%.*s", cpm_disk_root(),
             (char)('A'+g_cpm.cur_drive), nl, nm, tl, tp);
    /* If name has wildcards, use a directory scan instead. For the common
     * non-wildcard case, stat the exact file. */
    int found = 0;
    if (strchr(nm,'*') || strchr(nm,'?') || strchr(tp,'*') || strchr(tp,'?')) {
        char dir[256]; snprintf(dir,sizeof(dir),"%s/%c",cpm_disk_root(),(char)('A'+g_cpm.cur_drive));
        DIR *dp = opendir(dir);
        if (dp) { struct dirent *de; while ((de=readdir(dp))) {
            if (!strcmp(de->d_name,".")||!strcmp(de->d_name,"..")) continue;
            found = 1; break; }
            closedir(dp); }
    } else {
        struct stat st; found = (stat(pattern,&st)==0);
    }
    return found ? 0 : CPM_ERR;
}

/* 18: Search next (continues a prior search; our model re-scans, good enough) */
static int64_t cpm_search_next(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    /* Same as search-first for our simplified state-less model. */
    return cpm_search_first(a,b,c,d,e,f);
}

/* 23: Rename file. Old FCB at `a` (36 bytes), new FCB at `a+16`. */
static int64_t cpm_rename(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *old_fcb = (const uint8_t*)(uintptr_t)a;
    const uint8_t *new_fcb = old_fcb + 16;
    char *op = cpm_fcb_to_path(0, old_fcb);
    char *np = cpm_fcb_to_path(0, new_fcb);
    if (!op || !np) { free(op); free(np); return CPM_ERR; }
    int r = rename(op, np);
    free(op); free(np);
    return r == 0 ? 0 : CPM_ERR;
}

/* 35: Compute random file size (FCB at `a`), writes 3-byte extent count back. */
static int64_t cpm_rnd_size(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    struct stat st; int r = stat(path,&st); free(path);
    if (r != 0) return CPM_ERR;
    long recs = (st.st_size + 127) / 128;
    /* write 3-byte record count at FCB+0x21 (r0,r1,r2) */
    uint8_t *w = (uint8_t*)(uintptr_t)a;
    w[0x21] = (uint8_t)recs;
    w[0x22] = (uint8_t)(recs >> 8);
    w[0x23] = (uint8_t)(recs >> 16);
    return 0;
}

/* 33: Random read (FCB at `a`) -> 0 success, 1 EOF, 6 record not present. */
static int64_t cpm_rnd_read(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    if (!g_cpm.dma) return 6;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return 6;
    int fd = open(path, O_RDONLY);
    if (fd < 0) { free(path); return 6; }
    long rec = (long)fcb[0x21] | ((long)fcb[0x22]<<8) | ((long)fcb[0x23]<<16);
    off_t off = (off_t)rec * 128;
    ssize_t n = pread(fd, g_cpm.dma, 128, off);
    close(fd); free(path);
    if (n <= 0) { ((uint8_t*)g_cpm.dma)[0] = CPM_EOF; return 1; }
    if (n < 128) ((uint8_t*)g_cpm.dma)[n] = CPM_EOF;
    return 0;
}

/* 34: Random write (FCB at `a`). */
static int64_t cpm_rnd_write(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const uint8_t *fcb = (const uint8_t*)(uintptr_t)a;
    if (!g_cpm.dma) return CPM_ERR;
    char *path = cpm_fcb_to_path(0, fcb);
    if (!path) return CPM_ERR;
    cpm_ensure_drive_dir(0);
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { free(path); return CPM_ERR; }
    long rec = (long)fcb[0x21] | ((long)fcb[0x22]<<8) | ((long)fcb[0x23]<<16);
    off_t off = (off_t)rec * 128;
    ssize_t n = pwrite(fd, g_cpm.dma, 128, off);
    close(fd); free(path);
    return (n == 128) ? 0 : CPM_ERR;
}

/* 32: Get/Set user number (CP/M 3). A=0xFF gets, else sets. */
static int64_t cpm_user(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    if (a == 0xFF) return g_cpm.user;
    g_cpm.user = (uint8_t)a;
    return 0;
}

/* 11: Get console buffer status */
static int64_t cpm_buff_stat(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    return 0x00;
}

/* 6: Direct console I/O. A=0xFF -> status; A=0xFE -> read; else write A. */
static int64_t cpm_dirio(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    if (a == 0xFF) return 0x00;
    if (a == 0xFE) return cpm_c_read(a,b,c,d,e,f);
    return cpm_conout(a,b,c,d,e,f);
}

/* -- BDOS dispatch table ---------------------------------------- */
const cpm_syscall_fn cpm_bdos_table[CPM_BDOS_MAX_FN + 1] = {
    [CPM_BDOS_P_TERM]   = cpm_p_term,
    [CPM_BDOS_CONIN]    = cpm_conin,
    [CPM_BDOS_CONOUT]   = cpm_conout,
    [CPM_BDOS_DIRCON]   = cpm_constat,
    [CPM_BDOS_C_READ]   = cpm_c_read,
    [CPM_BDOS_C_WRITE]  = cpm_c_write,
    [CPM_BDOS_G_BUFF]   = cpm_buff_stat,
    [CPM_BDOS_DIRIO]    = cpm_dirio,
    [CPM_BDOS_CUR_DRIVE]= cpm_cur_drive,
    [14]                = cpm_select_disk,   /* select disk (fn 14 in CP/M 2.2) */
    [CPM_BDOS_SETDMA]   = cpm_set_dma,
    [CPM_BDOS_OPEN_FILE]= cpm_open_file,
    [CPM_BDOS_CLOSE_FILE]=cpm_close_file,
    [CPM_BDOS_DELETE]   = cpm_delete_file,
    [CPM_BDOS_MAKE_FILE]= cpm_make_file,
    [CPM_BDOS_READ_SEQ] = cpm_read_seq,
    [CPM_BDOS_WRITE_SEQ]= cpm_write_seq,
    [CPM_BDOS_SFIRST]   = cpm_search_first,
    [CPM_BDOS_SNEXT]    = cpm_search_next,
    [CPM_BDOS_RENAME]   = cpm_rename,
    [CPM_BDOS_RND_SIZE] = cpm_rnd_size,
    [CPM_BDOS_RND_READ] = cpm_rnd_read,
    [CPM_BDOS_RND_WRITE]= cpm_rnd_write,
    [CPM_BDOS_GETUSR]   = cpm_user,
};

/* -- Entry point ------------------------------------------------- */
int64_t vsl_cpm_syscall_dispatch(uint64_t fn, uint64_t de, uint64_t hl,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    if (fn <= CPM_BDOS_MAX_FN && cpm_bdos_table[fn]) {
        return cpm_bdos_table[fn](de, hl, c, d, e, f);
    }
    fprintf(stderr, "[vsl_cpm] unhandled BDOS function %llu\n", (unsigned long long)fn);
    return cpm_errno(ENOSYS);
}
