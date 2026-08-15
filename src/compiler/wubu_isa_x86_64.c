/*
 * wubu_isa_x86_64.c -- the x86-64 ISA driver.
 *
 * The driver space (wubu_isa_driver.h): consumes MIR, emits x86-64
 * machine code, runs it via mmap+JIT call. This is the SAME hourglass
 * neck the RISC-V driver consumes — one frontend, N backends.
 *
 * Strategy: the MIR register allocator (wubu_mir_regalloc.h) assigns
 * each virtual register to either a physical register or a stack slot.
 * Register-resident vrs live in their assigned x86-64 register for
 * their entire lifetime — no load/store traffic. Spilled vrs use
 * [rbp - offset] like the stack-only driver.
 *
 * Physical register map (14 total, indices 0..13):
 *   0=rax  1=r10  2=r11  3=r12  4=r13  5=r14  6=r15
 *   7=rbx  8=r8   9=r9   10=rdx 11=rsi 12=r10 13=rdi
 *
 * Note: rcx is NOT in the allocator pool — it's used implicitly by
 * shl/shr (which need cl). rdi is index 13, used as second-operand
 * scratch only when NOT holding a live vr.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include "wubu_mir_regalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* Peephole optimizer — declared in x86_peephole.c */
extern size_t x86_peephole_optimize(uint8_t *code, size_t n);

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

/* x86-64 register encoding for our 10 physical registers.
 * Index is the allocator's physical register number.
 * We skip rax(0), rcx(1), rdi(7), rbp(5), rsp(4).
 * rax is the implicit accumulator — not in the allocator pool.
 * rdi is used as second-operand scratch.
 * rcx is used for shift counts only. */
static const int reg_x86[10] = {
    10,  /* 0: r10 */
    11,  /* 1: r11 */
    12,  /* 2: r12 */
    13,  /* 3: r13 */
    14,  /* 4: r14 */
    15,  /* 5: r15 */
    3,   /* 6: rbx (callee-saved) */
    8,   /* 7: r8  */
    9,   /* 8: r9  */
    2,   /* 9: rdx */
};

/* does this x86 encoding need REX.B or REX.R? */
static inline int reg_needs_rex(int r) { return r >= 8; }

/* emit: mov dst_reg, src_reg (both are x86 encodings 0-15) */
static void emit_mov_reg(x86_emitter_t *e, int dst, int src) {
    if (dst == src) return;
    rex(e, 1, reg_needs_rex(src), 0, reg_needs_rex(dst));
    e8(e, 0x89);
    e8(e, (uint8_t)(0xC0 | ((src & 7) << 3) | (dst & 7)));
}

/* emit: mov dst_reg, [rbp + off]  (off is signed) */
static void emit_load_rbp(x86_emitter_t *e, int dst, int32_t off) {
    rex(e, 1, reg_needs_rex(dst), 0, 0);
    if (off >= -128 && off <= 127) { e8(e, 0x8B); e8(e, (uint8_t)(0x45 | ((dst & 7) << 3))); e8(e, (uint8_t)off); }
    else { e8(e, 0x8B); e8(e, (uint8_t)(0x85 | ((dst & 7) << 3))); e32(e, (uint32_t)off); }
}

/* emit: mov [rbp + off], src_reg */
static void emit_store_rbp(x86_emitter_t *e, int32_t off, int src) {
    rex(e, 1, reg_needs_rex(src), 0, 0);
    if (off >= -128 && off <= 127) { e8(e, 0x89); e8(e, (uint8_t)(0x45 | ((src & 7) << 3))); e8(e, (uint8_t)off); }
    else { e8(e, 0x89); e8(e, (uint8_t)(0x85 | ((src & 7) << 3))); e32(e, (uint32_t)off); }
}

/* Spilled vr slot: slot N -> [rbp - (N+1)*8] */
static int32_t spill_offset(int spill_slot) {
    return -(int32_t)((spill_slot + 1) * 8);
}

/* Load vr into a specific x86 register (encoded) */
static void emit_load_vr_to(x86_emitter_t *e, wubu_vr_t vr,
                             const wubu_reg_assign_t *assign, int dst_enc) {
    if (vr < 0) return;
    uint32_t vru = (uint32_t)vr;
    /* find assignment — we need the assign array indexed by vr */
    /* assign is indexed 0..assign_count-1; we pass count separately */
    (void)assign; (void)dst_enc; /* placeholder — see full impl below */
}

/* ---- label resolution ---- */
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
static void x86_patch_push(x86_patch_t **patches, size_t *np, size_t *cap,
                           size_t pos, uint32_t lbl) {
    if (*np == *cap) { *cap = *cap ? *cap * 2 : 16; *patches = realloc(*patches, *cap * sizeof(x86_patch_t)); }
    (*patches)[*np].pos = pos;
    (*patches)[*np].label = lbl;
    (*np)++;
}

/* ---- the full compile function with regalloc ---- */

static int x86_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size) {
    /* Step 1: call the MIR register allocator */
    size_t assign_count = 0;
    wubu_reg_assign_t *assign = wubu_mir_alloc_regs(p, 10, &assign_count);
    if (!assign) return -1;

    /* Count spilled vrs to size the frame */
    size_t n_spilled = 0;
    for (size_t i = 0; i < assign_count; i++) {
        if (assign[i].reg < 0) {
            int slot = -assign[i].reg - 1;
            if ((size_t)slot >= n_spilled) n_spilled = slot + 1;
        }
    }

    x86_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = n_spilled * 8;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    x86_patch_t *patches = NULL;
    size_t n_patches = 0, cap_patches = 0;
#define PATCH_PUSH(pos, lbl) x86_patch_push(&patches, &n_patches, &cap_patches, (pos), (lbl))

    /* prologue */
    e8(&e, 0x55);                      /* push rbp */
    rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xE5);  /* mov rbp, rsp */
    if (e.frame > 0) {
        e8(&e, 0x48); e8(&e, 0x81); e8(&e, 0xEC); e32(&e, (uint32_t)e.frame);
    }

    /* Helper: get x86 encoding for vr (returns -1 if spilled) */
    #define VR_ENC(vr) ((vr) < (wubu_vr_t)assign_count && assign[(vr)].reg >= 0 ? reg_x86[assign[(vr)].reg] : -1)
    #define VR_SPILL(vr) ((vr) < (wubu_vr_t)assign_count && assign[(vr)].reg < 0 ? spill_offset(-assign[(vr)].reg - 1) : 0)
    /* Lookahead: is the next instruction a RET that reads this vr? */
    #define NEXT_IS_RET(vr) (i + 1 < p->n && p->ins[i+1].op == MIR_RET && p->ins[i+1].a == (wubu_vr_t)(vr))

    int result_in_rax = 0;  /* set when last op skipped store to keep result in rax */

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n); result_in_rax = 0; continue; }
        if (in->op != MIR_RET) result_in_rax = 0;  /* reset unless RET handles it */

        switch (in->op) {
        case MIR_CONST: {
            int64_t imm = in->imm;
            int dst_enc = VR_ENC(in->dst);
            if (dst_enc >= 0) {
                /* mov reg, imm — use 32-bit encoding when possible */
                if (imm >= -2147483648LL && imm <= 2147483647LL) {
                    uint32_t imm32 = (uint32_t)((int32_t)imm);
                    if (dst_enc >= 8) { e8(&e, 0x41); e8(&e, (uint8_t)(0xB8 + (dst_enc & 7))); }
                    else { e8(&e, (uint8_t)(0xB8 + dst_enc)); }
                    e32(&e, imm32);
                } else {
                    /* Full 64-bit immediate */
                    if (dst_enc >= 8) rex(&e,1,0,0,reg_needs_rex(dst_enc)); else rex(&e,1,0,0,0);
                    e8(&e, (uint8_t)(0xB8 + (dst_enc & 7)));
                    e64(&e, (uint64_t)imm);
                }
            } else {
                /* spilled: mov rax, imm; mov [rbp+off], rax */
                if (imm >= -2147483648LL && imm <= 2147483647LL) {
                    uint32_t imm32 = (uint32_t)((int32_t)imm);
                    e8(&e, 0xB8); e32(&e, imm32);
                } else {
                    rex(&e,1,0,0,0); e8(&e, 0xB8); e64(&e, (uint64_t)imm);
                }
                emit_store_rbp(&e, VR_SPILL(in->dst), 0);
            }
            break;
        }
        case MIR_MOV: {
            int sa = VR_ENC(in->a), sd = VR_ENC(in->dst);
            if (sd >= 0) {
                /* dest is a register */
                if (sa >= 0) {
                    emit_mov_reg(&e, sd, sa);  /* mov dst_reg, src_reg */
                } else {
                    emit_load_rbp(&e, sd, VR_SPILL(in->a));  /* mov dst_reg, [rbp+off] */
                }
            } else {
                /* dest is spilled */
                if (sa >= 0) {
                    emit_store_rbp(&e, VR_SPILL(in->dst), sa);
                } else {
                    emit_load_rbp(&e, 0, VR_SPILL(in->a));  /* mov rax, [rbp+off_a] */
                    emit_store_rbp(&e, VR_SPILL(in->dst), 0);  /* mov [rbp+off_dst], rax */
                }
            }
            break;
        }
        case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
        case MIR_AND: case MIR_OR: case MIR_XOR: {
            /* Load 'a' into rax (accumulator) */
            int sa = VR_ENC(in->a);
            if (sa == 0) {
                /* already in rax — nothing to do */
            } else if (sa >= 0) {
                emit_mov_reg(&e, 0, sa);  /* mov rax, src_reg */
            } else {
                emit_load_rbp(&e, 0, VR_SPILL(in->a));
            }
            /* Load 'b' into rdi (second operand) */
            int sb = VR_ENC(in->b);
            if (sb == 7) {
                /* already in rdi (x86 encoding 7) — wait, rdi is encoding 7? No. */
                /* rdi x86 encoding is 7. But our reg_x86[] maps allocator index -> x86. */
                /* We need to emit mov rdi, <sb_enc> */
            }
            /* Actually, let's use rdi (x86 enc 7) as second-operand scratch */
            if (sb >= 0) {
                emit_mov_reg(&e, 7, sb);  /* mov rdi, b_reg */
            } else {
                emit_load_rbp(&e, 7, VR_SPILL(in->b));
            }
            switch (in->op) {
            case MIR_ADD: rex(&e,1,0,0,0); e8(&e, 0x01); e8(&e, 0xF8); break; /* add rax,rdi */
            case MIR_SUB: rex(&e,1,0,0,0); e8(&e, 0x29); e8(&e, 0xF8); break; /* sub rax,rdi */
            case MIR_MUL: rex(&e,1,0,0,0); e8(&e, 0x0F); e8(&e, 0xAF); e8(&e, 0xC7); break; /* imul rax,rdi */
            case MIR_DIV: rex(&e,1,0,0,0); e8(&e, 0x99); rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xFF); break;
            case MIR_MOD: rex(&e,1,0,0,0); e8(&e, 0x99); rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xFF); rex(&e,1,0,0,0); e8(&e, 0x89); e8(&e, 0xD0); break;
            case MIR_AND: rex(&e,1,0,0,0); e8(&e, 0x21); e8(&e, 0xF8); break;
            case MIR_OR:  rex(&e,1,0,0,0); e8(&e, 0x09); e8(&e, 0xF8); break;
            case MIR_XOR: rex(&e,1,0,0,0); e8(&e, 0x31); e8(&e, 0xF8); break;
            default: break;
            }
            /* Store result — skip if next instr is RET consuming this dst */
            int sd = VR_ENC(in->dst);
            if (sd >= 0) {
                if (sd != 0 && !NEXT_IS_RET(in->dst)) {
                    emit_mov_reg(&e, sd, 0);
                } else if (NEXT_IS_RET(in->dst)) {
                    result_in_rax = 1;  /* result stays in rax for RET */
                } else {
                    result_in_rax = 0;
                }
            } else {
                emit_store_rbp(&e, VR_SPILL(in->dst), 0);
                result_in_rax = 0;
            }
            break;
        }
        case MIR_SHL: case MIR_SHR: {
            /* shifts need rcx */
            int sa = VR_ENC(in->a);
            if (sa >= 0) emit_mov_reg(&e, 0, sa);
            else emit_load_rbp(&e, 0, VR_SPILL(in->a));

            int sb = VR_ENC(in->b);
            /* mov rcx, b */
            if (sb >= 0) {
                rex(&e,1,reg_needs_rex(sb),0,0); e8(&e, 0x89); e8(&e, (uint8_t)(0xC0 | ((sb & 7) << 3) | 1));
            } else {
                /* mov rcx, [rbp+off] */
                int32_t off = VR_SPILL(in->b);
                if (off >= -128 && off <= 127) { rex(&e,1,0,0,0); e8(&e, 0x8B); e8(&e, 0x4D); e8(&e, (uint8_t)off); }
                else { rex(&e,1,0,0,0); e8(&e, 0x8B); e8(&e, 0x8D); e32(&e, (uint32_t)off); }
            }
            if (in->op == MIR_SHL) { rex(&e,1,0,0,0); e8(&e, 0xD3); e8(&e, 0xE0); }
            else { rex(&e,1,0,0,0); e8(&e, 0xD3); e8(&e, 0xE8); }

            int sd = VR_ENC(in->dst);
            if (sd >= 0) { if (sd != 0) emit_mov_reg(&e, sd, 0); }
            else emit_store_rbp(&e, VR_SPILL(in->dst), 0);
            break;
        }
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE: {
            int sa = VR_ENC(in->a);
            if (sa >= 0) emit_mov_reg(&e, 0, sa);
            else emit_load_rbp(&e, 0, VR_SPILL(in->a));

            int sb = VR_ENC(in->b);
            if (sb >= 0) emit_mov_reg(&e, 7, sb);
            else emit_load_rbp(&e, 7, VR_SPILL(in->b));

            rex(&e,1,0,0,0); e8(&e, 0x39); e8(&e, 0xF8);  /* cmp rax, rdi */
            uint8_t cc;
            switch (in->op) {
            case MIR_EQ: cc = 0x94; break;
            case MIR_NE: cc = 0x95; break;
            case MIR_LT: cc = 0x9C; break;
            case MIR_LE: cc = 0x9E; break;
            case MIR_GT: cc = 0x9F; break;
            case MIR_GE: cc = 0x9D; break;
            default: cc = 0x94; break;
            }
            e8(&e, 0x0F); e8(&e, cc); e8(&e, 0xC0);        /* setcc al */
            rex(&e,1,0,0,0); e8(&e, 0x0F); e8(&e, 0xB6); e8(&e, 0xC0); /* movzx rax,al */

            int sd = VR_ENC(in->dst);
            if (sd >= 0) { if (sd != 0) emit_mov_reg(&e, sd, 0); }
            else emit_store_rbp(&e, VR_SPILL(in->dst), 0);
            break;
        }
        case MIR_NEG: {
            int sa = VR_ENC(in->a);
            if (sa >= 0) emit_mov_reg(&e, 0, sa);
            else emit_load_rbp(&e, 0, VR_SPILL(in->a));
            rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xD8);  /* neg rax */
            int sd = VR_ENC(in->dst);
            if (sd >= 0) { if (sd != 0) emit_mov_reg(&e, sd, 0); }
            else emit_store_rbp(&e, VR_SPILL(in->dst), 0);
            break;
        }
        case MIR_NOT: {
            int sa = VR_ENC(in->a);
            if (sa >= 0) emit_mov_reg(&e, 0, sa);
            else emit_load_rbp(&e, 0, VR_SPILL(in->a));
            rex(&e,1,0,0,0); e8(&e, 0xF7); e8(&e, 0xD0);  /* not rax */
            int sd = VR_ENC(in->dst);
            if (sd >= 0) { if (sd != 0) emit_mov_reg(&e, sd, 0); }
            else emit_store_rbp(&e, VR_SPILL(in->dst), 0);
            break;
        }
        case MIR_JMP:
            e8(&e, 0xE9);
            PATCH_PUSH(e.n, in->label);
            e32(&e, 0);
            break;
        case MIR_JZ: {
            int sa = VR_ENC(in->a);
            if (sa >= 0) emit_mov_reg(&e, 0, sa);
            else emit_load_rbp(&e, 0, VR_SPILL(in->a));
            rex(&e,1,0,0,0); e8(&e, 0x85); e8(&e, 0xC0);  /* test rax,rax */
            e8(&e, 0x0F); e8(&e, 0x84);                     /* jz rel32 */
            PATCH_PUSH(e.n, in->label);
            e32(&e, 0);
            break;
        }
        case MIR_RET: {
            /* If result is already in rax (lookahead skip), don't reload */
            if (!result_in_rax) {
                int sa = VR_ENC(in->a);
                if (sa >= 0) emit_mov_reg(&e, 0, sa);
                else emit_load_rbp(&e, 0, VR_SPILL(in->a));
            }
            result_in_rax = 0;
            e8(&e, 0xC9);  /* leave */
            e8(&e, 0xC3);  /* ret */
            break;
        }
        }
    }

    /* fallback ret */
    if (e.n == 0 || e.code[e.n-1] != 0xC3) { e8(&e, 0xC9); e8(&e, 0xC3); }

    /* patch jumps */
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
    wubu_mir_free_alloc(assign);

    /* Peephole: disabled — integrated into emitter instead */
    (void)x86_peephole_optimize;

    *out = e.code;
    *out_size = e.n;
    return 0;
}

static int64_t x86_run(const uint8_t *code, size_t size, int64_t arg) {
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

static void x86_describe(void) {
    printf("x86-64 driver: native JIT, MIR register allocator (11 regs), "
           "spill-to-stack fallback, WUBU-ABI-v1 frame.\n");
}

const wubu_isa_driver_t wubu_isa_x86_64 = {
    .name = "x86-64",
    .family = "native",
    .exec = WUBU_ISA_NATIVE,
    .compile = x86_compile,
    .run = x86_run,
    .describe = x86_describe,
};
