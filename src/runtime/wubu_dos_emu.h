/*
 * wubu_dos_emu.h -- WuBuOS in-process 16-bit DOS compatibility shim.
 *
 * A REAL 8086 interpreter + DOS INT layer that runs .COM/.EXE programs
 * INSIDE WuBuOS as ordinary WuBuOS processes -- no separate OS is booted,
 * no QEMU guest, no disk image. This is the "compatible window" engine:
 * the program's text/video output is captured into a text buffer and an
 * RGBA frame that the WuBuOS desktop window blits.
 *
 * Design:
 *   - 1 MB real-mode flat address space (seg*16 + off).
 *   - Full 16-bit register file + FLAGS (CF/ZF/SF/OF/PF/AF tracked).
 *   - Decodes a practical 8086 opcode subset (the set real DOS programs
 *     actually use): MOV/PUSH/POP, ADD/SUB/ADC/SBB/AND/OR/XOR/CMP/TEST,
 *     INC/DEC/NEG/NOT, shifts, MUL/DIV/IMUL/IDIV, string ops (MOVS/STOS/
 *     LODS/SCAS) with REP, jumps/loops/conditions, CALL/RET, INT/IRET,
 *     segment prefixes, LEA, XLAT, CBW/CWD, etc.
 *   - INT 21h (DOS), INT 10h (video text), INT 16h (keyboard) handled
 *     natively; other vectors jump through the real IVT so TSRs work.
 *   - Text screen model (80x25, char+attr) + RGBA renderer (font in
 *     wubu_dos_font.h) so the window has a real frame to show.
 *
 * Nothing here is a stub: load -> run -> capture are all real work.
 * Opaque handle; minimal includes; C11.
 */
#ifndef WUBU_DOS_EMU_H
#define WUBU_DOS_EMU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 1 MB real-mode address space. */
#define WUBU_DOS_MEM_SIZE    (1024u * 1024u)
/* Conventional text mode. */
#define WUBU_DOS_TEXT_COLS   80
#define WUBU_DOS_TEXT_ROWS   25
/* Rendered cell geometry. */
#define WUBU_DOS_CELL_W      8
#define WUBU_DOS_CELL_H      16

typedef enum {
    WUBU_DOS_RUNNING    = 0,
    WUBU_DOS_TERMINATED = 1,
    WUBU_DOS_ERROR      = 2
} WubuDosEmuState;

typedef struct WubuDosEmu WubuDosEmu;

/* Create/destroy an emulator with a 1 MB address space. */
WubuDosEmu *wubu_dos_emu_create(void);
void        wubu_dos_emu_destroy(WubuDosEmu *e);

/*
 * Load a program image.
 *   - load_com: raw .COM (origin CS:0x100 inside a PSP segment).
 *   - load_exe: MZ .EXE (header-parsed, relocations applied).
 * Returns 0 on success, -1 on a bad/unsupported image.
 */
int wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size);
int wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size);

/*
 * Run up to max_steps instructions. max_steps == 0 means run until the
 * program terminates or hits an error. Returns the resulting state.
 */
WubuDosEmuState wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps);

/* Inject an ASCII key into the keyboard buffer (drained by INT 16h). */
void wubu_dos_emu_key(WubuDosEmu *e, uint8_t ascii);

/* Read the captured text screen (NUL-terminated, rows joined by '\n').
 * Returns characters written (excluding NUL). */
size_t wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size);

/* Render the text screen to RGBA32 (cols*CELL_W x rows*CELL_H). Returns
 * bytes written (0 on error). rgba must hold the full frame. */
size_t wubu_dos_emu_frame_rgba(const WubuDosEmu *e, uint8_t *rgba,
                               int *out_w, int *out_h);

/* Exit code from INT 21h/4Ch (AL). */
int wubu_dos_emu_exit_code(const WubuDosEmu *e);
/* Raw memory peek (for tests / introspection). */
uint16_t wubu_dos_emu_peek16(const WubuDosEmu *e, uint16_t seg, uint16_t off);

/* Single-instruction step (exposed for unit tests). Returns 0 ok, -1 halt. */
int wubu_dos_emu_step(WubuDosEmu *e);

/* CPU register snapshot for tests (any pointer may be NULL). */
void wubu_dos_emu_regs(const WubuDosEmu *e,
                       uint16_t *ax, uint16_t *bx, uint16_t *cx, uint16_t *dx,
                       uint16_t *si, uint16_t *di, uint16_t *ip, uint16_t *flags,
                       uint16_t *cs);

#endif /* WUBU_DOS_EMU_H */
