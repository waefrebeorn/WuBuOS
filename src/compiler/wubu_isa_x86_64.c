/*
 * wubu_isa_x86_64.c -- the x86-64 ISA driver.
 *
 * The driver space (wubu_isa_driver.h): consumes MIR, emits x86-64
 * machine code, runs it via mmap+JIT call. This is the SAME hourglass
 * neck the RISC-V driver consumes — one frontend, N backends.
 *
 * Strategy: each virtual register gets a fixed stack slot
 * [rbp - (vr+1)*8]. Operations load operands into rax (and rdi for
 * the second), compute, store the result. Simple, correct, portable
 * — the fast register allocator is a driver improvement, not a
 * frontend concern. Because operands are virtual registers, the
 * rdi-clobber bug class is impossible by construction.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* ---- the emitter ---- */

typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t frame;                /* stack frame bytes */
    size_t *label_offsets;       /* label id -> byte offset */
    size_t n_labels;
} x86_emitter_t;

static void e8(x86_emitter_t *e, uint8_t b) { if (e->n == e->cap) { e->cap = e->cap ? e->cap*2 : 256; e->code = realloc(e->code, e->cap); } e->code[e->n++] = b; }
static void e32(x86_emitter_t *e, uint32_t v) { e8(e, v & 0xFF); e8(e, (v >> 8) & 0xFF); e8(e, (v >> 16) & 0xFF); e8(e, (v >> 24) & 0xFF); }
static void e64(x86_emitter_t *e, uint64_t v) { for (int i = 0; i < 8; i++) e8(e, (v >> (8*i)) & 0xFF); }
static void rex(x86_emitter_t *e, int w, int r, int x, int b) { e8(e, 0x40 | (w<<3) | (r<<2) | (x<<1) | b); }

/* mov rax, [rbp - off]  (off is the NEGATIVE byte offset below rbp) */
static void emit_load_slot(x86_emitter_t *e, int32_t off) {
    if (off >= -128 && off <= 127) { rex(e,1,0,0,0); e8(e, 0x8B); e8(e, 0x45); e8(e, (uint8_t)off); }
    else { rex(e,1,0,0,0); e8(e, 0x8B); e8(e, 0x85); e32(e, (uint32_t)off); }
}
/* mov [rbp - off], rax */
static void emit_store_slot(x86_emitter_t *e, int32_t off) {
    if (off >= -128 && off <= 127) { rex(e,1,0,0,0); e8(e, 0x89); e8(e, 0x45); e8(e, (uint8_t)off); }
    else { rex(e,1,0,0,0); e8(e, 0x89); e8(e, 0x85); e32(e, (uint32_t)off); }
}

/* vr N lives at [rbp - (N+1)*8]: NEGATIVE, inside the allocated frame */
static int32_t slot_of(size_t frame, wubu_vr_t vr) { (void)frame; return -(int32_t)((vr + 1) * 8); }

static void emit_mov_rax_imm64(x86_emitter_t *e, int64_t imm) {
    rex(e,1,0,0,0); e8(e, 0xB8); e64(e, (uint64_t)imm);
}

/* label resolution: labels are recorded DURING emission; jumps are
 * patched in a second pass (handles forward + backward targets). */
typedef struct { size_t pos; uint32_t label; } x86_patch_t;

static void note_label(x86_emitter_t *e, uint32_t label, size_t off) {
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const x86_emitter_t *e, uint32_t label) {
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

/* record a jump position to be patched once labels are known */
static void x86_patch_push(x86_patch_t **patches, size_t *np, size_t *cap,
                           size_t pos, uint32_t lbl)
{
    if (*np == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *patches = realloc(*patches, *cap * sizeof(x86_patch_t));
    }
    (*patches)[*np].pos = pos;
    (*patches)[*np].label = lbl;
    (*np)++;
}

/* jmp rel32 (patched later); returns the patch position */
static size_t emit_jmp_placeholder(x86_emitter_t *e, uint8_t opcode) {
    e8(e, 0x0F); e8(e, opcode);   /* 0x84=jz 0x85=jnz 0x80=jo... we use jz only */
    size_t pos = e->n;
    e32(e, 0);
    return pos;
}

static int x86_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size)
{
    /* count the max vr to size the frame */
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->dst > max_vr) max_vr = in->dst;
        if (in->a > max_vr) max_vr = in->a;
        if (in->b > max_vr) max_vr = in->b;
    }
    x86_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = (max_vr + 1) * 8 + 16;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    /* patch list: jump positions to fix after labels are known */
    x86_patch_t *patches = NULL;
    size_t n_patches = 0, cap_patches = 0;
#define PATCH_PUSH(p, l) x86_patch_push(&patches, &n_patches, &cap_patches, (p), (l))

    /* prologue */
    e8(&e, 0x55);                      /* push rbp */
    rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xE5);  /* mov rbp, rsp */
    e8(&e, 0x48); e8(&e, 0x81); e8(&e, 0xEC); e32(&e, (uint32_t)e.frame); /* sub rsp, frame */

    /* emit (labels recorded as they are placed) */
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) {
            note_label(&e, in->label, e.n);
            continue;
        }
        switch (in->op) {
        case MIR_CONST:
            emit_mov_rax_imm64(&e, in->imm);
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        case MIR_MOV:
            emit_load_slot(&e, slot_of(e.frame, in->a));
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
        case MIR_AND: case MIR_OR: case MIR_XOR: case MIR_SHL: case MIR_SHR:
        {
            emit_load_slot(&e, slot_of(e.frame, in->a));
            e8(&e, 0x50);                                   /* push rax */
            emit_load_slot(&e, slot_of(e.frame, in->b));
            rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xC7);   /* mov rdi, rax */
            e8(&e, 0x58);                                   /* pop rax */
            switch (in->op) {
            case MIR_ADD: rex(&e,1,0,0,0); e8(&e, 0x01); e8(&e, 0xF8); break; /* add rax,rdi */
            case MIR_SUB: rex(&e,1,0,0,0); e8(&e, 0x29); e8(&e, 0xF8); break; /* sub rax,rdi */
            case MIR_MUL: rex(&e,1,0,0,0); e8(&e, 0x0F); e8(&e, 0xAF); e8(&e, 0xC7); break; /* imul rax,rdi */
            case MIR_DIV: /* cqo; idiv rdi */
                rex(&e,1,0,0,0); e8(&e, 0x99);
                rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xFF); break;
            case MIR_MOD: /* cqo; idiv rdi; mov rax,rdx */
                rex(&e,1,0,0,0); e8(&e, 0x99);
                rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xFF);
                rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xD0); break;
            case MIR_AND: rex(&e,1,0,0,0); e8(&e, 0x21); e8(&e, 0xF8); break;
            case MIR_OR:  rex(&e,1,0,0,0); e8(&e, 0x09); e8(&e, 0xF8); break;
            case MIR_XOR: rex(&e,1,0,0,0); e8(&e, 0x31); e8(&e, 0xF8); break;
            case MIR_SHL: /* mov rcx,rdi; shl rax,cl */
                rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xF9);
                rex(&e,1,0,0,0); e8(&e, 0xD3); e8(&e, 0xE0); break;
            case MIR_SHR:
                rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xF9);
                rex(&e,1,0,0,0); e8(&e, 0xD3); e8(&e, 0xE8); break;
            default: break;
            }
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        }
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE:
        {
            emit_load_slot(&e, slot_of(e.frame, in->a));
            e8(&e, 0x50);
            emit_load_slot(&e, slot_of(e.frame, in->b));
            rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xC7);
            e8(&e, 0x58);
            rex(&e,1,0,0,0); e8(&e, 0x39); e8(&e, 0xF8);   /* cmp rax,rdi */
            uint8_t cc;
            switch (in->op) {
            case MIR_EQ: cc = 0x94; break;   /* sete */
            case MIR_NE: cc = 0x95; break;   /* setne */
            case MIR_LT: cc = 0x9C; break;   /* setl (signed) */
            case MIR_LE: cc = 0x9E; break;   /* setle */
            case MIR_GT: cc = 0x9F; break;   /* setg */
            case MIR_GE: cc = 0x9D; break;   /* setge */
            default: cc = 0x94; break;
            }
            e8(&e, 0x0F); e8(&e, cc); e8(&e, 0xC0);        /* setcc al */
            rex(&e,1,0,0,0); e8(&e, 0x0F); e8(&e, 0xB6); e8(&e, 0xC0); /* movzx rax,al */
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        }
        case MIR_NEG:
            emit_load_slot(&e, slot_of(e.frame, in->a));
            rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xD8);   /* neg rax */
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        case MIR_NOT:
            emit_load_slot(&e, slot_of(e.frame, in->a));
            rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xD0);   /* not rax */
            emit_store_slot(&e, slot_of(e.frame, in->dst));
            break;
        case MIR_JMP:
            e8(&e, 0xE9);                       /* jmp rel32 */
            PATCH_PUSH(e.n, in->label);
            e32(&e, 0);
            break;
        case MIR_JZ:
        {
            emit_load_slot(&e, slot_of(e.frame, in->a));
            rex(&e,1,0,0,0); e8(&e, 0x85); e8(&e, 0xC0);   /* test rax,rax */
            e8(&e, 0x0F); e8(&e, 0x84);        /* jz rel32 */
            PATCH_PUSH(e.n, in->label);
            e32(&e, 0);
            break;
        }
        case MIR_RET:
            emit_load_slot(&e, slot_of(e.frame, in->a));
            e8(&e, 0xC9);                      /* leave */
            e8(&e, 0xC3);                      /* ret */
            break;
        }
    }

    /* end: fallback ret if no MIR_RET was emitted */
    if (e.n == 0 || e.code[e.n-1] != 0xC3) { e8(&e, 0xC9); e8(&e, 0xC3); }

    /* second pass: patch every jump to its label */
    for (size_t i = 0; i < n_patches; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        size_t pos = patches[i].pos;
        int32_t rel = (int32_t)(t - (pos + 4));
        e.code[pos] = (uint8_t)(rel & 0xFF);
        e.code[pos+1] = (uint8_t)((rel >> 8) & 0xFF);
        e.code[pos+2] = (uint8_t)((rel >> 16) & 0xFF);
        e.code[pos+3] = (uint8_t)((rel >> 24) & 0xFF);
    }
    free(patches);

    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

static int64_t x86_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    void *exec = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec == MAP_FAILED) return -1;
    memcpy(exec, code, size);
    __builtin___clear_cache((char *)exec, (char *)exec + size);
    int64_t (*fn)(void) = (int64_t (*)(void))exec;
    int64_t r = fn();
    munmap(exec, size);
    return r;
}

static void x86_describe(void)
{
    printf("x86-64 driver: native JIT, stack-slot register assignment, "
           "WUBU-ABI-v1 frame.\n");
}

const wubu_isa_driver_t wubu_isa_x86_64 = {
    .name = "x86-64",
    .family = "native",
    .exec = WUBU_ISA_NATIVE,
    .compile = x86_compile,
    .run = x86_run,
    .describe = x86_describe,
};
