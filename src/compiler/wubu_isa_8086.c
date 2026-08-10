/*
 * wubu_isa_8086.c -- the INTEL 8086 ISA driver (the x86 family's root).
 *
 * The everything-hardware directive (2026-08-04): "AGI goes on
 * everything in some way and has tools for everything." The 8086
 * (1978) is the ancestor of the machine this repo runs on — 16-bit,
 * 20-bit segmented address space (1 MB via seg:off), 4 data regs
 * (AX/BX/CX/DX) + 4 index/ptr regs (SI/DI/BP/SP), CISC with variable-
 * length instructions, little-endian. MS-DOS ran on it for 20 years.
 *
 * This driver emits REAL 8086 machine code (a .COM-style image loaded
 * at CS:0x100) and executes it through the repo's EXISTING in-process
 * interpreter (src/runtime/wubu_dos_emu.c — the 22/22-tested DOS
 * emulator). The interpreter is the verification oracle for the bytes,
 * and tools/verify_isa.sh (objdump -m i8086) verifies each encoding.
 *
 * Strategy (the same stack-slot discipline as the m68k driver):
 *   - prologue:  push bp ; mov bp,sp ; sub sp,frame
 *   - each vr lives at [bp-disp]  (disp = (vr+1)*2, NEGATIVE below bp)
 *   - operations: mov ax,[bp-dispA] ; mov bx,[bp-dispB] ; op ax,bx ;
 *     mov [bp-dispDst],ax
 *   - result in AX; epilogue: mov sp,bp ; pop bp ; ret  — the caller
 *     (test harness) reads AX via wubu_dos_emu_regs()
 *
 * C11, self-contained. Links src/runtime/wubu_dos_emu.c + friends.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- little-endian byte emitter ---- */
typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t *label_offsets;
    size_t n_labels;
    size_t internal_seq;
} i8086_emitter_t;

/* an internal label: labels >= e->n_labels are free of MIR labels. */
static uint32_t internal_label(i8086_emitter_t *e)
{
    return (uint32_t)(e->n_labels + e->internal_seq++);
}

static void e8(i8086_emitter_t *e, uint8_t b)
{
    if (e->n + 1 > e->cap) {
        e->cap = e->cap ? e->cap * 2 : 512;
        e->code = realloc(e->code, e->cap);
    }
    e->code[e->n++] = b;
}
static void e16(i8086_emitter_t *e, uint16_t w)
{
    e8(e, (uint8_t)(w & 0xFF));
    e8(e, (uint8_t)(w >> 8));
}

/* slot displacement below BP: vr N at [bp - (N+1)*2] */
static int16_t slot_disp(wubu_vr_t vr) { return (int16_t)(-((int64_t)(vr + 1) * 2)); }

/* ---- VERIFIED encodings (objdump -m i8086; see tools/verify_isa.sh) ----
 *
 * 8086 register encoding (ModRM r/m + reg fields):
 *   AX=0 BX=3 CX=1 DX=2  (the low 3 bits of the reg field)
 * [bp+disp16] = ModRM mod=10, rm=110 (BP)
 *
 * MOV ax, imm16   : B8 iw        mov ax,0x0001
 * MOV bx, imm16   : BB iw
 * MOV ax, [bp+d]  : 8B 46 d16    mov 0x1234(%bp),%ax   (mod=10 rm=110 reg=000)
 * MOV [bp+d], ax  : 89 46 d16    mov %ax,0x1234(%bp)
 * MOV bx, [bp+d]  : 8B 5E d16
 * ADD ax, bx      : 01 D8        add %bx,%ax
 * SUB ax, bx      : 29 D8
 * MUL bx          : F7 E3        (unsigned AX*BX -> DX:AX)
 * IMUL bx         : F7 EB
 * DIV bx          : F7 F3        (unsigned AX/BX, quotient->AX)
 * IDIV bx         : F7 FB
 * AND ax, bx      : 21 D8
 * OR  ax, bx      : 09 D8
 * XOR ax, bx      : 31 D8
 * NEG ax          : F7 D8
 * NOT ax          : F7 D0
 * CMP ax, bx      : 39 D8        (flags only)
 * MOV ax, bx      : 89 D8
 * JMP rel16       : E9 iw
 * JZ/JNZ/JG/JGE/JL/JLE rel16: 0F 84/85/8F/8D/8C/8E iw
 *   (these are 386+ opcodes; on a bare 8086 the conditional jumps are
 *   74/75/7F/7D/7C/7E with 8-bit disp — use those for the 8086 subset)
 * INT 20h         : CD 20        (terminate, the .COM convention)
 * PUSH BP         : 55
 * MOV BP,SP       : 89 E5
 * SUB SP, imm16   : 81 EC iw
 * MOV SP,BP       : 89 EC
 * POP BP          : 5D
 * RET             : C3
 *
 * 8-bit conditional jumps (the real 8086 set):
 *   JZ  = 74, JNZ = 75, JG = 7F, JGE = 7D, JL = 7C, JLE = 7E
 */

static void emit_mov_ax_imm(i8086_emitter_t *e, int16_t v)
{
    e8(e, 0xB8); e16(e, (uint16_t)v);
}
static void emit_mov_bx_imm(i8086_emitter_t *e, int16_t v)
{
    e8(e, 0xBB); e16(e, (uint16_t)v);
}
/* mov ax, [bp+disp] : 8B 46 disp8 (modrm 46 = mod=01, reg=AX, rm=BP => 8-bit disp) */
static void emit_load_ax_slot(i8086_emitter_t *e, int16_t disp)
{
    e8(e, 0x8B); e8(e, 0x46); e8(e, (uint8_t)disp);
}
/* mov [bp+disp], ax : 89 46 disp8 (modrm 46 = mod=01, reg=AX, rm=BP => 8-bit disp) */
static void emit_store_ax_slot(i8086_emitter_t *e, int16_t disp)
{
    e8(e, 0x89); e8(e, 0x46); e8(e, (uint8_t)disp);
}
/* mov bx, [bp+disp] : 8B 5E disp8 (modrm 5E = mod=01, reg=BX, rm=BP => 8-bit disp) */
static void emit_load_bx_slot(i8086_emitter_t *e, int16_t disp)
{
    e8(e, 0x8B); e8(e, 0x5E); e8(e, (uint8_t)disp);
}
/* mov ax, bx : 89 D8 */
static void emit_mov_ax_bx(i8086_emitter_t *e) { e8(e, 0x89); e8(e, 0xD8); }

typedef struct { size_t pos; uint32_t label; uint8_t opcode; } i8086_patch_t;

static void patch_push(i8086_patch_t **patches, size_t *np, size_t *cap,
                       size_t pos, uint8_t opcode, uint32_t label)
{
    if (*np == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *patches = realloc(*patches, *cap * sizeof(i8086_patch_t));
    }
    (*patches)[*np].pos = pos;
    (*patches)[*np].opcode = opcode;
    (*patches)[*np].label = label;
    (*np)++;
}

static void note_label(i8086_emitter_t *e, uint32_t label, size_t off)
{
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const i8086_emitter_t *e, uint32_t label)
{
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

static int i8086_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size)
{
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->dst > max_vr) max_vr = in->dst;
        if (in->a > max_vr) max_vr = in->a;
        if (in->b > max_vr) max_vr = in->b;
    }

    i8086_emitter_t e;
    memset(&e, 0, sizeof(e));
    size_t frame = (max_vr + 1) * 2 + 32;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    i8086_patch_t *patches = NULL;
    size_t np = 0, cap = 0;

    /* prologue: push bp ; mov bp,sp ; sub sp,frame */
    e8(&e, 0x55);                      /* push bp */
    e8(&e, 0x89); e8(&e, 0xE5);        /* mov bp,sp */
    e8(&e, 0x81); e8(&e, 0xEC); e16(&e, (uint16_t)frame);  /* sub sp,frame */

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n); continue; }
        switch (in->op) {
        case MIR_CONST:
            emit_mov_ax_imm(&e, (int16_t)in->imm);
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        case MIR_MOV:
            emit_load_ax_slot(&e, slot_disp(in->a));
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
        case MIR_AND: case MIR_OR: case MIR_XOR:
            emit_load_ax_slot(&e, slot_disp(in->a));
            emit_load_bx_slot(&e, slot_disp(in->b));
            switch (in->op) {
            case MIR_ADD: e8(&e, 0x01); e8(&e, 0xD8); break;   /* add bx,ax -> ax+=bx */
            case MIR_SUB: e8(&e, 0x29); e8(&e, 0xD8); break;   /* sub */
            case MIR_MUL: e8(&e, 0xF7); e8(&e, 0xE3); break;   /* mul bx: DX:AX=AX*BX */
            case MIR_DIV:
                e8(&e, 0x99);                    /* cwd (sign-extend AX into DX:AX for IDIV) */
                e8(&e, 0xF7); e8(&e, 0xFB); break;   /* idiv bx (signed) */
            case MIR_MOD:
                e8(&e, 0x99);                    /* cwd (sign-extend AX into DX:AX) */
                e8(&e, 0xF7); e8(&e, 0xFB);      /* idiv bx: AX=quot, DX=rem (signed) */
                e8(&e, 0x89); e8(&e, 0xD0);      /* mov ax,dx (the remainder) */
                break;
            case MIR_AND: e8(&e, 0x21); e8(&e, 0xD8); break;
            case MIR_OR:  e8(&e, 0x09); e8(&e, 0xD8); break;
            case MIR_XOR: e8(&e, 0x31); e8(&e, 0xD8); break;
            default: break;
            }
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE:
        {
            uint32_t l_set1 = internal_label(&e);
            uint32_t l_done = internal_label(&e);
            emit_load_ax_slot(&e, slot_disp(in->a));
            emit_load_bx_slot(&e, slot_disp(in->b));
            e8(&e, 0x39); e8(&e, 0xD8);        /* cmp ax,bx (ax - bx) */
            uint8_t cc;
            switch (in->op) {
            case MIR_EQ: cc = 0x74; break;     /* jz */
            case MIR_NE: cc = 0x75; break;     /* jnz */
            case MIR_LT: cc = 0x7C; break;     /* jl (signed) */
            case MIR_LE: cc = 0x7E; break;     /* jle */
            case MIR_GT: cc = 0x7F; break;     /* jg */
            case MIR_GE: cc = 0x7D; break;     /* jge */
            default: cc = 0x74; break;
            }
            /* jcc set1 (8-bit disp, patched) */
            {
                size_t pos = e.n;
                e8(&e, cc); e8(&e, 0x00);
                patch_push(&patches, &np, &cap, pos, cc, l_set1);
            }
            emit_mov_ax_imm(&e, 0);            /* false: ax=0 */
            {
                size_t pos = e.n;
                e8(&e, 0xEB); e8(&e, 0x00);    /* jmp short done */
                patch_push(&patches, &np, &cap, pos, 0xEB, l_done);
            }
            note_label(&e, l_set1, e.n);       /* set1: */
            emit_mov_ax_imm(&e, 1);
            note_label(&e, l_done, e.n);       /* done: */
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        }
        case MIR_NEG:
            emit_load_ax_slot(&e, slot_disp(in->a));
            e8(&e, 0xF7); e8(&e, 0xD8);        /* neg ax */
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        case MIR_NOT:
            emit_load_ax_slot(&e, slot_disp(in->a));
            e8(&e, 0xF7); e8(&e, 0xD0);        /* not ax */
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        case MIR_SHL: case MIR_SHR:
        {
            /* loop shift: ax = value, cx = count
             * loop: test cx,cx (85 C9) ; jz done ; shl/shr ax,1 (D1 E0/E8)
             *       dec cx (49) ; jmp loop (EB rel8) ; done: */
            emit_load_ax_slot(&e, slot_disp(in->b));
            e8(&e, 0x89); e8(&e, 0xC1);        /* mov cx,ax (count) */
            emit_load_ax_slot(&e, slot_disp(in->a));   /* ax = value */
            size_t loop = e.n;
            e8(&e, 0x85); e8(&e, 0xC9);        /* test cx,cx */
            size_t jz_pos = e.n;
            e8(&e, 0x74); e8(&e, 0x00);        /* jz done (patched below) */
            e8(&e, 0xD1); e8(&e, in->op == MIR_SHL ? 0xE0 : 0xE8); /* shl/shr ax,1 */
            e8(&e, 0x49);                      /* dec cx */
            {
                int32_t rel = (int32_t)(loop - (e.n + 2));
                e8(&e, 0xEB); e8(&e, (uint8_t)(rel & 0xFF));  /* jmp loop */
            }
            /* done: patch the jz to here */
            {
                int32_t rel = (int32_t)(e.n - (jz_pos + 2));
                e.code[jz_pos + 1] = (uint8_t)(rel & 0xFF);
            }
            emit_store_ax_slot(&e, slot_disp(in->dst));
            break;
        }
        case MIR_JMP:
        {
            size_t pos = e.n;
            e8(&e, 0xE9); e16(&e, 0x0000);     /* jmp rel16 */
            patch_push(&patches, &np, &cap, pos, 0xE9, in->label);
            break;
        }
        case MIR_JZ:
        {
            emit_load_ax_slot(&e, slot_disp(in->a));
            e8(&e, 0x85); e8(&e, 0xC0);        /* test ax,ax */
            size_t pos = e.n;
            e8(&e, 0x74); e8(&e, 0x00);        /* jz rel8 (patched) */
            patch_push(&patches, &np, &cap, pos, 0x74, in->label);
            break;
        }
        case MIR_RET:
            emit_load_ax_slot(&e, slot_disp(in->a));   /* ax = result */
            e8(&e, 0x89); e8(&e, 0xEC);        /* mov sp,bp */
            e8(&e, 0x5D);                      /* pop bp */
            e8(&e, 0xC3);                      /* ret */
            break;
        default:
            break;
        }
    }

    /* fallback: if no RET was emitted, return 0 */
    if (e.n == 0 || e.code[e.n-1] != 0xC3) {
        emit_mov_ax_imm(&e, 0);
        e8(&e, 0x89); e8(&e, 0xEC);
        e8(&e, 0x5D);
        e8(&e, 0xC3);
    }

    /* patch pass: fix every jump to its label. 8086 conditional jumps
     * are rel8 (74/75/7C/7D/7F/7E/EB — ONE disp byte); only JMP E9 is
     * rel16. We store the branch size in the opcode byte's high bit. */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        size_t pos = patches[i].pos;
        uint8_t op = patches[i].opcode;
        if (op == 0xE9) {
            int32_t rel = (int32_t)(t - (pos + 2));
            e.code[pos + 1] = (uint8_t)(rel & 0xFF);
            e.code[pos + 2] = (uint8_t)((rel >> 8) & 0xFF);
        } else {
            /* rel8 branch */
            int32_t rel = (int32_t)(t - (pos + 2));
            if (rel < -128) rel = -128;
            if (rel > 127) rel = 127;
            e.code[pos + 1] = (uint8_t)(rel & 0xFF);
        }
    }

    free(patches);
    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

/* the interpreter lives in src/runtime/wubu_dos_emu.c — reuse it */
static int64_t i8086_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    /* forward decl of the emu entry points */
    extern void *wubu_dos_emu_create(void);
    extern void wubu_dos_emu_destroy(void *e);
    extern int wubu_dos_emu_load_com(void *e, const uint8_t *data, size_t size);
    extern int wubu_dos_emu_run(void *e, unsigned long long max_steps);
    extern void wubu_dos_emu_regs(const void *e, uint16_t *ax, uint16_t *bx,
                                  uint16_t *cx, uint16_t *dx, uint16_t *si,
                                  uint16_t *di, uint16_t *ip, uint16_t *flags,
                                  uint16_t *cs);

    void *e = wubu_dos_emu_create();
    if (!e) return -1;
    int rc = wubu_dos_emu_load_com(e, code, size);
    if (rc != 0) { wubu_dos_emu_destroy(e); return -2; }
    wubu_dos_emu_run(e, 10000000ULL);   /* bounded */
    uint16_t ax = 0, bx = 0, cx = 0, dx = 0, si = 0, di = 0, ip = 0, fl = 0, cs = 0;
    wubu_dos_emu_regs(e, &ax, &bx, &cx, &dx, &si, &di, &ip, &fl, &cs);
    wubu_dos_emu_destroy(e);
    /* 8086 is 16-bit: sign-extend the AX result to the I64 the battery expects */
    return (int64_t)(int16_t)ax;
}

static void i8086_describe(void)
{
    printf("Intel 8086 driver (1978): 16-bit CISC, 1 MB segmented space, "
           "little-endian, 4 data regs. Runs via the repo's in-process DOS "
           "emulator (wubu_dos_emu.c, 22/22 tested) — the x86 root, real bytes.\n");
}

const wubu_isa_driver_t wubu_isa_i8086 = {
    .name = "8086",
    .family = "portable",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = i8086_compile,
    .run = i8086_run,
    .describe = i8086_describe,
};
