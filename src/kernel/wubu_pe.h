/*
 * wubu_pe.h -- the IN-KERNEL PE loader.
 */
#ifndef WUBU_PE_H
#define WUBU_PE_H

#include <stdint.h>
#include <stddef.h>

/* the parsed PE info */
typedef struct {
    uint16_t machine;
    uint16_t num_sections;
    uint16_t optional_size;
    uint16_t subsystem;      /* 2=GUI 3=CUI */
    uint16_t is_pe32plus;    /* magic 0x20B (x64) vs 0x10B (PE32) */
    uint32_t entry_rva;
    uint32_t image_base;
    uint32_t import_rva;
    uint32_t import_size;
    uint32_t reloc_rva;
    uint32_t reloc_size;
    uint32_t tls_rva;      /* data directory 9: TLS (.tls) */
    uint32_t tls_size;
    uint32_t pe_offset;
} wubu_pe_info_t;

/* one mapped section */
typedef struct {
    char     name[9];
    uint32_t virtual_size;
    uint32_t virtual_addr;
    uint32_t raw_size;
    uint32_t raw_ptr;
    uint32_t characteristics;
} wubu_pe_section_t;

/* one import descriptor (a DLL the game needs) */
typedef struct {
    char dll[64];
    int  func_count;
} wubu_pe_import_t;

/* PE1: validate + parse the headers. 0 on success. */
int wubu_pe_parse(const void *data, size_t size, wubu_pe_info_t *out);

/* the loader resolves one import (by name or by ordinal) to its address.
 * Return 0 on success (setting *out), nonzero on a lookup miss (the image
 * cannot load — a "dependency not found" condition). dll is the DLL name,
 * func the function name (empty when is_ordinal), ordinal the ordinal. */
typedef int (*wubu_pe_resolve_fn)(const char *dll, const char *func,
                                  uint32_t ordinal, int is_ordinal,
                                  uintptr_t *out);

/* PE2: walk the section table. Returns the count. */
int wubu_pe_sections(const void *data, size_t size,
                     const wubu_pe_info_t *info,
                     wubu_pe_section_t *out, int max);

/* PE3: resolve an RVA to the raw file offset (the mapping plan). */
int wubu_pe_rva_to_raw(const wubu_pe_section_t *sects, int n,
                       uint32_t rva);

/* PE4: walk the import descriptors. Returns the count. */
int wubu_pe_imports(const void *data, size_t size,
                    const wubu_pe_info_t *info,
                    const wubu_pe_section_t *sects, int nsects,
                    wubu_pe_import_t *out, int max);

/* PE5: apply base relocations (.reloc data directory) to an image already
 * mapped at `load_base`.  Every HIGHLOW(3)/DIR64(10) fixup slot is raised by
 * delta = load_base - info->image_base (the ReactOS loader's relocation step).
 * No-op (0) when the image has no relocation directory. Returns 0 on success,
 * -1 on a malformed block. */
int wubu_pe_relocate(const wubu_pe_info_t *info,
                     const wubu_pe_section_t *sects, int nsects,
                     const void *file, size_t file_size,
                     void *image, uintptr_t load_base);

/* PE9: run the TLS callbacks recorded in the .tls directory (the image's
 * thread-aware init functions, each a function pointer; the array is
 * NULL-terminated). Callbacks are invoked in array order. Returns how many
 * callbacks ran (0 when the image has no .tls directory). */
int wubu_pe_tls_callbacks(const wubu_pe_info_t *info,
                          const wubu_pe_section_t *sects, int nsects,
                          void *image);

/* PE10: finish loading a PE image that has been mapped at load_base.
 * Runs any TLS callbacks first (ReactOS LdrpInitializeTls), then transfers to
 * the entry point (LdrpFetchAddressOfEntryPoint) and returns its return value.
 * Returns -1 if the image has no valid entry. */
int wubu_pe_call_entry(const wubu_pe_info_t *info,
                       const wubu_pe_section_t *sects, int nsects,
                       void *image);

/* PE11: snap (patch) the import address table of a MAPPED image. For each
 * import descriptor's thunks (resolved via the OriginalFirstThunk chain, x64
 * 8-byte slots), call `resolve(dll, func, ordinal, is_ordinal, &addr)` and write
 * addr into the corresponding IAT slot (FirstThunk) of the mapped image.
 * Returns the number of slots patched, or -1 if any thunk fails to resolve
 * (the image cannot load — ReactOS "dependency not found"). */
int wubu_pe_snap_iat(const void *data, size_t size,
                     const wubu_pe_info_t *info,
                     const wubu_pe_section_t *sects, int nsects,
                     void *image, wubu_pe_resolve_fn resolve);

#endif
