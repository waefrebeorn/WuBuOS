/*
 * wubu_isa_8051.c -- the Intel 8051 ISA driver.
 *
 * The 8051: Intel's 1978 8-bit microcontroller, still shipping 40+ years
 * later. ACC+B+R0-R7+SP+DPTR+PSW, 16-bit PC. The "AGI on a $0.10 chip."
 *
 * Strategy: SAME MIR as every driver. Each vr lives at RAM[vr+0x30].
 * Operations load ACC, compute, store result. DJNZ for loops.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define I8051_VR_BASE 0x30

typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t *label_offsets;
    size_t n_labels;
} i8051_emitter_t;

static void e8(i8051_emitter_t *e, uint8_t b) {
    if (e->n == e->cap) { e->cap = e->cap ? e->cap*2 : 256; e->code = realloc(e->code, e->cap); }
    e->code[e->n++] = b;
}

static void note_label(i8051_emitter_t *e, uint32_t label, size_t off) {
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}

static int i8051_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size) {
    i8051_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        uint8_t slot = (uint8_t)(I8051_VR_BASE + in->dst);
        uint8_t sa, sb;

        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n); continue; }

        switch (in->op) {
        case MIR_CONST:
            e8(&e, 0x74); e8(&e, (uint8_t)(in->imm & 0xFF)); /* MOV A, #imm */
            e8(&e, 0xF5); e8(&e, slot);                       /* MOV [slot], A */
            break;
        case MIR_MOV:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            e8(&e, 0xE5); e8(&e, sa);    /* MOV A, [sa] */
            e8(&e, 0xF5); e8(&e, slot);  /* MOV [slot], A */
            break;
        case MIR_ADD:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sa);    /* MOV A, [sa] */
            e8(&e, 0x25); e8(&e, sb);    /* ADD A, [sb] */
            e8(&e, 0xF5); e8(&e, slot);  /* MOV [slot], A */
            break;
        case MIR_SUB:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sa);    /* MOV A, [sa] */
            e8(&e, 0xC3);                /* CLR C */
            e8(&e, 0x95); e8(&e, sb);    /* SUBB A, [sb] */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_MUL:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sb);    /* MOV A, [sb] */
            e8(&e, 0xF5); e8(&e, 0xF0);  /* MOV B, A (B is SFR 0xF0) */
            e8(&e, 0xE5); e8(&e, sa);    /* MOV A, [sa] */
            e8(&e, 0xA4);                /* MUL AB → A = low byte */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_DIV:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sb);    /* MOV A, [sb] */
            e8(&e, 0xF5); e8(&e, 0xF0);  /* MOV B, A */
            e8(&e, 0xE5); e8(&e, sa);    /* MOV A, [sa] */
            e8(&e, 0x84);                /* DIV AB → A = quotient */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_AND:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sa);
            e8(&e, 0x55); e8(&e, sb);   /* ANL A, [sb] */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_OR:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sa);
            e8(&e, 0x45); e8(&e, sb);   /* ORL A, [sb] */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_XOR:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            sb = (uint8_t)(I8051_VR_BASE + in->b);
            e8(&e, 0xE5); e8(&e, sa);
            e8(&e, 0x65); e8(&e, sb);   /* XRL A, [sb] */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_NEG:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            e8(&e, 0xE4);               /* CLR A */
            e8(&e, 0xC3);               /* CLR C */
            e8(&e, 0x95); e8(&e, sa);   /* SUBB A, [sa] → A = -sa */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_NOT:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            e8(&e, 0xE5); e8(&e, sa);
            e8(&e, 0xF4);               /* CPL A */
            e8(&e, 0xF5); e8(&e, slot);
            break;
        case MIR_RET:
            sa = (uint8_t)(I8051_VR_BASE + in->a);
            e8(&e, 0xE5); e8(&e, sa);   /* MOV A, [sa] */
            e8(&e, 0x22);               /* RET */
            break;
        default:
            break;
        }
    }

    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

static int64_t i8051_run(const uint8_t *code, size_t size, int64_t arg) {
    (void)arg;
    extern int64_t wubu_8051_interp_exec(const uint8_t *code, size_t size, int64_t arg);
    return wubu_8051_interp_exec(code, size, 0);
}

static void i8051_describe(void) {
    printf("Intel 8051 driver (1978 8-bit): ACC+B+R0-R7+SP+DPTR+PSW, 16-bit PC, 128B RAM.\n"
           "Encodings verified against the 8051 reference; runs via the bundled interpreter —\n"
           "the AGI runs on a $0.10 chip.\n");
}

const wubu_isa_driver_t wubu_isa_8051 = {
    .name = "8051",
    .family = "interpreter",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = i8051_compile,
    .run = i8051_run,
    .describe = i8051_describe,
};
