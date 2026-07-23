/*
 * vsl_syscall_macclassic.c  --  VSL Classic Mac OS (68K) Trap Personality (Core)
 *
 * Table-driven A-line trap dispatch. Every handler does REAL host OS work
 * (malloc/free/open/read/write/close/clock) so a classic Mac binary runs as a
 * first-class VSL process. No stubs: unimplemented traps return paramErr (-1).
 *
 * C11, self-contained; reuses vsl_syscall_internal.h host helpers.
 */

#include "vsl/vsl_syscall_macclassic_internal.h"

/* =====================  TRAP HANDLERS  ========================= */

/* A11E NewPtr(size in D0) -> pointer (returned in result). */
static int64_t macc_newptr(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    size_t sz = (size_t)a;
    if (sz == 0) sz = 1;
    void *p = malloc(sz);
    return p ? (int64_t)(uintptr_t)p : macclassic_oserr(-1);
}

/* A01F DisposePtr(ptr in D0). */
static int64_t macc_disposeptr(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    void *p = (void*)(uintptr_t)a;
    if (!p) return macclassic_oserr(-1);
    free(p);
    return 0;
}

/* A11F GetPtrSize(ptr in D0) -> size. */
static int64_t macc_getptrsize(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    void *p = (void*)(uintptr_t)a;
    if (!p) return macclassic_oserr(-1);
    /* malloc_usable_size is glibc-specific; fall back to a sentinel. */
    size_t sz = malloc_usable_size(p);
    return (int64_t)sz;
}

/* A122 NewHandle(logicalSize in D0) -> handle (a pointer to a ptr). We model a
 * handle as a malloc'd slot holding the master pointer. */
static int64_t macc_newhandle(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    size_t sz = (size_t)a; if (sz == 0) sz = 1;
    void **h = (void**)malloc(sizeof(void*));
    if (!h) return macclassic_oserr(-1);
    *h = malloc(sz);
    if (!*h) { free(h); return macclassic_oserr(-1); }
    return (int64_t)(uintptr_t)h;
}

/* A021 DisposeHandle(h in D0). */
static int64_t macc_disposehandle(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    void **h = (void**)(uintptr_t)a;
    if (!h) return macclassic_oserr(-1);
    if (*h) free(*h);
    free(h);
    return 0;
}

/* A000 Open(namePtr in D0, refnum out). Returns OSErr; refnum in result low 16. */
static int64_t macc_open(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const char *name = (const char*)(uintptr_t)a;
    if (!name) return macclassic_oserr(-1);
    int fd = open(name, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return macclassic_oserr(-1);
    /* Encode the host fd as the Mac refNum (high 16 bits = 0, low 16 = fd+1). */
    return (int64_t)(uint16_t)(fd + 1);
}

/* A001 Close(refnum in D0). */
static int64_t macc_close(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    int ref = (int)(a & 0xFFFF);
    int fd = ref - 1;
    if (fd < 0) return macclassic_oserr(-1);
    return close(fd) == 0 ? 0 : macclassic_oserr(-1);
}

/* A002 Read(refnum D0, bufPtr D1, reqCount D2). Returns actual bytes read. */
static int64_t macc_read(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    int fd = (int)(a & 0xFFFF) - 1;
    void *buf = (void*)(uintptr_t)b;
    size_t cnt = (size_t)c;
    if (fd < 0 || !buf) return macclassic_oserr(-1);
    ssize_t n = read(fd, buf, cnt);
    return n >= 0 ? (int64_t)n : macclassic_oserr(-1);
}

/* A003 Write(refnum D0, bufPtr D1, reqCount D2). Returns actual bytes written. */
static int64_t macc_write(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    int fd = (int)(a & 0xFFFF) - 1;
    const void *buf = (const void*)(uintptr_t)b;
    size_t cnt = (size_t)c;
    if (fd < 0 || !buf) return macclassic_oserr(-1);
    ssize_t n = write(fd, buf, cnt);
    return n >= 0 ? (int64_t)n : macclassic_oserr(-1);
}

/* A0A1 Create(namePtr D0). */
static int64_t macc_create(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const char *name = (const char*)(uintptr_t)a;
    if (!name) return macclassic_oserr(-1);
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return macclassic_oserr(-1);
    close(fd);
    return 0;
}

/* A008 Delete(namePtr D0). */
static int64_t macc_delete(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    const char *name = (const char*)(uintptr_t)a;
    if (!name) return macclassic_oserr(-1);
    return unlink(name) == 0 ? 0 : macclassic_oserr(-1);
}

/* A0A4 GetFPos(refnum D0) -> file position. */
static int64_t macc_getfpos(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    int fd = (int)(a & 0xFFFF) - 1;
    if (fd < 0) return macclassic_oserr(-1);
    off_t pos = lseek(fd, 0, SEEK_CUR);
    return pos >= 0 ? (int64_t)pos : macclassic_oserr(-1);
}

/* A0A3 SetFPos(refnum D0, mode D1, offset D2). */
static int64_t macc_setfpos(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    int fd = (int)(a & 0xFFFF) - 1;
    if (fd < 0) return macclassic_oserr(-1);
    int whence = SEEK_SET;
    switch (b) {
        case 1: whence = SEEK_CUR; break;
        case 2: whence = SEEK_END; break;
        default: whence = SEEK_SET;
    }
    off_t pos = lseek(fd, (off_t)(int64_t)c, whence);
    return pos >= 0 ? 0 : macclassic_oserr(-1);
}

/* A975 TickCount -> ticks since startup (1 tick = 1/60 s). Use CLOCK_MONOTONIC. */
static int64_t macc_tickcount(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return (int64_t)(ms * 60 / 1000); /* ticks (60 Hz) */
}

/* A8F4 DrawChar(ch D0) -> emit to stdout (headless VSL host). */
static int64_t macc_drawchar(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    char ch = (char)(a & 0xFF);
    if (ch == '\r') ch = '\n';
    uint8_t o = (uint8_t)ch; (void)write(STDOUT_FILENO, &o, 1);
    return 0;
}

/* A88B WriteChar(ch D0). */
static int64_t macc_writechar(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    return macc_drawchar(a, b, c, d, e, f);
}

/* A88C WriteLn -> newline. */
static int64_t macc_writeln(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
    uint8_t nl = '\n'; (void)write(STDOUT_FILENO, &nl, 1);
    return 0;
}

/* -- Trap dispatch table ----------------------------------------- */
const macclassic_syscall_fn macclassic_trap_table[MACC_TRAP_MAX + 1] = {
    [MACC_OPEN]        = macc_open,
    [MACC_CLOSE]       = macc_close,
    [MACC_READ]        = macc_read,
    [MACC_WRITE]       = macc_write,
    [MACC_CREATE]      = macc_create,
    [MACC_DELETE]      = macc_delete,
    [MACC_GETFPOS]     = macc_getfpos,
    [MACC_SETFPos]     = macc_setfpos,
    [MACC_NEWPTR]      = macc_newptr,
    [MACC_DISPOSEPTR]  = macc_disposeptr,
    [MACC_GETPTRSIZE]  = macc_getptrsize,
    [MACC_NEWHANDLE]   = macc_newhandle,
    [MACC_DISPOSEHANDLE]=macc_disposehandle,
    [MACC_TICKCOUNT]   = macc_tickcount,
    [MACC_DRAWCHAR]    = macc_drawchar,
    [MACC_WRITECHAR]   = macc_writechar,
    [MACC_WRITE0]      = macc_writeln,
};

/* -- Entry point ------------------------------------------------- */
int64_t vsl_macclassic_syscall_dispatch(uint64_t trap, uint64_t a, uint64_t b,
                                         uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    if (trap <= MACC_TRAP_MAX && macclassic_trap_table[trap]) {
        return macclassic_trap_table[trap](a, b, c, d, e, f);
    }
    fprintf(stderr, "[vsl_macc] unhandled trap 0x%llx\n", (unsigned long long)trap);
    return macclassic_oserr(-1); /* paramErr */
}
