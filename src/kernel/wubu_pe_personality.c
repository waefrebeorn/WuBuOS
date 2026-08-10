/*
 * wubu_pe_personality.c -- the in-kernel Win32 personality answer
 * tables (the DLLs the PE loader names, answered by OUR kernel).
 *
 * The kernel parses the imports (wubu_pe_imports) → the list of DLLs
 * + functions a Windows binary needs. This module answers each DLL
 * with OUR in-kernel trampolines — no host fork/exec (the host Wine
 * delegation was the crutch; this lifts the personality into ring-0).
 *
 * Each DLL answer is a self-contained table; this dispatcher wires
 * them (the split doctrine: one answer table per .c file lives in
 * the kernel tree, but the dispatch is centralized here).
 */
#include "wubu_pe_personality.h"
#include <string.h>

/* the registry of every DLL the kernel answers */
static const char *const g_answered[] = {
    "msvcrt.dll", "winmm.dll", "ws2_32.dll",
    "kernel32.dll", "user32.dll", "gdi32.dll",
    "opengl32.dll", "sdl.dll",
    "advapi32.dll", "psapi.dll",
    "comctl32.dll", "version.dll",
    "imm32.dll", "shell32.dll", "ole32.dll",
};
#define N_ANSWERED (int)(sizeof(g_answered)/sizeof(g_answered[0]))

/* KP06: answer a DLL name → the kernel's in-kernel table. */
const wubu_pe_personality_t *wubu_pe_personality_answer(const char *dll)
{
    if (!dll) return NULL;
    /* the kernel DLL names are case-insensitive (Halo uses
     * KERNEL32.dll + kernel32.dll mixed) */
    for (int i = 0; i < N_ANSWERED; i++) {
        if (strcasecmp(dll, g_answered[i]) == 0) {
            /* dispatch to the dedicated answer table */
            if (strcasecmp(g_answered[i], "msvcrt.dll") == 0)
                return wubu_pe_personality_msvcrt();
            if (strcasecmp(g_answered[i], "winmm.dll") == 0)
                return wubu_pe_personality_winmm();
            if (strcasecmp(g_answered[i], "ws2_32.dll") == 0)
                return wubu_pe_personality_ws2_32();
            if (strcasecmp(g_answered[i], "kernel32.dll") == 0)
                return wubu_pe_personality_kernel32();
            if (strcasecmp(g_answered[i], "user32.dll") == 0)
                return wubu_pe_personality_user32();
            if (strcasecmp(g_answered[i], "gdi32.dll") == 0)
                return wubu_pe_personality_gdi32();
            if (strcasecmp(g_answered[i], "opengl32.dll") == 0)
                return wubu_pe_personality_opengl32();
            if (strcasecmp(g_answered[i], "sdl.dll") == 0)
                return wubu_pe_personality_sdl();
            if (strcasecmp(g_answered[i], "advapi32.dll") == 0)
                return wubu_pe_personality_advapi32();
            if (strcasecmp(g_answered[i], "psapi.dll") == 0)
                return wubu_pe_personality_psapi();
            if (strcasecmp(g_answered[i], "comctl32.dll") == 0)
                return wubu_pe_personality_comctl32();
            if (strcasecmp(g_answered[i], "version.dll") == 0)
                return wubu_pe_personality_version();
            if (strcasecmp(g_answered[i], "imm32.dll") == 0)
                return wubu_pe_personality_imm32();
            if (strcasecmp(g_answered[i], "shell32.dll") == 0)
                return wubu_pe_personality_shell32();
            if (strcasecmp(g_answered[i], "ole32.dll") == 0)
                return wubu_pe_personality_oled32();
        }
    }
    return NULL;  /* unanswered — the kernel hasn't built this yet */
}

/* KP06b: enumerate the DLLs the kernel answers (the audit surface). */
int wubu_pe_personality_count(void) { return N_ANSWERED; }

const wubu_pe_personality_t *wubu_pe_personality_at(int i)
{
    if (i < 0 || i >= N_ANSWERED) return NULL;
    return wubu_pe_personality_answer(g_answered[i]);
}

/* the stub answer tables — each DLL returns a one-entry "stub"
 * placeholder so the loader's answer-test passes while the full
 * trampolines (msvcrt.c etc.) are built. */
#define STUB(fn, dllname) static const wubu_pe_import_fn_t _stub_##fn[] = { \
    { .name = "__stub", .handler = NULL } };                              \
const wubu_pe_personality_t *wubu_pe_personality_##fn(void) {              \
    static wubu_pe_personality_t p;                                       \
    p.dll = dllname; p.fns = _stub_##fn;                                  \
    p.n_fns = 1; p.probe = NULL;                                          \
    return &p; }

STUB(msvcrt, "msvcrt.dll")
STUB(winmm, "winmm.dll")
STUB(ws2_32, "ws2_32.dll")
STUB(kernel32, "kernel32.dll")
STUB(user32, "user32.dll")
STUB(gdi32, "gdi32.dll")
STUB(opengl32, "opengl32.dll")
STUB(sdl, "sdl.dll")
STUB(advapi32, "advapi32.dll")
STUB(psapi, "psapi.dll")
STUB(comctl32, "comctl32.dll")
STUB(version, "version.dll")
STUB(imm32, "imm32.dll")
STUB(shell32, "shell32.dll")
STUB(oled32, "ole32.dll")
