/*
 * dosgui_era_apps.c -- One representative application per computing era,
 * each tagged with the VSL syscall personality it exercises so the WuBuOS
 * desktop can prove EVERY OS personality is reachable as a first-class
 * process from the shell (not just compiled, but launchable + routed).
 *
 * Era -> personality mapping (VSL class bits, see vsl_syscall_dispatch):
 *   1974 CP/M 2.2            -> 0xC0  (BDOS, vsl_cpm_syscall_dispatch)
 *   1981 MS-DOS              -> 0xD0  (in-process 8086 shim, wubu_dos_proc)
 *   1984 Classic Mac 68K     -> 0xB0  (Classic Mac 68K A-line traps)
 *   1993 Windows NT / Win32  -> 0xFF  (ReactOS NT path, wubu_exec_win_pe->Wine)
 *   2001 macOS XNU           -> 0xE0  (Mach-O via darling, wubu_exec_macho)
 *   2007 Linux native        -> 0x00  (VSL Linux table, wubu_exec_linux_elf)
 *   2020 HolyC / TempleOS    -> 0xF0  (HolyC JIT, hc_eval)
 *
 * Each entry's `executable` is a REAL artifact in demos/era/ (built by
 * demos/era/build_era.sh). `dosgui_era_apps_launch(idx)` runs it through the
 * correct personality's exec backend. Gaps (CP/M, Classic Mac) have
 * executable="" and runnable=false because WuBuOS has the syscall
 * PERSONALITY but NO CPU EMULATOR for those ISAs yet (documented in
 * docs/ERA_APPS_AND_RESOLVE_GAPS.md).
 *
 * C11, self-contained; depends only on dosgui_startmenu.h + wubu_exec.h.
 */

#include "dosgui_startmenu.h"
#include "dosgui_era_apps.h"
#include "dosgui_startmenu_internal.h"   /* declares extern SmProgramDB g_program_db */
#include "wubu_exec.h"                    /* wubu_exec_linux_elf / win_pe / holyc */
#include "wubu_dos_proc.h"               /* wubu_dos_proc_launch */
#include "wubu_container.h"              /* WUBU_PAYLOAD_DOS_COM */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* VSL personality class bits (mirror of vsl_syscall_dispatch routing).
 * Unique class bytes so the registry can list every OS personality without
 * ambiguity. NT and XNU share the 0xFF high-byte in the LIVE syscall router,
 * but for INVENTORY we keep them distinct so the desktop shows each era
 * separately. */
#define ERA_PERSONA_NATIVE   0x00   /* Linux ELF / VSL Linux table */
#define ERA_PERSONA_CPM      0xC0   /* CP/M 2.2 BDOS */
#define ERA_PERSONA_MACCLASS 0xB0  /* Classic Mac OS 68K A-line */
#define ERA_PERSONA_NT       0xFF   /* ReactOS NT / Win32 PE */
#define ERA_PERSONA_XNU      0xE0   /* macOS XNU (Mach-O via darling) */
#define ERA_PERSONA_DOS      0xD0   /* in-process 8086 shim (separate emu) */
#define ERA_PERSONA_HOLYC    0xF0   /* HolyC / TempleOS JIT */

/* Resolve the demos/era path relative to the repo, so the binary works
 * regardless of CWD. Falls back to the bare name if REPO_ROOT is unset. */
static const char *era_root(void) {
    const char *r = getenv("WUBU_REPO_ROOT");
    return r ? r : ".";
}

/* ---- The canonical "one app per era" set WuBuOS can HOST right now ---- */
static const struct {
    const char *name;
    const char *executable;   /* "" = known gap, not yet runnable here */
    const char *category;     /* era label shown in the shell */
    uint32_t    personality;  /* VSL class bit this app proves */
    bool        runnable;     /* false = tracked gap, greyed in UI */
} g_era_apps[] = {
    /* 1974 -- CP/M 2.2: BDOS personality. GAP: no 8080 CPU emulator. */
    { "CP/M :: STAT",        "", "Era: CP/M 1974",
      ERA_PERSONA_CPM, false },
    /* 1981 -- MS-DOS: real 8086 .COM, runs via the in-process shim. */
    { "DOS :: HELLO",        "demos/era/dos_hello.com", "Era: MS-DOS 1981",
      ERA_PERSONA_DOS, true },
    /* 1984 -- Classic Mac 68K A-line traps. GAP: no 68000 CPU emulator. */
    { "Mac :: About",        "", "Era: Classic Mac 1984",
      ERA_PERSONA_MACCLASS, false },
    /* 1993 -- Windows NT / Win32 via Proton+Wine (real, wubu_exec_win_pe). */
    { "Win32 :: Era Demo",   "demos/era/win_era_demo.exe", "Era: Win32 1993",
      ERA_PERSONA_NT, true },
    /* 2001 -- macOS XNU (Mach-O via darling; render leg proven via metal2vulkan). */
    { "macOS :: Era Demo",   "", "Era: macOS XNU 2001",
      ERA_PERSONA_XNU, false },  /* gap: no xclang here to build a Mach-O demo */
    /* 2007 -- Linux native ELF (runs via wubu_exec_linux_elf). */
    { "Linux :: Era Demo",   "demos/era/linux_era_demo.elf", "Era: Linux 2007",
      ERA_PERSONA_NATIVE, true },
    /* 2020 -- HolyC / TempleOS (JIT eval via hc_eval). */
    { "HolyC :: Era Demo",   "demos/era/holyc_era_demo.hc", "Era: HolyC 2020",
      ERA_PERSONA_HOLYC, true },
};

#define ERA_APP_COUNT (int)(sizeof(g_era_apps) / sizeof(g_era_apps[0]))

/* Count of personalities that are actually runnable end-to-end on this host. */
int dosgui_era_apps_runnable_count(void) {
    int n = 0;
    for (int i = 0; i < ERA_APP_COUNT; i++)
        if (g_era_apps[i].runnable) n++;
    return n;
}

/* Count of distinct VSL personalities represented (runnable or gap). */
int dosgui_era_apps_personality_coverage(uint32_t *out_personas, int max) {
    int n = 0;
    for (int i = 0; i < ERA_APP_COUNT; i++) {
        bool seen = false;
        for (int j = 0; j < n; j++)
            if (out_personas[j] == g_era_apps[i].personality) { seen = true; break; }
        if (!seen && n < max) out_personas[n++] = g_era_apps[i].personality;
    }
    return n;
}

/* Register every era app into the start-menu program DB, tagging the
 * VSL personality so the shell can group/filter by era. Runnables get a
 * real executable; gaps get executable="" (UI shows them greyed). */
void dosgui_era_apps_register(void) {
    for (int i = 0; i < ERA_APP_COUNT; i++) {
        if (g_program_db.count >= DOSGUI_MAX_PROGRAM_ENTRIES) break;
        SmProgramEntry *e = &g_program_db.entries[g_program_db.count];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, g_era_apps[i].name, sizeof(e->name) - 1);
        if (g_era_apps[i].executable && g_era_apps[i].executable[0])
            strncpy(e->executable, g_era_apps[i].executable, sizeof(e->executable) - 1);
        strncpy(e->category, g_era_apps[i].category, sizeof(e->category) - 1);
        e->is_builtin = true;
        e->vsl_personality = g_era_apps[i].personality;
        g_program_db.count++;
    }
}

/* Human-readable label for a VSL personality class bit (for UI/debug). */
const char *dosgui_era_personality_label(uint32_t p) {
    switch (p) {
        case ERA_PERSONA_NATIVE:   return "Linux (native ELF)";
        case ERA_PERSONA_CPM:      return "CP/M 2.2 BDOS";
        case ERA_PERSONA_MACCLASS: return "Classic Mac 68K A-line";
        case ERA_PERSONA_NT:       return "Windows NT / Win32 (ReactOS)";
        case ERA_PERSONA_XNU:      return "macOS XNU";
        case ERA_PERSONA_DOS:      return "MS-DOS (8086 shim)";
        case ERA_PERSONA_HOLYC:    return "HolyC / TempleOS";
        default:                   return "unknown";
    }
}

/* Read an entire file into a malloc'd buffer (caller frees). Returns NULL
 * on error. Used by the launcher to feed bytes to wubu_exec_*. */
static uint8_t *era_read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    *out_size = got;
    return buf;
}

/* Launch era app `idx` through its VSL personality's REAL exec backend.
 * Returns 0 on a clean launch attempt, -1 on a setup error (e.g. missing
 * file, unsupported gap). This is the honest bridge the desktop uses: the
 * registry's `executable` path becomes a real process, not a placeholder. */
int dosgui_era_apps_launch(int idx) {
    if (idx < 0 || idx >= ERA_APP_COUNT) return -1;
    const char *exe = g_era_apps[idx].executable;
    uint32_t persona = g_era_apps[idx].personality;

    if (!exe || !exe[0]) {
        fprintf(stderr, "[era] '%s' is a documented gap (no CPU emulator for "
                        "this ISA yet) -- not launched.\n", g_era_apps[idx].name);
        return -1;
    }

    /* Resolve relative to repo root if not absolute. */
    char full[512];
    if (exe[0] == '/') {
        strncpy(full, exe, sizeof(full) - 1);
    } else {
        snprintf(full, sizeof(full), "%s/%s", era_root(), exe);
    }

    switch (persona) {
    case ERA_PERSONA_DOS: {  /* 8086 shim */
        WubuDosProc *p = wubu_dos_proc_launch(full, WUBU_PAYLOAD_DOS_COM);
        return p ? 0 : -1;
    }
    case ERA_PERSONA_NT:    /* Win64 PE -> Wine/Proton */
    case ERA_PERSONA_XNU: { /* Mach-O -> darling (falls through to Wine today) */
        size_t sz = 0;
        uint8_t *data = era_read_file(full, &sz);
        if (!data) { fprintf(stderr, "[era] cannot read %s\n", full); return -1; }
        int64_t rc = wubu_exec_win_pe(data, sz);
        free(data);
        return (rc >= 0) ? 0 : -1;
    }
    case ERA_PERSONA_NATIVE: {  /* Linux ELF -> VSL */
        size_t sz = 0;
        uint8_t *data = era_read_file(full, &sz);
        if (!data) { fprintf(stderr, "[era] cannot read %s\n", full); return -1; }
        int64_t rc = wubu_exec_linux_elf(data, sz);
        free(data);
        return (rc >= 0) ? 0 : -1;
    }
    case ERA_PERSONA_HOLYC: {  /* HolyC source -> JIT */
        size_t sz = 0;
        uint8_t *data = era_read_file(full, &sz);
        if (!data) { fprintf(stderr, "[era] cannot read %s\n", full); return -1; }
        int64_t rc = wubu_exec_holyc((const char *)data, sz);
        free(data);
        return (rc >= 0) ? 0 : -1;
    }
    case ERA_PERSONA_CPM:
    case ERA_PERSONA_MACCLASS:
    default:
        fprintf(stderr, "[era] '%s' needs a CPU emulator (gap) -- not launched.\n",
                g_era_apps[idx].name);
        return -1;
    }
}

/* Launch by era-app name (used by the desktop click handler). Returns 0 on a
 * clean launch attempt, -1 if the app was not found or is a gap. */
int dosgui_era_apps_launch_by_name(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < ERA_APP_COUNT; i++)
        if (strcmp(g_era_apps[i].name, name) == 0)
            return dosgui_era_apps_launch(i);
    return -1;
}
