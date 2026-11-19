/*
 * wubu_macho_fmt.h -- Mach-O / universal-binary FORMAT constants.
 *
 * One owner for the on-disk format facts, shared by the in-kernel loader
 * (src/kernel/wubu_macho.c) and the VSL mac layer (src/runtime/vsl/
 * vsl_macho.h). Values are the Apple Mach-O specification's, not ours to
 * tune. Created by the org-wide hardcoding census (2026-11-19): the kernel
 * loader carried MH_MAGIC_64 = 0xFEEDFC02 (wrong -- it rejected every valid
 * 64-bit Mach-O) while VSL had the right 0xFEEDFACF. Two owners, one truth,
 * and the truth lived in only one of them.
 */
#ifndef WUBU_MACHO_FMT_H
#define WUBU_MACHO_FMT_H

#define MH_MAGIC      0xFEEDFACEu  /* 32-bit Mach-O */
#define MH_CIGAM      0xCEFAEDFEu  /* 32-bit byte-swapped */
#define MH_MAGIC_64   0xFEEDFACFu  /* 64-bit Mach-O */
#define MH_CIGAM_64   0xCFFAEDFEu  /* 64-bit byte-swapped */
#define FAT_MAGIC     0xCAFEBABEu  /* universal binary */
#define FAT_CIGAM     0xBEBAFECAu  /* universal byte-swapped */
#define FAT_MAGIC_64  0xCAFEBABFu  /* 64-bit universal */
#define FAT_CIGAM_64  0xBFBAFECAu  /* 64-bit universal byte-swapped */

#endif /* WUBU_MACHO_FMT_H */