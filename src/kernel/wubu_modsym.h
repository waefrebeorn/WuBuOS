/*
 * wubu_modsym.h -- runtime-extensible symbol registry for the PE loader.
 *
 * The in-kernel PE loader (wubu_pe.c) resolves a load's imports through a
 * `wubu_pe_resolve_fn` callback. P2 wires that callback to a REAL name->address
 * registry instead of a hardcoded test resolver: modules register symbols
 * (e.g. the NT personality exports its syscall handlers by Nt* name here), and
 * the loader's IAT snap looks imports up in the registry. Registering at
 * runtime (not compile-time) is the point -- a loaded holyd module or an NT
 * module can publish new symbols after boot and a subsequently-loaded PE can
 * bind to them.
 *
 * Allocator-free and self-contained (kernel-clean: -ffreestanding -nostdlib --
 * it only uses memcpy/strcasecmp/strcmp from libc_string, and no malloc). A
 * fixed table of {dll, name, addr} entries.
 */
#ifndef WUBU_MODSYM_H
#define WUBU_MODSYM_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_MODSYM_MAX 1024

typedef struct {
    char     dll[64];   /* module/namespace the symbol exports under, UPPER */
    char     name[128]; /* exported name (NtXxx), case-sensitive to match PE */
    uintptr_t addr;     /* resolved address (handler function pointer) */
    char     present;   /* slot occupied */
} wubu_modsym_entry_t;

/* Insert or RE-INSERT a symbol. Returns 0 on success, -1 if the registry is
 * full or a name is too long. If a slot already holds {dll,name}, it is
 * overwritten in place (re-binding / hot-swap). dll is uppercased internally;
 * name is kept verbatim. */
int wubu_modsym_register(const char *dll, const char *name, uintptr_t addr);

/* Look up {dll,name}. Returns 0 and sets *out on hit; nonzero on miss.
 * dll compare is case-insensitive (Windows DLL names are), name compare is
 * case-sensitive. */
int wubu_modsym_lookup(const char *dll, const char *name, uintptr_t *out);

/* Number of currently-registered symbols. */
int wubu_modsym_count(void);

/* Drop all entries (re-run nt_dispatch_init + re-export). */
void wubu_modsym_clear(void);

/* The resolver the PE loader uses. Implements wubu_pe_resolve_fn:
 *   return 0 on success (sets *out), nonzero on a lookup miss (image cannot
 *   load -- "dependency not found"). is_ordinal entries are NOT name-based;
 *   this registry is name-keyed, so an ordinal import is returned as unresolved
 *   (PE files may bind by name OR ordinal -- name is the common case and what
 *   the NT personality exports). */
int wubu_modsym_resolve(const char *dll, const char *func,
                        uint32_t ordinal, int is_ordinal,
                        uintptr_t *out);

#endif /* WUBU_MODSYM_H */
