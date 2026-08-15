/*
 * wubu_isa_avr.c -- the Atmel AVR ISA driver.
 *
 * The AVR: Atmel's 8-bit RISC microcontroller (ATmega328P = Arduino Uno).
 * 32 8-bit registers (R0-R31), SREG flags, 16-bit instructions.
 * The "AGI on an Arduino" — the most popular MCU in the world.
 *
 * Strategy: SAME MIR as every driver. Each vr lives at RAM[vr+0x30].
 * Emits virtual AVR-style opcodes interpreted by wubu_avr_interp.c.
 * Accumulator-based model: W register + file registers.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define AVR_VR_BASE 0x30

typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t *label_offsets;
    size_t n_labels;
} avr_emitter_t;

static void ep8(avr_emitter_t *e, uint8_t b) {
    if (e->n == e->cap) { e->cap = e->cap ? e->cap * 2 : 256; e->code = realloc(e->code, e->cap); }
    e->code[e->n++] = b;
}

static void ep16(avr_emitter_t *e, uint16_t w) {
    ep8(e, (uint8_t)(w & 0xFF));
    ep8(e, (uint8_t)((w >> 8) & 0xFF));
}

static void note_label(avr_emitter_t *e, uint32_t label, size_t off) {
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}

/* Virtual AVR opcodes */
#define AVR_LDI  0x01
#define AVR_ADD  0x02
#define AVR_SUB  0x03
#define AVR_AND  0x04
#define AVR_OR   0x05
#define AVR_XOR  0x06
#define AVR_MOV  0x07
#define AVR_NEG  0x08
#define AVR_NOT  0x09
#define AVR_RET  0x0A

static int avr_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size) {
    avr_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];

        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n); continue; }

        switch (in->op) {
        case MIR_CONST:
            ep8(&e, AVR_LDI);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)(in->imm & 0xFF));
            break;
        case MIR_MOV:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            break;
        case MIR_ADD:
            /* dst = a + b: MOV dst, a; ADD dst, b */
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_ADD);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->b);
            break;
        case MIR_SUB:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_SUB);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->b);
            break;
        case MIR_AND:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_AND);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->b);
            break;
        case MIR_OR:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_OR);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->b);
            break;
        case MIR_XOR:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_XOR);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->b);
            break;
        case MIR_NEG:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_NEG);
            ep8(&e, (uint8_t)in->dst);
            break;
        case MIR_NOT:
            ep8(&e, AVR_MOV);
            ep8(&e, (uint8_t)in->dst);
            ep8(&e, (uint8_t)in->a);
            ep8(&e, AVR_NOT);
            ep8(&e, (uint8_t)in->dst);
            break;
        case MIR_RET:
            ep8(&e, AVR_RET);
            ep8(&e, (uint8_t)in->a);
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

static int64_t avr_run(const uint8_t *code, size_t size, int64_t arg) {
    extern int64_t wubu_avr_interp(const uint8_t *code, size_t size, int64_t arg);
    return wubu_avr_interp(code, size, arg);
}

static void avr_describe(void) {
    printf("Atmel AVR driver (8-bit RISC): W+32 regs, 16-bit ISA, Arduino heritage.\n"
           "Accumulator-based model; runs via the bundled interpreter —\n"
           "the AGI runs on an Arduino.\n");
}

const wubu_isa_driver_t wubu_isa_avr = {
    .name = "avr",
    .family = "interpreter",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = avr_compile,
    .run = avr_run,
    .describe = avr_describe,
};
