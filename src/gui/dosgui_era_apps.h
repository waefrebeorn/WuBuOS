/*
 * dosgui_era_apps.h -- "One app per computing era" registry.
 *
 * Each era is tagged with the VSL syscall personality it exercises so the
 * WuBuOS desktop can prove EVERY OS personality is reachable as a
 * first-class process from the shell (not just compiled, but launchable +
 * routed). Declares the registration hook consumed by dosgui_startmenu.c.
 *
 * C11, minimal includes, self-contained.
 */

#ifndef DOSGUI_ERA_APPS_H
#define DOSGUI_ERA_APPS_H

#include <stdint.h>
#include <stdbool.h>

/* VSL personality class bits (mirror of vsl_syscall_dispatch routing). */
#define ERA_PERSONA_NATIVE   0x00   /* Linux ELF / VSL Linux table */
#define ERA_PERSONA_CPM      0xC0   /* CP/M 2.2 BDOS */
#define ERA_PERSONA_MACCLASS 0xB0   /* Classic Mac OS 68K A-line */
#define ERA_PERSONA_NT       0xFF   /* ReactOS NT / Win32 PE (NT path) */
#define ERA_PERSONA_XNU      0xE0   /* macOS XNU (Mach-O via darling) */
#define ERA_PERSONA_DOS      0xD0   /* in-process 8086 shim (separate emu) */
#define ERA_PERSONA_HOLYC    0xF0   /* HolyC / TempleOS JIT */

/* Register every era app into the shared start-menu program DB, tagging the
 * VSL personality so the shell can group/filter by era. Call once during
 * dosgui_startmenu_init(). */
void dosgui_era_apps_register(void);

/* Launch era app `idx` through its VSL personality's real exec backend
 * (DOS->8086 shim, Win->Wine, Linux->VSL ELF, HolyC->JIT). Gaps (CP/M,
 * Classic Mac) return -1 with a clear message. Returns 0 on a clean launch
 * attempt, -1 on setup error / unsupported gap. */
int dosgui_era_apps_launch(int idx);

/* Launch era app by its registry name (desktop click handler). */
int dosgui_era_apps_launch_by_name(const char *name);

/* Count of personalities actually runnable end-to-end on this host. */
int dosgui_era_apps_runnable_count(void);

/* Distinct VSL personality class bits represented (runnable or gap). */
int dosgui_era_apps_personality_coverage(uint32_t *out_personas, int max);

/* Human-readable label for a VSL personality class bit. */
const char *dosgui_era_personality_label(uint32_t p);

#endif /* DOSGUI_ERA_APPS_H */
