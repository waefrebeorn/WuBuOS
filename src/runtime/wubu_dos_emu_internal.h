/* wubu_dos_emu_internal.h -- opaque engine internals for the 8086/DOS shim.
 * Forward declarations ONLY. Each .c file is self-contained: it defines the
 * functions it owns and includes this header for the cross-module prototypes.
 * No function bodies here (keeps modules independent + link-clean). */
#ifndef WUBU_DOS_EMU_INTERNAL_H
#define WUBU_DOS_EMU_INTERNAL_H
#include "wubu_dos_emu.h"
#include "wubu_dos_font.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#define F_CF 0x0001
#define F_PF 0x0004
#define F_AF 0x0010
#define F_ZF 0x0040
#define F_SF 0x0080
#define F_TF 0x0100
#define F_IF 0x0200
#define F_DF 0x0400
#define F_OF 0x0800

struct WubuDosEmu {
    uint8_t  mem[WUBU_DOS_MEM_SIZE];
    uint16_t es, cs, ss, ds;
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t ip;
    uint16_t flags;
    uint64_t steps;
    WubuDosEmuState state;
    int      exit_code;

    /* DOS video text model (80x25). */
    uint8_t  text[WUBU_DOS_TEXT_ROWS][WUBU_DOS_TEXT_COLS];
    uint8_t  attr[WUBU_DOS_TEXT_ROWS][WUBU_DOS_TEXT_COLS];
    int      cur_x, cur_y, cur_attr;

    /* Keyboard ring buffer (drained by INT 16h). */
    uint8_t  kbuf[64];   /* ASCII byte */
    uint8_t  kscan[64];  /* scancode (for INT 16h ah=0 -> AX = scan<<8 | ascii) */
    int      khead, ktail;
    uint8_t  kshift;     /* shift-state flags (INT 16h ah=2) */

    /* Host-backed DOS file table. Slot 0/1/2 are the console (stdin/stdout/
     * stderr); slots >=3 back real host files opened via INT 21h. Handle = slot+1. */
    int      host_fd[64];
    uint8_t  host_used[64];

    /* PSP segment (COM=0x1000, EXE=its own). Stored so INT 21h/62h returns it. */
    uint16_t psp_seg;

    /* Transient prefix state for the current instruction. */
    uint8_t  seg_prefix; /* 0 none, else ES/CS/SS/DS reg id+1 */
    uint8_t  rep_prefix; /* 0 none, 1 REP/REPE, 2 REPNE */
};

/* ---- forward declarations (no bodies) ---- */
int decode_main(WubuDosEmu *e, uint8_t op);
uint16_t *regp(WubuDosEmu *e, int i);
uint16_t reg_read(WubuDosEmu *e, int modrm, int w);
uint16_t pop16(WubuDosEmu *e);
void scroll_up(WubuDosEmu *e, int top, int left, int bot, int right, int lines, uint8_t attr);
int dos_handle_to_fd(WubuDosEmu *e, uint16_t h);
uint32_t phys(WubuDosEmu *e, uint16_t seg, uint16_t off);
uint8_t alu8(WubuDosEmu *e, int arith, uint8_t a, uint8_t b);
void int16(WubuDosEmu *e);
uint16_t seg_for(WubuDosEmu *e, int id);
void ww(WubuDosEmu *e, int i, uint16_t v);
uint8_t do_shift8(WubuDosEmu *e, int op, uint8_t v, int cnt);
void do_stos(WubuDosEmu *e, int w);
uint8_t rd8(WubuDosEmu *e, uint16_t s, uint16_t o);
void logic8(WubuDosEmu *e, uint8_t v);
uint16_t do_shift16(WubuDosEmu *e, int op, uint16_t v, int cnt);
int wubu_dos_emu_exit_code(const WubuDosEmu *e);
uint16_t rw(WubuDosEmu *e, int i);
void wubu_dos_emu_regs(const WubuDosEmu *e, uint16_t *ax, uint16_t *bx, uint16_t *cx, uint16_t *dx, uint16_t *si, uint16_t *di, uint16_t *ip, uint16_t *flags, uint16_t *cs);
void wr16(WubuDosEmu *e, uint16_t s, uint16_t o, uint16_t v);
int wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size);
void rm_write(WubuDosEmu *e, int modrm, int w, uint16_t v);
void wubu_dos_emu_destroy(WubuDosEmu *e);
void reg_write(WubuDosEmu *e, int modrm, int w, uint16_t v);
uint16_t rm_read(WubuDosEmu *e, int modrm, int w);
void push16(WubuDosEmu *e, uint16_t v);
void int10(WubuDosEmu *e);
int wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size);
void wb(WubuDosEmu *e, int i, uint8_t v);
int step(WubuDosEmu *e);
int getF(WubuDosEmu *e, uint16_t b);
uint16_t fetch16(WubuDosEmu *e);
size_t wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size);
int wubu_dos_emu_step(WubuDosEmu *e);
void do_movs(WubuDosEmu *e, int w);
uint16_t sub16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin);
void int21(WubuDosEmu *e);
uint8_t kbd_pop(WubuDosEmu *e);
int parity8(uint8_t v);
void wr8(WubuDosEmu *e, uint16_t s, uint16_t o, uint8_t v);
void do_scas(WubuDosEmu *e, int w);
void wubu_dos_emu_key(WubuDosEmu *e, uint8_t ascii);
uint8_t rb(WubuDosEmu *e, int i);
void ea(WubuDosEmu *e, int modrm, uint16_t *seg, uint16_t *off);
uint16_t alu16(WubuDosEmu *e, int arith, uint16_t a, uint16_t b);
void setF(WubuDosEmu *e, uint16_t b, int v);
WubuDosEmuState wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps);
uint16_t add16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin);
uint16_t rd16(WubuDosEmu *e, uint16_t s, uint16_t o);
uint8_t add8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin);
void do_lods(WubuDosEmu *e, int w);
WubuDosEmu *wubu_dos_emu_create(void);
uint16_t wubu_dos_emu_peek16(const WubuDosEmu *e, uint16_t seg, uint16_t off);
uint8_t sub8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin);
uint16_t dos_handle_alloc(WubuDosEmu *e, int host_fd);
uint8_t fetch8(WubuDosEmu *e);
size_t wubu_dos_emu_frame_rgba(const WubuDosEmu *e, uint8_t *rgba, int *out_w, int *out_h);
int cond_met(WubuDosEmu *e, int cc);
void do_cmps(WubuDosEmu *e, int w);
void logic16(WubuDosEmu *e, uint16_t v);
void put_char(WubuDosEmu *e, char c);

#endif
