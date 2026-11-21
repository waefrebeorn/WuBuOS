/* guest_libc_big.c -- bounded GUEST libc provider with a REAL-SIZED version table.
 *
 * Includes the working freestanding libc shim (puts/__libc_start_main/
 * __cxa_finalize/optind) and adds one dummy definition per GLIBC version node
 * from the host's REAL libc.so.6, so DT_VERDEFNUM == 40
 * (same as a REAL glibc libc.so.6).  This exercises the raised, explicitly
 * bounded version tables: COPY/GLOB_DAT/JUMP_SLOT must still bind by DT_VERNEED
 * name + vn_file with a table larger than the old 32-entry cap.  Freestanding:
 * no host libc, no ld.so, no TLS, no IFUNC -- all bytes execute inside VSL.
 */
#include "guest_libc_shim.c"

int vsl_bigver_00 = 1;
__asm__(".symver vsl_bigver_00,bigver_00@@GLIBC_2.2.5");
int vsl_bigver_01 = 2;
__asm__(".symver vsl_bigver_01,bigver_01@@GLIBC_2.2.6");
int vsl_bigver_02 = 3;
__asm__(".symver vsl_bigver_02,bigver_02@@GLIBC_2.3");
int vsl_bigver_03 = 4;
__asm__(".symver vsl_bigver_03,bigver_03@@GLIBC_2.3.2");
int vsl_bigver_04 = 5;
__asm__(".symver vsl_bigver_04,bigver_04@@GLIBC_2.3.3");
int vsl_bigver_05 = 6;
__asm__(".symver vsl_bigver_05,bigver_05@@GLIBC_2.3.4");
int vsl_bigver_06 = 7;
__asm__(".symver vsl_bigver_06,bigver_06@@GLIBC_2.4");
int vsl_bigver_07 = 8;
__asm__(".symver vsl_bigver_07,bigver_07@@GLIBC_2.5");
int vsl_bigver_08 = 9;
__asm__(".symver vsl_bigver_08,bigver_08@@GLIBC_2.6");
int vsl_bigver_09 = 10;
__asm__(".symver vsl_bigver_09,bigver_09@@GLIBC_2.7");
int vsl_bigver_10 = 11;
__asm__(".symver vsl_bigver_10,bigver_10@@GLIBC_2.8");
int vsl_bigver_11 = 12;
__asm__(".symver vsl_bigver_11,bigver_11@@GLIBC_2.9");
int vsl_bigver_12 = 13;
__asm__(".symver vsl_bigver_12,bigver_12@@GLIBC_2.10");
int vsl_bigver_13 = 14;
__asm__(".symver vsl_bigver_13,bigver_13@@GLIBC_2.11");
int vsl_bigver_14 = 15;
__asm__(".symver vsl_bigver_14,bigver_14@@GLIBC_2.12");
int vsl_bigver_15 = 16;
__asm__(".symver vsl_bigver_15,bigver_15@@GLIBC_2.13");
int vsl_bigver_16 = 17;
__asm__(".symver vsl_bigver_16,bigver_16@@GLIBC_2.14");
int vsl_bigver_17 = 18;
__asm__(".symver vsl_bigver_17,bigver_17@@GLIBC_2.15");
int vsl_bigver_18 = 19;
__asm__(".symver vsl_bigver_18,bigver_18@@GLIBC_2.16");
int vsl_bigver_19 = 20;
__asm__(".symver vsl_bigver_19,bigver_19@@GLIBC_2.17");
int vsl_bigver_20 = 21;
__asm__(".symver vsl_bigver_20,bigver_20@@GLIBC_2.18");
int vsl_bigver_21 = 22;
__asm__(".symver vsl_bigver_21,bigver_21@@GLIBC_2.22");
int vsl_bigver_22 = 23;
__asm__(".symver vsl_bigver_22,bigver_22@@GLIBC_2.23");
int vsl_bigver_23 = 24;
__asm__(".symver vsl_bigver_23,bigver_23@@GLIBC_2.24");
int vsl_bigver_24 = 25;
__asm__(".symver vsl_bigver_24,bigver_24@@GLIBC_2.25");
int vsl_bigver_25 = 26;
__asm__(".symver vsl_bigver_25,bigver_25@@GLIBC_2.26");
int vsl_bigver_26 = 27;
__asm__(".symver vsl_bigver_26,bigver_26@@GLIBC_2.27");
int vsl_bigver_27 = 28;
__asm__(".symver vsl_bigver_27,bigver_27@@GLIBC_2.28");
int vsl_bigver_28 = 29;
__asm__(".symver vsl_bigver_28,bigver_28@@GLIBC_2.29");
int vsl_bigver_29 = 30;
__asm__(".symver vsl_bigver_29,bigver_29@@GLIBC_2.30");
int vsl_bigver_30 = 31;
__asm__(".symver vsl_bigver_30,bigver_30@@GLIBC_2.31");
int vsl_bigver_31 = 32;
__asm__(".symver vsl_bigver_31,bigver_31@@GLIBC_2.32");
int vsl_bigver_32 = 33;
__asm__(".symver vsl_bigver_32,bigver_32@@GLIBC_2.33");
int vsl_bigver_33 = 34;
__asm__(".symver vsl_bigver_33,bigver_33@@GLIBC_2.34");
int vsl_bigver_34 = 35;
__asm__(".symver vsl_bigver_34,bigver_34@@GLIBC_2.35");
int vsl_bigver_35 = 36;
__asm__(".symver vsl_bigver_35,bigver_35@@GLIBC_2.36");
int vsl_bigver_36 = 37;
__asm__(".symver vsl_bigver_36,bigver_36@@GLIBC_2.38");
int vsl_bigver_37 = 38;
__asm__(".symver vsl_bigver_37,bigver_37@@GLIBC_2.39");
int vsl_bigver_38 = 39;
__asm__(".symver vsl_bigver_38,bigver_38@@GLIBC_ABI_DT_RELR");
int vsl_bigver_39 = 40;
__asm__(".symver vsl_bigver_39,bigver_39@@GLIBC_PRIVATE");
