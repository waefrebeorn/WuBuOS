/*
 * test_arm64_enc.c — Verify ARM64 encoder emits correct bytes.
 * Compares against known-good encodings from ARM reference.
 */
#include "wubu_arm64.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass, fail, total;
#define CHECK(cond, msg) do{total++;if(cond){pass++;}else{fail++;printf("  FAIL: %s\n",msg);}}while(0)

static int bytes_eq(const uint8_t *got, const uint8_t *exp, size_t n) {
    return memcmp(got, exp, n) == 0;
}

int main(void) {
    WArm64Enc enc;
    uint8_t buf[256];

    printf("=== ARM64 ENCODER TEST ===\n\n");

    /* MOVZ X0, #42 = 0xD2800540 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_mov_imm(&enc, WREG_X0, 42);
    CHECK(enc.pos == 4, "mov x0, #42: 4 bytes");
    CHECK(buf[0]==0x40 && buf[1]==0x05 && buf[2]==0x80 && buf[3]==0xD2,
          "mov x0, #42: correct encoding");

    /* MOVZ X1, #0x1234 = 0xD2824681 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_movz_imm(&enc, WREG_X1, 0x1234, 0, 1);
    CHECK(buf[0]==0x81 && buf[1]==0x46 && buf[2]==0x82 && buf[3]==0xD2,
          "movz x1, #0x1234: correct encoding");

    /* RET X30 = 0xD65F03C0 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_ret(&enc, WREG_LR);
    CHECK(buf[0]==0xC0 && buf[1]==0x03 && buf[2]==0x5F && buf[3]==0xD6,
          "ret x30: correct encoding");

    /* ADD X0, X1, #1 = 0x91000420 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_add_imm(&enc, WREG_X0, WREG_X1, 1, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x04 && buf[2]==0x00 && buf[3]==0x91,
          "add x0, x1, #1: correct encoding");

    /* SUB X0, X0, #1 = 0xD1000400 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_sub_imm(&enc, WREG_X0, WREG_X0, 1, 1);
    CHECK(buf[0]==0x00 && buf[1]==0x04 && buf[2]==0x00 && buf[3]==0xD1,
          "sub x0, x0, #1: correct encoding");

    /* ADD X0, X1, X2 = 0x8B020020 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_add_reg(&enc, WREG_X0, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0x8B,
          "add x0, x1, x2: correct encoding");

    /* SUB X0, X1, X2 = 0xCB020020 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_sub_reg(&enc, WREG_X0, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0xCB,
          "sub x0, x1, x2: correct encoding");

    /* AND X0, X1, X2 = 0x8A020020 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_and_reg(&enc, WREG_X0, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0x8A,
          "and x0, x1, x2: correct encoding");

    /* ORR X0, X1, X2 = 0xAA020020 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_orr_reg(&enc, WREG_X0, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0xAA,
          "orr x0, x1, x2: correct encoding");

    /* EOR X0, X1, X2 = 0xCA020020 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_eor_reg(&enc, WREG_X0, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0xCA,
          "eor x0, x1, x2: correct encoding");

    /* CMP X1, X2 = SUBS XZR, X1, X2 = 0xEB02003F */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_cmp_reg(&enc, WREG_X1, WREG_X2, 1);
    CHECK(buf[0]==0x3F && buf[1]==0x00 && buf[2]==0x02 && buf[3]==0xEB,
          "cmp x1, x2: correct encoding");

    /* MOV X0, X1 = ORR X0, XZR, X1 = 0xAA0103E0 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_mov_reg(&enc, WREG_X0, WREG_X1);
    CHECK(buf[0]==0xE0 && buf[1]==0x03 && buf[2]==0x01 && buf[3]==0xAA,
          "mov x0, x1: correct encoding");

    /* B #0 (infinite loop) = 0x14000000 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_b_uncond(&enc, 0);
    CHECK(buf[0]==0x00 && buf[1]==0x00 && buf[2]==0x00 && buf[3]==0x14,
          "b .: correct encoding");

    /* B.cond EQ, #0 = 0x54000000 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_b_cond(&enc, 0, WCC_EQ);
    CHECK(buf[0]==0x00 && buf[1]==0x00 && buf[2]==0x00 && buf[3]==0x54,
          "b.eq .: correct encoding");

    /* LDR X0, [X1, #8] = 0xF9400420 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_ldr_imm(&enc, WREG_X0, WREG_X1, 8, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x04 && buf[2]==0x40 && buf[3]==0xF9,
          "ldr x0, [x1, #8]: correct encoding");

    /* STR X0, [X1, #8] = 0xF9000420 */
    warm64_enc_init(&enc, buf, sizeof(buf));
    warm64_str_imm(&enc, WREG_X0, WREG_X1, 8, 1);
    CHECK(buf[0]==0x20 && buf[1]==0x04 && buf[2]==0x00 && buf[3]==0xF9,
          "str x0, [x1, #8]: correct encoding");

    printf("\n=== SUMMARY ===\n");
    printf("=== test_arm64_enc: %d/%d passed, %d failed ===\n", pass, total, fail);
    return fail ? 1 : 0;
}
