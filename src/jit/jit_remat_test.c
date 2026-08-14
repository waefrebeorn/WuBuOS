/*
 * jit_remat_test.c  --  Verify constant rematerialization in the linear-scan
 * register allocator: a spilled constant vreg reloads as `mov reg, imm`
 * (no memory traffic), while a spilled non-constant reloads via `mov reg,[rbp]`.
 *
 * The discriminator between the two reload encodings is the OPCODE byte:
 *   - remat (constant):  mov r64, imm64  = opcode B8+rd (0xB8..0xBF)
 *   - memory (non-const):mov r64, r/m64  = opcode 0x8B
 * The REX prefix may be 0x48 (RAX..R8-free) or 0x49/0x4C/... (R10+) so we only
 * assert the W bit (0x08) is set and branch on the opcode byte.
 */
#include "x86_regalloc.h"
#include "wubu_x86.h"
#include <stdio.h>
#include <stdint.h>

static int pass, fail;
#define CHECK(cond, msg) do { \
    if (cond) { pass++; } \
    else { fail++; printf("FAIL: %s\n", msg); } \
} while (0)

int main(void) {
    Wx86Enc e;

    /* --- Case 1: constant vreg spilled, then reloaded → remat as immediate --- */
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        int spilled_vreg = -1;
        for (int v = 0; v < 20; v++) {
            Wx86Reg hw = xra_alloc(&ra, v);
            if (hw == WREG_NONE) { spilled_vreg = v; }
        }
        CHECK(spilled_vreg >= 0, "at least one vreg spilled under load");

        /* Free one allocated register so the reload has a home
         * (minic frees the LHS vreg after its binop). */
        for (int v = 0; v < spilled_vreg; v++) {
            Wx86Reg hw = xra_get_reg(&ra, v);
            if (hw != WREG_NONE) { xra_free_reg(&ra, hw); break; }
        }

        xra_mark_const(&ra, spilled_vreg, 0x1234567890ABCDEFLL);

        wx86_enc_init_dynamic(&e, 64);
        Wx86Reg hw = xra_spill_load(&ra, spilled_vreg, &e);
        CHECK(hw != WREG_NONE, "spill reload allocates a register");
        CHECK((e.buf[0] & 0x08) != 0, "REX.W set (64-bit operand)");
        /* mov r64, imm64 opcode = B8..BF */
        CHECK((e.buf[1] & 0xF8) == 0xB8, "remat: opcode B8+rd (mov r64,imm64), NOT 8B memory load");
        /* Verify the immediate value round-trips. */
        uint64_t imm = 0;
        for (int i = 0; i < 8; i++) imm |= ((uint64_t)e.buf[2+i]) << (8*i);
        CHECK(imm == 0x1234567890ABCDEFLL, "remat immediate value correct");
        CHECK(e.pos == 10, "remat is a compact 10-byte mov r64,imm64");
        wx86_enc_free(&e);
    }

    /* --- Case 2: non-constant vreg spilled, reloaded → memory load (mov r,[rbp]) --- */
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        int spilled_vreg = -1;
        for (int v = 0; v < 20; v++) {
            Wx86Reg hw = xra_alloc(&ra, v);
            if (hw == WREG_NONE) { spilled_vreg = v; }
        }
        CHECK(spilled_vreg >= 0, "pool exhausts (non-const case)");
        for (int v = 0; v < spilled_vreg; v++) {
            Wx86Reg hw = xra_get_reg(&ra, v);
            if (hw != WREG_NONE) { xra_free_reg(&ra, hw); break; }
        }
        wx86_enc_init_dynamic(&e, 64);
        Wx86Reg hw = xra_spill_load(&ra, spilled_vreg, &e);
        CHECK(hw != WREG_NONE, "non-const spill reload allocates");
        CHECK((e.buf[0] & 0x08) != 0, "REX.W set (non-const)");
        CHECK(e.buf[1] == 0x8B, "non-const: opcode 8B (mov r64,r/m64) = memory load");
        wx86_enc_free(&e);
    }

    /* --- Case 3: xra_is_const reflects the mark --- */
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        CHECK(!xra_is_const(&ra, 0), "fresh vreg not marked constant");
        xra_mark_const(&ra, 0, 42);
        CHECK(xra_is_const(&ra, 0), "xra_mark_const sets the flag");
        xra_mark_const(&ra, 5, -7);
        CHECK(xra_is_const(&ra, 5) && xra_is_const(&ra, 0), "independent per-vreg flags");
    }

    printf("\n=== jit_remat_test: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
