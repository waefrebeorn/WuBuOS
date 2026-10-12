/*
 * wubu_syscall_shim.c -- foreign-ABI syscall translation (Track D1 shim).
 *
 * On top of wubu_abi_path (which maps foreign PATHS onto /n/foreign), this
 * maps foreign SYSCALLS onto WuBu operations. A foreign app (Windows PE,
 * Unix, DOS) issues its own syscalls -- open/read/write/net/graphics -- and
 * a shim must translate each onto the WuBu surface: file ops become 9P
 * verbs on the translated path; graphics become the WuBu VBE/WM syscalls;
 * everything else is refused (fail-closed -- an unmapped foreign syscall is
 * never silently guessed).
 *
 * This module is the classification + dispatch decision, pure and testable:
 * given a foreign syscall category + its path argument, produce the WuBu
 * operation to perform and the translated namespace path. The actual 9P
 * open/read/write is the caller's job (links styxfs); this owns the mapping.
 *
 * C11, minimal includes.
 */

#include "wubu_syscall_shim.h"
#include "wubu_abi_path.h"
#include <string.h>

/* Classify a foreign syscall into a WuBu operation category. */
wubu_shu_op_t wubu_syscall_shim_classify(wubu_foreign_syscall_t sc) {
    switch (sc) {
    case WUBU_FSC_OPEN: case WUBU_FSC_READ: case WUBU_FSC_WRITE:
    case WUBU_FSC_CLOSE: case WUBU_FSC_STAT: case WUBU_FSC_SEEK:
        return WUBU_SHU_FILE;
    case WUBU_FSC_SOCKET: case WUBU_FSC_CONNECT: case WUBU_FSC_NET:
        return WUBU_SHU_NET;
    case WUBU_FSC_FILL_RECT: case WUBU_FSC_DRAW_TEXT: case WUBU_FSC_CREATE_WIN:
        return WUBU_SHU_GRAPHICS;
    default:
        return WUBU_SHU_REFUSE;   /* fail-closed: unmapped -> refuse */
    }
}

/* Translate a foreign file syscall: classify it, and for a FILE op translate
 * its path argument onto the namespace via wubu_abi_path. Returns the WuBu
 * operation; fills translated_path (namespace path) for FILE ops. For
 * non-file or refused ops, translated_path is set to "". */
wubu_shu_op_t wubu_syscall_shim_translate(wubu_abi_t abi,
                                          wubu_foreign_syscall_t sc,
                                          const char *path_arg,
                                          char *translated_path, size_t cap) {
    wubu_shu_op_t op = wubu_syscall_shim_classify(sc);
    if (translated_path && cap) translated_path[0] = '\0';
    if (op != WUBU_SHU_FILE) return op;
    if (!path_arg || !translated_path) return WUBU_SHU_REFUSE;
    if (wubu_abi_path_translate(abi, path_arg, translated_path, cap) != 0)
        return WUBU_SHU_REFUSE;
    return WUBU_SHU_FILE;
}

const char *wubu_syscall_shim_op_name(wubu_shu_op_t op) {
    switch (op) {
    case WUBU_SHU_FILE:     return "file";      /* 9P open/read/write */
    case WUBU_SHU_NET:      return "net";       /* 9P net verbs */
    case WUBU_SHU_GRAPHICS: return "graphics";  /* WuBu VBE/WM syscalls */
    case WUBU_SHU_REFUSE: default: return "refuse";  /* fail-closed */
    }
}
