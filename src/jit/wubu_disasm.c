/*
 * wubu_disasm.c  --  WuBuOS x86-64 Trivial Disassembler
 *
 * Decodes the x86-64 subset emitted by wubu_x86.h.
 * Never claims to be a full x86-64 decoder — that would be 10K+ LOC.
 * Covers: REX+opcodes, ModRM, immediate operands, rel32 jumps/calls.
 */

#include "wubu_disasm.h"
#include "wubu_x86.h"

#include <stdio.h>
#include <string.h>

/* Read little-endian values from byte stream */
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int32_t read_i32(const uint8_t *p) {
    return (int32_t)read_u32(p);
}

static int64_t read_i64(const uint8_t *p) {
    uint64_t lo = read_u32(p);
    uint64_t hi = read_u32(p + 4);
    return (int64_t)(lo | (hi << 32));
}

/* Decode ModRM into reg and rm, accounting for REX bits */
static void decode_modrm(uint8_t modrm, bool rex_r, bool rex_b,
                          int *reg_out, int *rm_out) {
    *reg_out = ((modrm >> 3) & 7) | (rex_r ? 8 : 0);
    *rm_out  = (modrm & 7) | (rex_b ? 8 : 0);
}

static const char *regname64(int reg) {
    static const char *names[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };
    if (reg >= 0 && reg <= 15) return names[reg];
    return "?";
}

static const char *regname32(int reg) {
    static const char *names[] = {
        "eax","ecx","edx","ebx","esp","ebp","esi","edi",
        "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"
    };
    if (reg >= 0 && reg <= 15) return names[reg];
    return "?";
}

int wdisasm_one(const uint8_t *code, size_t code_len,
                size_t offset, WDisasmInst *inst) {
    memset(inst, 0, sizeof(WDisasmInst));

    if (offset >= code_len) return 0;
    size_t pos = offset;
    size_t remain = code_len - offset;
    const uint8_t *c = code + offset;

    /* Parse REX prefix */
    bool has_rex = false;
    bool rex_w = false, rex_r = false, rex_b = false;
    size_t start = pos;
    uint8_t b = c[0];

    if (b >= 0x40 && b <= 0x4F) {
        has_rex = true;
        rex_w = (b & 0x08) != 0;
        rex_r = (b & 0x04) != 0;
        rex_b = (b & 0x01) != 0;
        pos++;
        if (pos >= code_len) return 0;
        c = code + pos;
        remain = code_len - pos;
        b = c[0];
    }

    /* Single-byte opcodes (no REX needed) */
    if (!has_rex) {
        /* ret */
        if (b == 0xC3) {
            strcpy(inst->mnemonic, "ret");
            inst->length = 1;
            return 1;
        }
        /* push r64 (50-57) */
        if (b >= 0x50 && b <= 0x57) {
            int reg = b - 0x50;
            snprintf(inst->mnemonic, sizeof(inst->mnemonic), "push");
            snprintf(inst->operands, sizeof(inst->operands), "%s", regname64(reg));
            inst->length = 1;
            return 1;
        }
        /* pop r64 (58-5F) */
        if (b >= 0x58 && b <= 0x5F) {
            int reg = b - 0x58;
            snprintf(inst->mnemonic, sizeof(inst->mnemonic), "pop");
            snprintf(inst->operands, sizeof(inst->operands), "%s", regname64(reg));
            inst->length = 1;
            return 1;
        }
        /* jmp rel32 (E9) */
        if (b == 0xE9 && remain >= 5) {
            int32_t rel = read_i32(c + 1);
            strcpy(inst->mnemonic, "jmp");
            snprintf(inst->operands, sizeof(inst->operands), "%+d", rel + 5);
            inst->length = 5;
            return 5;
        }
        /* call rel32 (E8) */
        if (b == 0xE8 && remain >= 5) {
            int32_t rel = read_i32(c + 1);
            strcpy(inst->mnemonic, "call");
            snprintf(inst->operands, sizeof(inst->operands), "%+d", rel + 5);
            inst->length = 5;
            return 5;
        }
        /* REX.B push/pop (for r8-r15) */
        if (b == 0x41 && remain >= 2) {
            uint8_t b2 = c[1];
            if (b2 >= 0x50 && b2 <= 0x57) {
                int reg = (b2 - 0x50) + 8;
                snprintf(inst->mnemonic, sizeof(inst->mnemonic), "push");
                snprintf(inst->operands, sizeof(inst->operands), "%s", regname64(reg));
                inst->length = 2;
                return 2;
            }
            if (b2 >= 0x58 && b2 <= 0x5F) {
                int reg = (b2 - 0x58) + 8;
                snprintf(inst->mnemonic, sizeof(inst->mnemonic), "pop");
                snprintf(inst->operands, sizeof(inst->operands), "%s", regname64(reg));
                inst->length = 2;
                return 2;
            }
        }
    }

    /* REX-prefixed or two-byte opcodes */
    if (b == 0xB8 && rex_w && remain >= 9) {
        /* mov r64, imm64 (REX.W B8+rd) */
        int rd = 0;
        /* Actually: opcode is B8 + reg_lo, and REX.B gives reg_hi */
        rd = (b - 0xB8) | (rex_b ? 8 : 0);
        int64_t imm = read_i64(c + 1);
        strcpy(inst->mnemonic, "mov");
        snprintf(inst->operands, sizeof(inst->operands), "%s, %ld",
                 regname64(rd), (long)imm);
        inst->length = (int)(pos - start) + 1 + 8;
        return inst->length;
    }

    /* mov r32, imm32 via C7 /0 */
    if (b == 0xC7 && remain >= 2) {
        uint8_t modrm = c[1];
        int reg_field = 0, rm = 0;
        decode_modrm(modrm, rex_r, rex_b, &reg_field, &rm);
        uint8_t mod = (modrm >> 6) & 3;
        if (mod == 3 && reg_field == 0 && remain >= 6) {
            /* mov r/m64, imm32 (sign-extended) */
            int32_t imm = read_i32(c + 2);
            strcpy(inst->mnemonic, "mov");
            snprintf(inst->operands, sizeof(inst->operands), "%s, %d",
                     regname64(rm), imm);
            inst->length = (int)(pos - start) + 1 + 1 + 4;
            return inst->length;
        }
    }

    /* 0F two-byte opcodes (Jcc, IMUL) */
    if (b == 0x0F && remain >= 2) {
        uint8_t b2 = c[1];
        /* Jcc rel32 (0F 80-8F) */
        if (b2 >= 0x80 && b2 <= 0x8F && remain >= 6) {
            int cc = b2 & 0xF;
            int32_t rel = read_i32(c + 2);
            static const char *ccnames[] = {
                "jo","jno","jb","jae","je","jne","jbe","ja",
                "js","jns","??","??","jl","jge","jle","jg"
            };
            snprintf(inst->mnemonic, sizeof(inst->mnemonic), "%s",
                     cc < 16 ? ccnames[cc] : "j??");
            snprintf(inst->operands, sizeof(inst->operands), "%+d", rel + 6);
            inst->length = (int)(pos - start) + 2 + 4;
            return inst->length;
        }
        /* IMUL r, r/m (0F AF) */
        if (b2 == 0xAF && remain >= 3) {
            uint8_t modrm = c[2];
            int reg = 0, rm = 0;
            decode_modrm(modrm, rex_r, rex_b, &reg, &rm);
            strcpy(inst->mnemonic, "imul");
            snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                     regname64(reg), regname64(rm));
            inst->length = (int)(pos - start) + 3;
            return inst->length;
        }
    }

    /* ALU reg-reg opcodes with ModRM */
    /* 01 = add r/m, r  |  29 = sub r/m, r  |  31 = xor r/m, r
     * 39 = cmp r/m, r  |  85 = test r/m, r |  89 = mov r/m, r
     * 8B = mov r, r/m  |  8D = lea r, m     |  F7 = neg/idiv
     * FF = call/push   |  C1 = shr/shl/sar  |  81 = add/cmp/sub imm32
     * 83 = add/sub rsp, imm8 */
    if (b == 0x89 || b == 0x8B || b == 0x01 || b == 0x29 ||
        b == 0x31 || b == 0x39 || b == 0x85 || b == 0x8D ||
        b == 0x81 || b == 0x83 || b == 0xC1 || b == 0xF7 || b == 0xFF) {
        if (remain < 2) return 0;
        uint8_t modrm = c[1];
        int reg_f = 0, rm = 0;
        decode_modrm(modrm, rex_r, rex_b, &reg_f, &rm);
        uint8_t mod = (modrm >> 6) & 3;

        switch (b) {
        case 0x89: /* mov r/m, r */
            if (mod == 3) {
                strcpy(inst->mnemonic, "mov");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x8B: /* mov r, r/m */
            if (mod == 3) {
                strcpy(inst->mnemonic, "mov");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(reg_f), regname64(rm));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x01:
            if (mod == 3) {
                strcpy(inst->mnemonic, "add");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x29:
            if (mod == 3) {
                strcpy(inst->mnemonic, "sub");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x31:
            if (mod == 3) {
                strcpy(inst->mnemonic, "xor");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x39:
            if (mod == 3) {
                strcpy(inst->mnemonic, "cmp");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x85:
            if (mod == 3) {
                strcpy(inst->mnemonic, "test");
                snprintf(inst->operands, sizeof(inst->operands), "%s, %s",
                         regname64(rm), regname64(reg_f));
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0x81: /* add/sub/cmp imm32 */
            if (mod == 3 && remain >= 6) {
                int32_t imm = read_i32(c + 2);
                const char *ops[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
                const char *mnem = (reg_f < 8) ? ops[reg_f] : "???";
                strcpy(inst->mnemonic, mnem);
                snprintf(inst->operands, sizeof(inst->operands), "%s, %d",
                         regname64(rm), imm);
                inst->length = (int)(pos - start) + 6;
                return inst->length;
            }
            break;

        case 0x83: /* sub/add rsp, imm8 — or other r/m, imm8 */
            if (mod == 3 && remain >= 3) {
                int8_t imm = (int8_t)c[2];
                /* Check for known patterns: sub rsp = 83 EC, add rsp = 83 C4 */
                const char *ops[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
                const char *mnem = (reg_f < 8) ? ops[reg_f] : "???";
                strcpy(inst->mnemonic, mnem);
                snprintf(inst->operands, sizeof(inst->operands), "%s, %d",
                         regname64(rm), (int)imm);
                inst->length = (int)(pos - start) + 3;
                return inst->length;
            }
            break;

        case 0xC1: /* shift r/m, imm8 */
            if (mod == 3 && remain >= 3) {
                uint8_t imm = c[2];
                const char *shifts[] = {"rol","ror","rcl","rcr","shl","shr","??","sar"};
                const char *mnem = (reg_f < 8) ? shifts[reg_f] : "???";
                strcpy(inst->mnemonic, mnem);
                snprintf(inst->operands, sizeof(inst->operands), "%s, %u",
                         regname64(rm), imm);
                inst->length = (int)(pos - start) + 3;
                return inst->length;
            }
            break;

        case 0xF7: /* neg/idiv/mul */
            if (mod == 3) {
                if (reg_f == 3) {
                    strcpy(inst->mnemonic, "neg");
                    snprintf(inst->operands, sizeof(inst->operands), "%s",
                             regname64(rm));
                } else if (reg_f == 7) {
                    strcpy(inst->mnemonic, "idiv");
                    snprintf(inst->operands, sizeof(inst->operands), "%s",
                             regname64(rm));
                } else if (reg_f == 0) {
                    strcpy(inst->mnemonic, "test");
                    snprintf(inst->operands, sizeof(inst->operands), "%s, ???",
                             regname64(rm));
                } else {
                    strcpy(inst->mnemonic, "f7");
                    snprintf(inst->operands, sizeof(inst->operands), "/%d %s",
                             reg_f, regname64(rm));
                }
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;

        case 0xFF: /* call/push/increment/jmp r/m */
            if (mod == 3) {
                if (reg_f == 2) {
                    strcpy(inst->mnemonic, "call");
                    snprintf(inst->operands, sizeof(inst->operands), "%s",
                             regname64(rm));
                } else if (reg_f == 6) {
                    strcpy(inst->mnemonic, "push");
                    snprintf(inst->operands, sizeof(inst->operands), "%s",
                             regname64(rm));
                } else {
                    strcpy(inst->mnemonic, "ff");
                    snprintf(inst->operands, sizeof(inst->operands), "/%d %s",
                             reg_f, regname64(rm));
                }
                inst->length = (int)(pos - start) + 2;
                return inst->length;
            }
            break;
        }
    }

    /* cqo (99) after REX.W */
    if (b == 0x99 && rex_w) {
        strcpy(inst->mnemonic, "cqo");
        inst->length = (int)(pos - start) + 1;
        return inst->length;
    }

    /* movabs r64, imm64 (B8+rd) — REX.W already parsed above, but
     * check for B9-BF range when REX.W is set (B8 caught earlier) */
    if (b >= 0xB9 && b <= 0xBF && rex_w && remain >= 9) {
        int rd2 = (b - 0xB8) | (rex_b ? 8 : 0);
        int64_t imm = read_i64(c + 1);
        strcpy(inst->mnemonic, "movabs");
        snprintf(inst->operands, sizeof(inst->operands), "%s, 0x%lx",
                 regname64(rd2), (unsigned long)imm);
        inst->length = (int)(pos - start) + 1 + 8;
        return inst->length;
    }

    /* Fallback: unknown instruction — emit as db byte */
    snprintf(inst->mnemonic, sizeof(inst->mnemonic), "db");
    snprintf(inst->operands, sizeof(inst->operands), "0x%02X", b);
    inst->length = (int)(pos - start) + 1;
    return inst->length;
}

int wdisasm_dump(const uint8_t *code, size_t code_len, FILE *out) {
    size_t offset = 0;
    int count = 0;
    while (offset < code_len) {
        WDisasmInst inst;
        int len = wdisasm_one(code, code_len, offset, &inst);
        if (len <= 0) break;
        fprintf(out, "%04zx: %-8s %s\n", offset, inst.mnemonic, inst.operands);
        offset += (size_t)len;
        count++;
    }
    return count;
}

int wdisasm_to_str(const uint8_t *code, size_t code_len,
                   char *buf, size_t bufsz) {
    size_t offset = 0;
    int written = 0;
    while (offset < code_len && (size_t)written < bufsz - 1) {
        WDisasmInst inst;
        int len = wdisasm_one(code, code_len, offset, &inst);
        if (len <= 0) break;
        int n = snprintf(buf + written, bufsz - (size_t)written,
                         "%04zx: %-8s %s\n", offset, inst.mnemonic, inst.operands);
        if (n < 0) break;
        written += n;
        offset += (size_t)len;
    }
    return written;
}
