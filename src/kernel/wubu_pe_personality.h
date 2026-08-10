/*
 * wubu_pe_personality.h -- the IN-KERNEL Win32 personality answer
 * tables (the DLLs the PE loader names, answered by OUR kernel).
 *
 * The user's correction: "everything is kernel development — lift
 * all the ZealOS and Arch into OUR kernel." The host Wine/Proton
 * delegation (wubu_exec.c line 27: "VSL is now HOST DELEGATION")
 * is the crutch; THIS is the in-kernel answer.
 *
 * The PE loader (wubu_pe.c) parses the imports → the list of DLLs
 * + functions a Windows binary needs. This header defines OUR
 * personality: for each DLL name, an import-resolver the kernel
 * implements in ring-0 (no host fork/exec).
 *
 * Opaque: the caller sees only the resolver table; the implementations
 * live in the .c files (msvcrt.c, winmm.c, ws2_32.c, opengl32_stub.c,
 * sdl_inproc.c — each self-contained). No god header.
 */
#ifndef WUBU_PE_PERSONALITY_H
#define WUBU_PE_PERSONALITY_H

#include "wubu_pe.h"

/* one import the kernel must answer */
typedef struct {
    const char *name;       /* the function name (e.g. "CreateFileA") */
    void *handler;          /* the in-kernel trampoline (opaque) */
} wubu_pe_import_fn_t;

/* one DLL → the kernel's answer table */
typedef struct {
    const char *dll;        /* e.g. "KERNEL32.dll" */
    wubu_pe_import_fn_t *fns; /* the table of functions */
    int n_fns;
    void *(*probe)(void);   /* optional: returns the DLL base */
} wubu_pe_personality_t;

/* KP06: answer the imports the PE loader named. Returns the resolver
 * for a DLL, or NULL (unanswered) if the kernel lacks it. The caller
 * (the in-kernel Win32 exec) walks the answer. */
const wubu_pe_personality_t *
wubu_pe_personality_answer(const char *dll_name);

/* KP06b: enumerate every DLL the kernel answers (the audit surface). */
int wubu_pe_personality_count(void);
const wubu_pe_personality_t *wubu_pe_personality_at(int i);

/* the in-kernel answer tables — one per DLL. Each is built by the
 * dedicated module (self-contained, the split doctrine): */
const wubu_pe_personality_t *wubu_pe_personality_msvcrt(void);
const wubu_pe_personality_t *wubu_pe_personality_winmm(void);
const wubu_pe_personality_t *wubu_pe_personality_ws2_32(void);
const wubu_pe_personality_t *wubu_pe_personality_kernel32(void);
const wubu_pe_personality_t *wubu_pe_personality_user32(void);
const wubu_pe_personality_t *wubu_pe_personality_gdi32(void);
const wubu_pe_personality_t *wubu_pe_personality_opengl32(void);
const wubu_pe_personality_t *wubu_pe_personality_sdl(void);
const wubu_pe_personality_t *wubu_pe_personality_advapi32(void);
const wubu_pe_personality_t *wubu_pe_personality_psapi(void);
const wubu_pe_personality_t *wubu_pe_personality_comctl32(void);
const wubu_pe_personality_t *wubu_pe_personality_version(void);
const wubu_pe_personality_t *wubu_pe_personality_imm32(void);
const wubu_pe_personality_t *wubu_pe_personality_shell32(void);
const wubu_pe_personality_t *wubu_pe_personality_oled32(void);

#endif
