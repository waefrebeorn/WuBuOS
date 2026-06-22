/*
 * wubu_syscall.h  --  WuBuOS HolyC Syscall Bridge
 *
 * 25 TempleOS/ZealOS-compatible syscalls exposed to HolyC.
 * Bridges HolyC compiler function table → kernel syscall table.
 *
 * Syscall Convention (System V AMD64 ABI):
 *   RAX = syscall number
 *   RDI, RSI, RDX, RCX, R8, R9 = args 1-6
 *   Returns I64 in RAX
 *
 * HolyC calls these as normal functions:
 *   I64 x = VBEFillRect(100, 100, 200, 200, 0xFF0000);
 *
 * The bridge registers them in HCGen.functions table with func_ptr
 * pointing to a syscall trampoline that does `syscall` instruction.
 */

#ifndef WUBU_SYSCALL_H
#define WUBU_SYSCALL_H

#include <stdint.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════
 * Syscall Numbers (matching TempleOS/ZealOS where possible)
 * ══════════════════════════════════════════════════════════════════ */

#define SYS_VBE_FILL_RECT        0   /* VBEFillRect(x, y, w, h, color) */
#define SYS_VBE_FILL_CIRCLE      1   /* VBEFillCircle(x, y, r, color) */
#define SYS_VBE_DRAW_TEXT        2   /* VBEDrawText(x, y, str, color, scale) */
#define SYS_VBE_DRAW_CHAR        3   /* VBEDrawChar(x, y, c, color, scale) */
#define SYS_VBE_VLINE            4   /* VBEVLine(x, y1, y2, color) */
#define SYS_VBE_HLINE            5   /* VBEHLine(x1, x2, y, color) */
#define SYS_VBE_TEXT_WIDTH       6   /* VBETextWidth(str, scale) */
#define SYS_VBE_SWAP             7   /* VBESwap() - double buffer flip */

#define SYS_WM_CREATE_WIN        8   /* WMCreateWin(x, y, w, h, title) */
#define SYS_WM_DESTROY_WIN       9   /* WMDestroyWin(id) */
#define SYS_WM_SET_FOCUS         10  /* WMSetFocus(id) */
#define SYS_WM_GET_FOCUSED       11  /* WMGetFocused() */
#define SYS_WM_RENDER            12  /* WMRender() */

#define SYS_FILE_OPEN            13  /* FileOpen(path, mode) */
#define SYS_FILE_READ            14  /* FileRead(fd, buf, len) */
#define SYS_FILE_WRITE           15  /* FileWrite(fd, buf, len) */
#define SYS_FILE_CLOSE           16  /* FileClose(fd) */

#define SYS_STYX_OPEN            17  /* StyxOpen(path, mode) */
#define SYS_STYX_READ            18  /* StyxRead(fid, offset, count, buf) */
#define SYS_STYX_WRITE           19  /* StyxWrite(fid, offset, count, buf) */

#define SYS_CONTAINER_CREATE     20  /* ContainerCreate(name, args, env) */
#define SYS_CONTAINER_DESTROY    21  /* ContainerDestroy(id) */
#define SYS_CONTAINER_EXEC       22  /* ContainerExec(id, cmd, args) */

#define SYS_WUBU_EXEC            23  /* WubuExec(data_ptr, size, filename_ptr) */

#define SYS_GET_TIME             24  /* GetTime() - returns TSC or ms */
#define SYS_SLEEP                25  /* Sleep(ms) */

#define SYS_MAX_DEFINED          26  /* Total defined syscalls */

/* ══════════════════════════════════════════════════════════════════
 * Kernel-side: syscall handler function signatures
 * ══════════════════════════════════════════════════════════════════ */

/* VBE Handlers */
int64_t sys_vbe_fill_rect(int64_t x, int64_t y, int64_t w, int64_t h, int64_t color, int64_t _unused);
int64_t sys_vbe_fill_circle(int64_t x, int64_t y, int64_t r, int64_t color, int64_t _unused, int64_t _unused2);
int64_t sys_vbe_draw_text(int64_t x, int64_t y, int64_t str_ptr, int64_t color, int64_t scale, int64_t _unused);
int64_t sys_vbe_draw_char(int64_t x, int64_t y, int64_t c, int64_t color, int64_t scale, int64_t _unused);
int64_t sys_vbe_vline(int64_t x, int64_t y1, int64_t y2, int64_t color, int64_t _unused, int64_t _unused2);
int64_t sys_vbe_hline(int64_t x1, int64_t x2, int64_t y, int64_t color, int64_t _unused, int64_t _unused2);
int64_t sys_vbe_text_width(int64_t str_ptr, int64_t scale, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4);
int64_t sys_vbe_swap(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6);

/* WM Handlers */
int64_t sys_wm_create_win(int64_t x, int64_t y, int64_t w, int64_t h, int64_t title_ptr, int64_t _unused);
int64_t sys_wm_destroy_win(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5);
int64_t sys_wm_set_focus(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5);
int64_t sys_wm_get_focused(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6);
int64_t sys_wm_render(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6);

/* File Handlers */
int64_t sys_file_open(int64_t path_ptr, int64_t mode, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4);
int64_t sys_file_read(int64_t fd, int64_t buf_ptr, int64_t len, int64_t _unused, int64_t _unused2, int64_t _unused3);
int64_t sys_file_write(int64_t fd, int64_t buf_ptr, int64_t len, int64_t _unused, int64_t _unused2, int64_t _unused3);
int64_t sys_file_close(int64_t fd, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5);

/* Styx Handlers */
int64_t sys_styx_open(int64_t path_ptr, int64_t mode, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4);
int64_t sys_styx_read(int64_t fid, int64_t offset, int64_t count, int64_t buf_ptr, int64_t _unused, int64_t _unused2);
int64_t sys_styx_write(int64_t fid, int64_t offset, int64_t count, int64_t buf_ptr, int64_t _unused, int64_t _unused2);

/* Container Handlers */
int64_t sys_container_create(int64_t name_ptr, int64_t args_ptr, int64_t env_ptr, int64_t _unused, int64_t _unused2, int64_t _unused3);
int64_t sys_container_destroy(int64_t id, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5);
int64_t sys_container_exec(int64_t id, int64_t cmd_ptr, int64_t args_ptr, int64_t _unused, int64_t _unused2, int64_t _unused3);

/* Universal Exec Handler */
int64_t sys_wubu_exec(int64_t data_ptr, int64_t size, int64_t filename_ptr, int64_t _unused, int64_t _unused2, int64_t _unused3);

/* Utility Handlers */
int64_t sys_get_time(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6);
int64_t sys_sleep(int64_t ms, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5);

/* ══════════════════════════════════════════════════════════════════
 * Registration API
 * ══════════════════════════════════════════════════════════════════ */

/* Register all 25 syscalls in kernel syscall table */
int wubu_syscall_register_all(void);

/* Setup HolyC compiler external function table */
/* Call this from HCCompiler initialization to expose syscalls to HolyC */
int wubu_holyc_register_syscalls(void *hc_compiler);

/* Syscall trampoline - assembly stub that does `syscall` instruction */
/* Returns address of syscall entry point for given syscall number */
void *wubu_syscall_trampoline(uint32_t num);

/* Map syscall name to number for HolyC external function registration */
const char *wubu_syscall_name(uint32_t num);

#endif /* WUBU_SYSCALL_H */