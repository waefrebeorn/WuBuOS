/*
 * brainfuck.c -- THE MEME FLAG: a real brainfuck -> x86-64 JIT.
 *
 * The user's joke (2026-08-04), shipped for real:
 *   - c11 luddites, we abstract away
 *   - C18/C2* allowed via `-c_developer`
 *   - any other language via `-i_make_shit_code`
 *   - and for the meme: brainfuck compiles via `-brainfuck`
 *     (because we ballin).
 *
 * Brainfuck is 8 commands:
 *   > < + - . , [ ]
 * We compile each to x86-64 directly: the tape lives in a calloc'd
 * buffer, the cell pointer in RBX, each `[`/`]` is a real loop with
 * a JNZ back-edge. No interpreter — real machine code, ring-0 ready.
 *
 * C11, self-contained. Compile with:
 *   gcc -O2 -Isrc/compiler -Isrc/jit -o brainfuck <the jit objects> brainfuck.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

/* mmap the emitted code as RWX (JIT pages) */
static void *mmap_code(uint8_t *code, size_t len)
{
    size_t page = 4096;
    size_t sz = (len + page - 1) & ~(page - 1);
    void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;
    memcpy(p, code, len);
    return p;
}

/* ---- the tiny x86-64 emitter (REX-prefixed, direct) ---------------- */

typedef struct {
    uint8_t *code;
    size_t cap, len;
} bf_code_t;

static void emit(bf_code_t *c, uint8_t b) {
    if (c->len >= c->cap) {
        c->cap = c->cap ? c->cap * 2 : 4096;
        c->code = (uint8_t *)realloc(c->code, c->cap);
        if (!c->code) { fprintf(stderr, "brainfuck: oom\n"); exit(1); }
    }
    c->code[c->len++] = b;
}

/* the cell pointer lives in RBX; the tape base in R12.
 * prologue: push rbx; push r12; mov rdi -> r12 (tape); xor ebx, ebx */
static void emit_prologue(bf_code_t *c) {
    emit(c, 0x53);                    /* push rbx */
    emit(c, 0x41); emit(c, 0x54);     /* push r12 */
    emit(c, 0x49); emit(c, 0x89); emit(c, 0xFC);  /* mov r12, rdi (tape) */
    emit(c, 0x31); emit(c, 0xDB);     /* xor ebx, ebx (cell = 0) */
}
static void emit_epilogue(bf_code_t *c) {
    emit(c, 0x31); emit(c, 0xC0);     /* xor eax, eax (return 0) */
    emit(c, 0x41); emit(c, 0x5C);     /* pop r12 */
    emit(c, 0x5B);                    /* pop rbx */
    emit(c, 0xC3);                    /* ret */
}

/* helpers for the loop patch queue */
typedef struct { size_t at; int is_open; } bf_patch_t;

/* > : inc rbx */
static void op_right(bf_code_t *c) { emit(c, 0x48); emit(c, 0xFF); emit(c, 0xC3); }
/* < : dec rbx */
static void op_left(bf_code_t *c)  { emit(c, 0x48); emit(c, 0xFF); emit(c, 0xCB); }
/* + : inc byte [r12+rbx] */
static void op_plus(bf_code_t *c)  { emit(c, 0x41); emit(c, 0xFE); emit(c, 0x04); emit(c, 0x1C); }
/* - : dec byte [r12+rbx] */
static void op_minus(bf_code_t *c) { emit(c, 0x41); emit(c, 0xFE); emit(c, 0x0C); emit(c, 0x1C); }
/* . : movzx eax, byte [r12+rbx]; push rdi; push rsi; ... call putchar */
static void op_out(bf_code_t *c) {
    emit(c, 0x41); emit(c, 0x0F); emit(c, 0xB6); emit(c, 0x04); emit(c, 0x1C); /* movzx eax, byte [r12+rbx] */
    emit(c, 0x50);                    /* push rax (arg) */
    emit(c, 0x48); emit(c, 0x89); emit(c, 0xC7);  /* mov rdi, rax */
    emit(c, 0x48); emit(c, 0xB8);     /* movabs rax, putchar */
    extern int putchar(int);
    uint64_t put = (uint64_t)(uintptr_t)&putchar;
    for (int i = 0; i < 8; i++) emit(c, (uint8_t)(put >> (8 * i)));
    emit(c, 0xFF); emit(c, 0xD0);     /* call rax */
    emit(c, 0x58);                    /* pop rax */
}
/* , : call getchar; mov byte [r12+rbx], al */
static void op_in(bf_code_t *c) {
    emit(c, 0x48); emit(c, 0xB8);     /* movabs rax, getchar */
    extern int getchar(void);
    uint64_t get = (uint64_t)(uintptr_t)&getchar;
    for (int i = 0; i < 8; i++) emit(c, (uint8_t)(get >> (8 * i)));
    emit(c, 0xFF); emit(c, 0xD0);     /* call rax */
    emit(c, 0x41); emit(c, 0x88); emit(c, 0x04); emit(c, 0x1C); /* mov byte [r12+rbx], al */
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <brainfuck-source>  (the meme flag: -brainfuck)\n", argv[0]);
        return 2;
    }
    const char *src = argv[1];
    size_t n = strlen(src);

    bf_code_t c = {0};
    emit_prologue(&c);

    bf_patch_t *patches = NULL;
    size_t np = 0, cp = 0;
#define PATCH_PUSH(p) do { \
    if (np >= cp) { cp = cp ? cp * 2 : 64; patches = (bf_patch_t *)realloc(patches, cp * sizeof(*patches)); if (!patches) { fprintf(stderr, "oom\n"); return 1; } } \
    patches[np++] = (p); } while (0)

    for (size_t i = 0; i < n; i++) {
        switch (src[i]) {
        case '>': op_right(&c); break;
        case '<': op_left(&c);  break;
        case '+': op_plus(&c);  break;
        case '-': op_minus(&c); break;
        case '.': op_out(&c);   break;
        case ',': op_in(&c);    break;
        case '[': {
            /* cmp byte [r12+rbx], 0; jz rel32 */
            emit(&c, 0x41); emit(&c, 0x80); emit(&c, 0x3C); emit(&c, 0x1C); emit(&c, 0x00);
            emit(&c, 0x0F); emit(&c, 0x84);
            emit(&c, 0); emit(&c, 0); emit(&c, 0); emit(&c, 0);
            bf_patch_t p = { c.len - 4, 1 };   /* the jz rel32 field */
            PATCH_PUSH(p);
            break;
        }
        case ']': {
            /* cmp byte [r12+rbx], 0; jnz rel32 */
            emit(&c, 0x41); emit(&c, 0x80); emit(&c, 0x3C); emit(&c, 0x1C); emit(&c, 0x00);
            emit(&c, 0x0F); emit(&c, 0x85);
            emit(&c, 0); emit(&c, 0); emit(&c, 0); emit(&c, 0);
            bf_patch_t p = { c.len - 4, 0 };   /* the jnz rel32 field */
            PATCH_PUSH(p);
            break;
        }
        default: break;   /* any other char is a comment (brainfuck) */
        }
    }
#undef PATCH_PUSH

    /* ---- patch pass: pair the jz (0F 84) and jnz (0F 85) by nesting
     * over the SOURCE brackets (emission order == source order) ---- */
    size_t *jz_at = (size_t *)malloc(sizeof(size_t) * (np + 1));
    size_t *jnz_at = (size_t *)malloc(sizeof(size_t) * (np + 1));
    size_t njz = 0, njnz = 0;
    for (size_t i = 0; i < c.len - 1; i++) {
        if (c.code[i] == 0x0F && c.code[i + 1] == 0x84) jz_at[njz++] = i + 2;
        if (c.code[i] == 0x0F && c.code[i + 1] == 0x85) jnz_at[njnz++] = i + 2;
    }
    /* map open ordinal -> close ordinal via a source bracket stack */
    size_t no = njz;                       /* # of '[' == # of jz */
    size_t *close_of_open = (size_t *)malloc(sizeof(size_t) * (no + 1));
    size_t *open_stack = (size_t *)malloc(sizeof(size_t) * (no + 1));
    size_t osp = 0, oo = 0, co = 0;
    for (size_t i = 0; i < n; i++) {
        if (src[i] == '[') open_stack[osp++] = oo++;
        else if (src[i] == ']') {
            if (osp > 0) { size_t o = open_stack[--osp]; close_of_open[o] = co; }
            co++;
        }
    }
    for (size_t o = 0; o < no; o++) {
        size_t jz_rel = jz_at[o];          /* the jz's rel32 field */
        size_t jnz_rel = jnz_at[close_of_open[o]];   /* matching jnz's rel32 */
        /* jz jumps PAST the matching jnz: target = jnz_rel + 4
         * (the jnz instruction ends at jnz_rel + 4) */
        int32_t t1 = (int32_t)((int64_t)(jnz_rel + 4) - (int64_t)(jz_rel + 4));
        /* jnz jumps BACK to the cmp before the jz: the jz is
         * 0F 84 (2 bytes) + rel (4) = 6 bytes, and the cmp before it
         * is 5 bytes, so the cmp starts at jz_rel - 2 - 5 = jz_rel - 7 */
        int32_t t2 = (int32_t)((int64_t)(jz_rel - 7) - (int64_t)(jnz_rel + 4));
        c.code[jz_rel + 0] = (uint8_t)((uint32_t)t1);
        c.code[jz_rel + 1] = (uint8_t)((uint32_t)t1 >> 8);
        c.code[jz_rel + 2] = (uint8_t)((uint32_t)t1 >> 16);
        c.code[jz_rel + 3] = (uint8_t)((uint32_t)t1 >> 24);
        c.code[jnz_rel + 0] = (uint8_t)((uint32_t)t2);
        c.code[jnz_rel + 1] = (uint8_t)((uint32_t)t2 >> 8);
        c.code[jnz_rel + 2] = (uint8_t)((uint32_t)t2 >> 16);
        c.code[jnz_rel + 3] = (uint8_t)((uint32_t)t2 >> 24);
    }
    free(close_of_open);
    free(open_stack);
    free(jz_at);
    free(jnz_at);

    emit_epilogue(&c);

    /* make it executable and RUN it (the meme, fulfilled) */
    size_t tape = 30000;
    void *tape_buf = calloc(tape, 1);
    if (!tape_buf) { fprintf(stderr, "oom\n"); return 1; }

    /* mmap the code as RWX */
    void *exec = mmap_code(c.code, c.len);
    if (!exec) { fprintf(stderr, "cannot mmap\n"); return 1; }

    typedef void (*bf_fn)(void *);
    bf_fn run = (bf_fn)exec;
    run(tape_buf);
    fflush(stdout);

    free(patches);
    free(c.code);
    free(tape_buf);
    return 0;
}
