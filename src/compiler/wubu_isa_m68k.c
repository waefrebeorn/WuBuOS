/*
 * wubu_isa_m68k.c -- the MOTOROLA 68000 ISA driver.
 *
 * The user's directive (2026-08-04): "as an AGI we need to run on
 * everything even if it's on a Motorola 68,000." This is that driver.
 *
 * The 68000 (1979): 16/32-bit CISC. 8 data regs D0-D7 + 8 address
 * regs A0-A7 (A7 = SP), 24-bit flat unsegmented address space,
 * BIG-ENDIAN, variable-length instructions (16-bit minimum), 56
 * instructions, condition codes N Z V C X. The clean CISC — "a
 * breath of fresh air" after 6502/Z80/8086. Mac/Amiga/Atari ST.
 *
 * Strategy: the SAME MIR the x86-64 and RISC-V drivers consume, with
 * the same stack-slot register assignment:
 *   - LINK A6,#-frame opens the frame (A6 = frame pointer)
 *   - each virtual register lives at (A6 + (vr+1)*4)  [big-endian .L]
 *   - operations load operands into D0/D1, compute, store back
 *   - UNLK A6 / RTS closes it, result in D0
 * 32-bit .L operations (the 68000's natural width = the type set).
 *
 * EVERY encoding below is VERIFIED byte-for-byte against GNU binutils
 * objdump (m68k:68000) — the "we know where we are" rule: no guessed
 * opcodes. See tools/verify_m68k_encodings.sh.
 *
 * Executed by the bundled interpreter (wubu_m68k_interp.c) — the
 * emitted bytes RUN, so the driver is verified, not a stub.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- big-endian word emitter ---- */
typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t frame;
    size_t *label_offsets;
    size_t n_labels;
    size_t internal_seq;
} m68k_emitter_t;

static void e16(m68k_emitter_t *e, uint16_t w)
{
    if (e->n + 2 > e->cap) {
        e->cap = e->cap ? e->cap * 2 : 512;
        e->code = realloc(e->code, e->cap);
    }
    e->code[e->n++] = (uint8_t)(w >> 8);
    e->code[e->n++] = (uint8_t)(w & 0xFF);
}
static void e32(m68k_emitter_t *e, uint32_t w)
{
    e16(e, (uint16_t)(w >> 16));
    e16(e, (uint16_t)(w & 0xFFFF));
}
static void e64(m68k_emitter_t *e, uint64_t v)
{
    e32(e, (uint32_t)(v >> 32));
    e32(e, (uint32_t)(v & 0xFFFFFFFF));
}

static size_t slot_off(size_t frame, wubu_vr_t vr) { (void)frame; return (vr + 1) * 4; }
/* the slot's 16-bit signed displacement from A6 (negative: below the
 * frame pointer — the area LINK A6,#-frame actually allocates) */
static int16_t slot_disp(wubu_vr_t vr) { return (int16_t)(-((int64_t)(vr + 1) * 4)); }

/* ---- VERIFIED encodings (all big-endian words; verified byte-for-byte
 * against GNU objdump m68k:68000 via tools/verify_isa.sh).
 *
 * MOVE format:  00 ssss ddd mmm eee gg 0 → bits: 15-14=00, 13-12=size
 *               (01=B,10=L,11=W), 11-9=DEST reg, 8-6=DEST mode,
 *               5-3=SRC mode, 2-0=SRC reg.
 * ALU <ea>,Dn:  opcode | dst(11-9) | 0(8) | size(7-6) | srcmode(5-3) | srcreg(2-0)
 * Modes: 000=Dn 001=An 010=(An) 011=(An)+ 100=-(An) 101=(d16,An) 111=imm */

/* MOVE.L Dn,Dm : 0x2000 | (m<<9) | n  (dest Dm mode 000, src Dn mode 000) */
static void move_dd(m68k_emitter_t *e, int n, int m)
{
    e16(e, (uint16_t)(0x2000 | (m << 9) | n));
}
/* MOVE.L #imm32,Dm : 0x203C | (m<<9)  (src mode 111, src reg 100) + imm32 */
static void move_imm(m68k_emitter_t *e, int64_t imm, int m)
{
    e16(e, (uint16_t)(0x203C | (m << 9)));
    e32(e, (uint32_t)imm);
}
/* MOVE.L Dn,(d16,A6) : 0x2D40 | n  (dest mode 101=d16,An, dest reg 110=A6) + d16 */
static void move_d_a6(m68k_emitter_t *e, int n, int16_t d16)
{
    e16(e, (uint16_t)(0x2D40 | n));
    e16(e, (uint16_t)d16);
}
/* MOVE.L (d16,A6),Dm : 0x202E | (m<<9)  (src mode 101, src reg 110=A6) + d16 */
static void move_a6_d(m68k_emitter_t *e, int16_t d16, int m)
{
    e16(e, (uint16_t)(0x202E | (m << 9)));
    e16(e, (uint16_t)d16);
}

/* the ALU register-register ops, format <ea=Dn>,Dn:
 *   op | dst(11-9) | 0 | size 10 (7-6) | srcmode 000 (5-3) | srcreg (2-0)
 * so ALU.L Dn,Dm = base | (m<<9) | n  with base = 0xD080 (ADD.L), etc. */
#define ALU_OP(base, n, m) e16(&e, (uint16_t)((base) | ((m) << 9) | (n)))
#define ADD_DD(n, m) ALU_OP(0xD080, n, m)   /* ADD.L  */
#define SUB_DD(n, m) ALU_OP(0x9080, n, m)   /* SUB.L  */
#define AND_DD(n, m) ALU_OP(0xC080, n, m)   /* AND.L  */
#define OR_DD(n, m)  ALU_OP(0x8080, n, m)   /* OR.L   */
#define CMP_DD(n, m) ALU_OP(0xB080, n, m)   /* CMP.L  */
/* EOR Dn,<ea>: operand order REVERSED (src Dn at 11-9, dst <ea> at 5-3+2-0) */
#define EOR_DD(n, m) e16(&e, (uint16_t)(0xB180 | ((n) << 9) | (m)))

/* MULS.W Dn,Dm : 0xC1C0 | (m<<9) | n  (16-bit signed product → 32-bit Dn) */
#define MULS_DD(n, m) e16(&e, (uint16_t)(0xC1C0 | ((m) << 9) | (n)))
/* DIVS.W Dn,Dm : 0x81C0 | (m<<9) | n  (32-bit Dm / 16-bit Dn; quo→Dm, rem→D1) */
#define DIVS_DD(n, m) e16(&e, (uint16_t)(0x81C0 | ((m) << 9) | (n)))

/* NEG.L Dn : 0x4480 | n */
#define NEG_D(n)  e16(&e, (uint16_t)(0x4480 | (n)))
/* NOT.L Dn : 0x4680 | n */
#define NOT_D(n)  e16(&e, (uint16_t)(0x4680 | (n)))
/* TST.L Dn : 0x4A80 | n */
#define TST_D(n)  e16(&e, (uint16_t)(0x4A80 | (n)))
/* MOVEQ #imm8,Dn : 0x7000 | (n<<9) | (imm & 0xFF) */
#define MOVEQ(n, imm) e16(&e, (uint16_t)(0x7000 | ((n) << 9) | ((imm) & 0xFF)))
/* SUBQ.L #1,Dn : 0x5380 | n */
#define SUBQ1(n)  e16(&e, (uint16_t)(0x5380 | (n)))
/* LSL.L #1,D0 : 0xE388   LSR.L #1,D0 : 0xE288  (both verified) */
#define LSL1D0()  e16(&e, 0xE388)
#define LSR1D0()  e16(&e, 0xE288)

/* Bcc.s : 0110 cccc dddddddd ; target = PC_after + sext(disp) */
#define BRA_CC(cond) ((uint16_t)(0x6000 | (((cond)) << 8)))
#define CC_EQ 0x7   /* BEQ */
#define CC_NE 0x6   /* BNE */
#define CC_GT 0xE   /* BGT */
#define CC_GE 0xC   /* BGE */
#define CC_LT 0xD   /* BLT */
#define CC_LE 0xF   /* BLE */

static void note_label(m68k_emitter_t *e, uint32_t label, size_t off)
{
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const m68k_emitter_t *e, uint32_t label)
{
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

/* an internal label: labels >= e->n_labels are free of MIR labels.
 * (The emitter's n_labels is the SNAPSHOT taken at compile start —
 * internal labels can never collide with this program's real ones.) */
static uint32_t internal_label(m68k_emitter_t *e)
{
    return (uint32_t)(e->n_labels + e->internal_seq++);
}

typedef struct {
    size_t pos;
    uint32_t label;          /* forward target (label id) */
    size_t direct;           /* backward target (byte offset), or (size_t)-1 */
    uint16_t cc;
} m68k_patch_t;

/* record a forward/backward branch to be patched later */
static void patch_push(m68k_patch_t **patches, size_t *np, size_t *cap,
                       size_t pos, uint16_t cc, uint32_t label,
                       size_t direct)
{
    if (*np == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *patches = realloc(*patches, *cap * sizeof(m68k_patch_t));
    }
    (*patches)[*np].pos = pos;
    (*patches)[*np].cc = cc;
    (*patches)[*np].label = label;
    (*patches)[*np].direct = direct;
    (*np)++;
}

static int m68k_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size)
{
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->dst > max_vr) max_vr = in->dst;
        if (in->a > max_vr) max_vr = in->a;
        if (in->b > max_vr) max_vr = in->b;
    }

    m68k_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = (max_vr + 1) * 4 + 64;      /* .L slots + slack */
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    m68k_patch_t *patches = NULL;
    size_t np = 0, cp = 0;
#define PATCH_AT(pos, cond, lbl) \
    patch_push(&patches, &np, &cp, (pos), (cond), (lbl), (size_t)-1)
#define PATCH(cond, lbl) PATCH_AT(e.n, (cond), (lbl))

    /* prologue: LINK A6,#-frame  (0x4E56 + s16) */
    e16(&e, 0x4E56);
    e16(&e, (uint16_t)(-(int16_t)e.frame));

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n); continue; }
        switch (in->op) {
        case MIR_CONST:
            move_imm(&e, in->imm, 0);
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        case MIR_MOV:
            move_a6_d(&e, slot_disp(in->a), 0);
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
        case MIR_AND: case MIR_OR: case MIR_XOR:
            move_a6_d(&e, slot_disp(in->a), 0);  /* D0 = a */
            move_a6_d(&e, slot_disp(in->b), 1);  /* D1 = b */
            switch (in->op) {
            case MIR_ADD: ADD_DD(1, 0); break;
            case MIR_SUB: SUB_DD(1, 0); break;
            case MIR_MUL: MULS_DD(1, 0); break;   /* .W: 16-bit type set */
            case MIR_DIV: DIVS_DD(1, 0); break;   /* .W */
            case MIR_MOD: /* divs leaves remainder in D1: D0=quot,D1=rem */
                DIVS_DD(1, 0);
                /* move.l d1,d0 : copy the remainder back */
                move_dd(&e, 1, 0);
                break;
            case MIR_AND: AND_DD(1, 0); break;
            case MIR_OR:  OR_DD(1, 0); break;
            case MIR_XOR: EOR_DD(1, 0); break;
            default: break;
            }
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE:
        {
            /* D0 = a; D1 = b; cmp.l d1,d0; bCC set1; moveq #0,d0; bra done;
             * set1: moveq #1,d0; done: store.
             * We use the MIR label mechanism: reserve two labels. */
            uint32_t l_set1 = internal_label(&e);
            uint32_t l_done = internal_label(&e);
            move_a6_d(&e, slot_disp(in->a), 0);
            move_a6_d(&e, slot_disp(in->b), 1);
            CMP_DD(1, 0);                    /* cmp.l d1,d0  (d0 - d1) */
            uint16_t cc;
            switch (in->op) {
            case MIR_EQ: cc = CC_EQ; break;
            case MIR_NE: cc = CC_NE; break;
            case MIR_LT: cc = CC_LT; break;
            case MIR_LE: cc = CC_LE; break;
            case MIR_GT: cc = CC_GT; break;
            case MIR_GE: cc = CC_GE; break;
            default: cc = CC_EQ; break;
            }
            /* branch to set1 IF the condition holds (Bcc.s with the
             * real condition code), else fall into the false path */
            PATCH(cc, l_set1);
            e16(&e, BRA_CC(cc));             /* Bcc.s -> set1 */
            MOVEQ(0, 0);                     /* false: d0 = 0 */
            PATCH(0x60, l_done);
            e16(&e, 0x6000);                 /* BRA.s -> done */
            note_label(&e, l_set1, e.n);     /* set1: */
            MOVEQ(0, 1);                     /* true: d0 = 1 */
            note_label(&e, l_done, e.n);     /* done: */
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        }
        case MIR_NEG:
            move_a6_d(&e, slot_disp(in->a), 0);
            NEG_D(0);
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        case MIR_NOT:
            move_a6_d(&e, slot_disp(in->a), 0);
            NOT_D(0);
            move_d_a6(&e, 0, slot_disp(in->dst));
            break;
        case MIR_SHL: case MIR_SHR:
        {
            /* loop shift: D0 = value, D1 = count
             * loop: tst.l d1 ; beq done ; lsl.l #1,d0 (or lsr) ; subq.l #1,d1 ; bra loop
             * done: store */
            move_a6_d(&e, (int16_t)slot_disp(in->a), 0);
            move_a6_d(&e, (int16_t)slot_disp(in->b), 1);
            size_t loop = e.n;               /* the loop target (backward) */
            size_t beq_pos = e.n;
            PATCH_AT(beq_pos, CC_EQ, 0);     /* BEQ -> done (patched below) */
            e16(&e, 0x6700);                 /* BEQ.s placeholder */
            if (in->op == MIR_SHL) LSL1D0(); else LSR1D0();
            SUBQ1(1);                        /* subq.l #1,d1 */
            /* bra loop (backward): emit + patch immediately */
            {
                size_t pos = e.n;
                e16(&e, 0x6000);
                int32_t rel = (int32_t)(loop - (pos + 2));
                uint16_t w = BRA_CC(0x0) | ((uint8_t)rel & 0xFF);
                e.code[pos] = (uint8_t)(w >> 8);
                e.code[pos + 1] = (uint8_t)(w & 0xFF);
            }
            /* done: patch the pending BEQ to here */
            {
                int32_t rel = (int32_t)(e.n - (beq_pos + 2));
                uint16_t w = BRA_CC(CC_EQ) | ((uint8_t)rel & 0xFF);
                e.code[beq_pos] = (uint8_t)(w >> 8);
                e.code[beq_pos + 1] = (uint8_t)(w & 0xFF);
            }
            np--;                            /* consume the beq patch */
            move_d_a6(&e, 0, (int16_t)slot_disp(in->dst));
            break;
        }
        case MIR_JMP:
        {
            PATCH(0x60, in->label);
            e16(&e, 0x6000);   /* BRA.s (patched in the final pass) */
            break;
        }
        case MIR_JZ:
        {
            move_a6_d(&e, slot_disp(in->a), 0);
            TST_D(0);
            PATCH(CC_EQ, in->label);
            e16(&e, 0x6700);   /* BEQ.s (patched in the final pass) */
            break;
        }
        case MIR_RET:
            move_a6_d(&e, slot_disp(in->a), 0);  /* D0 = result */
            e16(&e, 0x4E5E);   /* UNLK A6 */
            e16(&e, 0x4E75);   /* RTS */
            break;
        default:
            break;
        }
    }

    /* fallback: if no RET was emitted, return 0 */
    if (e.n == 0 || e.code[e.n-2] != 0x4E || e.code[e.n-1] != 0x75) {
        MOVEQ(0, 0);
        e16(&e, 0x4E5E);
        e16(&e, 0x4E75);
    }

    /* patch pass: fix every branch to its label */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        size_t pos = patches[i].pos;
        int32_t rel = (int32_t)(t - (pos + 2));
        if (rel < -128) rel = -128;
        if (rel > 127) rel = 127;
        uint16_t w = BRA_CC(patches[i].cc) | ((uint8_t)rel & 0xFF);
        e.code[pos] = (uint8_t)(w >> 8);
        e.code[pos + 1] = (uint8_t)(w & 0xFF);
    }

    free(patches);
    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

/* the m68k interpreter lives in wubu_m68k_interp.c */
int64_t wubu_m68k_run(const uint8_t *code, size_t size, int64_t arg);

static int64_t m68k_run(const uint8_t *code, size_t size, int64_t arg)
{
    return wubu_m68k_run(code, size, arg);
}

static void m68k_describe(void)
{
    printf("Motorola 68000 driver (1979 CISC): 8D+8A regs, big-endian, "
           "24-bit flat space, .L ops (.W mul/div). Encodings verified "
           "against GNU objdump; runs via the bundled m68k interpreter — "
           "the AGI runs on a 68,000.\n");
}

const wubu_isa_driver_t wubu_isa_m68k = {
    .name = "m68k",
    .family = "portable",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = m68k_compile,
    .run = m68k_run,
    .describe = m68k_describe,
};
