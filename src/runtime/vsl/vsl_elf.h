/*
 * vsl_elf.h  --  VSL ELF Loading API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_ELF_H
#define WUBUOS_VSL_ELF_H

#include <stdint.h>
#include <stddef.h>

/* Parse and validate an ELF64 binary.
 * Returns 0 on success, fills out_entry with entry point. */
int vsl_elf_validate(const void *elf_data, size_t elf_size,
                     uint64_t *out_entry);

/* Load an ELF64 binary into VSL address space.
 * Returns entry point, or 0 on failure. */
uint64_t vsl_elf_load(const void *elf_data, size_t elf_size);

/* Get ELF interpreter (dynamic linker) path.
 * Returns 0 on success, fills buf with path.
 * Returns -1 if statically linked (no interpreter). */
int vsl_elf_interpreter(const void *elf_data, size_t elf_size,
                        char *buf, size_t buf_size);

#endif /* WUBUOS_VSL_ELF_H */