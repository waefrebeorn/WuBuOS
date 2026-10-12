/*
 * wubu_syscall_shim.h -- foreign-ABI syscall translation (Track D1 shim).
 *
 * Maps a foreign app's syscalls onto WuBu operations: file ops become 9P
 * verbs on the translated path (via wubu_abi_path), graphics become the
 * WuBu VBE/WM syscalls, net becomes 9P net verbs, and everything else is
 * REFUSED (fail-closed -- an unmapped foreign syscall is never guessed).
 */
#ifndef WUBU_SYSCALL_SHIM_H
#define WUBU_SYSCALL_SHIM_H

#include <stddef.h>
#include "wubu_abi_path.h"   /* wubu_abi_t */

/* A foreign syscall, abstracted across ABIs (the shim normalizes each
 * ABI's numbering onto this set). */
typedef enum {
    WUBU_FSC_OPEN = 0, WUBU_FSC_READ, WUBU_FSC_WRITE, WUBU_FSC_CLOSE,
    WUBU_FSC_STAT, WUBU_FSC_SEEK,
    WUBU_FSC_SOCKET, WUBU_FSC_CONNECT, WUBU_FSC_NET,
    WUBU_FSC_FILL_RECT, WUBU_FSC_DRAW_TEXT, WUBU_FSC_CREATE_WIN,
    WUBU_FSC_UNKNOWN
} wubu_foreign_syscall_t;

/* The WuBu operation a foreign syscall maps onto. */
typedef enum {
    WUBU_SHU_FILE = 0,   /* 9P open/read/write on the translated path */
    WUBU_SHU_NET,        /* 9P net verbs */
    WUBU_SHU_GRAPHICS,   /* WuBu VBE/WM syscalls */
    WUBU_SHU_REFUSE      /* fail-closed: unmapped -> refuse */
} wubu_shu_op_t;

/* Classify a foreign syscall into a WuBu operation category. */
wubu_shu_op_t wubu_syscall_shim_classify(wubu_foreign_syscall_t sc);

/* Translate a foreign file syscall: classify + (for FILE ops) translate the
 * path onto the namespace. Fills translated_path for FILE ops ("" otherwise).
 * Returns the WuBu operation (WUBU_SHU_REFUSE on failure). */
wubu_shu_op_t wubu_syscall_shim_translate(wubu_abi_t abi,
                                          wubu_foreign_syscall_t sc,
                                          const char *path_arg,
                                          char *translated_path, size_t cap);

/* "file" / "net" / "graphics" / "refuse" */
const char *wubu_syscall_shim_op_name(wubu_shu_op_t op);

#endif /* WUBU_SYSCALL_SHIM_H */
