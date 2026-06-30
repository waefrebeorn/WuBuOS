/*
 * vsl_memory.h  --  VSL Memory Management API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_MEMORY_H
#define WUBUOS_VSL_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Forward declare opaque type */
struct VSL_MMAP;
typedef struct VSL_MMAP VSL_MMAP;

/* Map memory in VSL address space.
 * Returns virtual address, or 0 on failure. */
uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset);

/* Unmap memory in VSL address space.
 * Returns 0 on success, -1 on failure. */
int vsl_munmap(uint64_t addr, size_t size);

/* Set VSL heap break (brk).
 * Returns new brk, or -1 on failure. */
int64_t vsl_brk(uint64_t new_brk);

/* Get current brk */
uint64_t vsl_get_brk(void);

/* mmap region accessors */
uint64_t vsl_mmap_get_addr(const VSL_MMAP *mm);
size_t vsl_mmap_get_size(const VSL_MMAP *mm);
int vsl_mmap_get_prot(const VSL_MMAP *mm);
int vsl_mmap_get_flags(const VSL_MMAP *mm);
int vsl_mmap_get_fd(const VSL_MMAP *mm);
uint64_t vsl_mmap_get_offset(const VSL_MMAP *mm);

#endif /* WUBUOS_VSL_MEMORY_H */