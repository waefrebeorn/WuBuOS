/*
 * wubu_disasm.h  --  WuBuOS x86-64 Trivial Disassembler
 *
 * Single-pass, instruction-at-a-time disassembler.
 * Decodes opcode → mnemonic + operand strings.
 * Not a full x86 decoder — covers the subset emitted by wubu_x86.h.
 */

#ifndef WUBU_DISASM_H
#define WUBU_DISASM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Disassembled instruction */
typedef struct {
    char    mnemonic[16];    /* e.g. "mov", "add", "ret" */
    char    operands[64];    /* e.g. "rax, 42" */
    int     length;          /* Instruction byte length */
} WDisasmInst;

/* Disassemble one instruction at 'code' + offset.
 * Returns the instruction length in bytes, or 0 on failure.
 * Fills 'inst' with decoded mnemonic and operands. */
int wdisasm_one(const uint8_t *code, size_t code_len,
                size_t offset, WDisasmInst *inst);

/* Disassemble 'code_len' bytes starting at 'code', printing to 'out'.
 * Returns number of instructions successfully decoded. */
int wdisasm_dump(const uint8_t *code, size_t code_len, FILE *out);

/* Disassemble to a string buffer (caller provides buf of size bufsz).
 * Returns number of characters written (excluding NUL). */
int wdisasm_to_str(const uint8_t *code, size_t code_len,
                   char *buf, size_t bufsz);

#endif /* WUBU_DISASM_H */
