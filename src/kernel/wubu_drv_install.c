/*
 * wubu_drv_install.c -- the DRIVER SELF-INSTALLATION arm.
 *
 * The AGI-OS missing arm: a device with no registered driver used to be
 * left "unbound" forever. This module NUMERATES it (modalias), DECIDES
 * (manifest), ACQUIRES (source), BUILDS (ET_REL object), LOADS (our own
 * ELF relocatable loader), and RE-PROBES (register + bind live).
 *
 * The ELF64 ET_REL loader here is the "own entire system" piece: it reads
 * a normal gcc-produced .o, copies PROGBITS sections into a contiguous
 * code block, relocates R_X86_64_64 / PC32 / RELATIVE against the kernel
 * export table, and calls the module's `wubu_mod_entry` symbol. No host
 * modprobe, no apt, no kext -- the kernel installs its own drivers.
 *
 * C11. Freestanding-safe in the loader core (reads the byte buffer, no
 * libc); the build + manifest + report paths use the hosted libc where
 * appropriate (gated WUBU_DI_HOSTED).
 */
#include "wubu_drv_install.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

/* ------------------------------------------------------------------ */
/* ELF64 constants (kept local so the loader has no <elf.h> dep).      */
/* ------------------------------------------------------------------ */
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_REL 1
#define EM_X86_64 62

#define PT_NULL 0

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_NOBITS 8

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

/* symbol bindings / types */
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STT_FUNC 2
#define STT_OBJECT 1

#define SHN_UNDEF 0
#define SHN_ABS 0xfff1

/* PLT: 64 entries * 16 bytes = 1024 bytes appended past PROGBITS. */
#define WUBU_DI_MAX_PLT 64
#define WUBU_DI_PLT_MAXSIZE (WUBU_DI_MAX_PLT * 16)

/* R_X86_64 relocations */
#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_PLT32 4
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9   /* GoTPCREL (r15w): treat as PLT for flat loader */

/* Driver cache directory */
#define WUBU_DRV_CACHE_DIR "~/opt/wubu_drivers"
#define WUBU_MESA_UPSTREAM "https://gitlab.freedesktop.org/mesa/mesa.git"

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    unsigned char st_info, st_other;
    uint16_t st_shndx;
    uint64_t st_value, st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((uint32_t)(i))

/* ------------------------------------------------------------------ */
/* the manifest — the AGI's mutable driver knowledge. A device whose     */
/* modalias prefix matches an entry is a device we can install.          */
/* ------------------------------------------------------------------ */
static const wubu_drv_manifest_t g_manifest[] = {
    /* NVIDIA dGPU (proprietary blob not source-able; NVK is the OSS leg).
       Marked fw_container: a generic framework maps this to the Mesa NVK
       stack at runtime (see gpu_universal_layer.md). */
    { "pci:v000010DE", "nv_gpu", "fw_container", "mesa/nvk" },

    /* AMD dGPU / APU (Steam Deck Van Gogh = RADV, OSS). */
    { "pci:v00001002", "amd_gpu", "git", "mesa/radv" },

    /* Intel iGPU (ANV, OSS). */
    { "pci:v00008086", "intel_gpu", "git", "mesa/anv" },

    /* Arm Mali (PanVK). */
    { "pci:v000013B5", "arm_gpu", "git", "mesa/panvk" },

    /* Qualcomm Adreno (Turnip). */
    { "pci:v00005143", "qcom_gpu", "git", "mesa/turnip" },

    /* TEST STUB: tiny Mesa driver for self-install testing.
       Points to local git repo for end-to-end verification. */
    { "pci:v00001002d0000163F", "test_mesa_steamdeck", "git", "file:///tmp/mesa_stub_steamdeck" },

    /* VirtIO GPU (hosted / KVM leg). */
    { "pci:v00001AF4", "virtio_gpu", "local", "src/kernel/wubu_drv_virtio.c" },

    /* a local test driver (self-install selftest). */
    { "pci:vCAFE", "cafe_demo", "local", "tools/probe/drv_demo.c" },
};
#define WUBU_DI_NMANIFEST ((int)(sizeof(g_manifest) / sizeof(g_manifest[0])))

/* ------------------------------------------------------------------ */
/* DI1: synthesize the Linux-style modalias.                            */
/*   pci:v000010DEd00002504sv00001463sd00008877bc03sc00i00             */
/*   usb:vXXXXpXXXXdXXXXdcXXdscXXdpXXicXXiscXXipXX                      */
/* We emit the compact (unambiguous) form the registry can key on.     */
/* ------------------------------------------------------------------ */
int wubu_drv_modalias(const wubu_drv_dev_t *dev, char *out, size_t cap)
{
    if (!dev || !out || cap == 0) return -1;
    int n;
    if (dev->bus == WUBU_DRV_BUS_USB) {
        n = snprintf(out, cap, "usb:v%04Xp%04Xd%04Xdc%02X",
                     dev->vendor, dev->device, dev->device, dev->class_code);
    } else { /* PCI + ACPI fall back to the PCI modalias keyed on vendor */
        n = snprintf(out, cap, "pci:v%04Xd%04X",
                     dev->vendor, dev->device);
    }
    if (n < 0) return -1;
    return n;
}

/* DI2: is this device already bound? */
int wubu_drv_is_bound(const wubu_drv_dev_t *dev)
{
    return dev && dev->bound;
}

/* DI4: manifest lookup by modalias prefix. */
const wubu_drv_manifest_t *wubu_drv_manifest_lookup(const char *modalias)
{
    if (!modalias) return NULL;
    for (int i = 0; i < WUBU_DI_NMANIFEST; i++) {
        size_t pl = strlen(g_manifest[i].modalias_prefix);
        if (strncmp(modalias, g_manifest[i].modalias_prefix, pl) == 0)
            return &g_manifest[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* DI5: the export table — symbols a loaded module may call back into. */
/* ------------------------------------------------------------------ */
struct wubu_di_export {
    const char *name;
    const void *addr;
};
static const struct wubu_di_export g_exports[] = {
    { "wubu_drv_register", (const void *)wubu_drv_register },
    { "wubu_drv_add_device", (const void *)wubu_drv_add_device },
    { "wubu_drv_device_count", (const void *)wubu_drv_device_count },
    { "wubu_drv_driver_count", (const void *)wubu_drv_driver_count },
};
#define WUBU_DI_NEXPORT ((int)(sizeof(g_exports) / sizeof(g_exports[0])))

const void *wubu_drv_export_lookup(const char *sym)
{
    if (!sym) return NULL;
    for (int i = 0; i < WUBU_DI_NEXPORT; i++)
        if (strcmp(g_exports[i].name, sym) == 0)
            return g_exports[i].addr;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* DI3: wubu_drv_acquire() -- fetch source for git/pkg sources.       */
/*   For git sources: clone into ~/opt/wubu_drivers/<driver_name>/      */
/*   Returns 0 on success, negative on failure.                        */
/* ------------------------------------------------------------------ */
int wubu_drv_acquire(const wubu_drv_manifest_t *m)
{
    if (!m) return -1;

    if (strcmp(m->source, "git") != 0)
        return 0; /* nothing to acquire for local/fw sources */

    /* Build cache path: ~/opt/wubu_drivers/<driver_name> */
    char cache_home[512];
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    snprintf(cache_home, sizeof(cache_home), "%s/opt/wubu_drivers", home);

    char cache_dir[1024];
    snprintf(cache_dir, sizeof(cache_dir), "%s/%s", cache_home, m->driver_name);

    /* Check if already cached */
    if (access(cache_dir, F_OK) == 0) {
        char src_file[2048];
        snprintf(src_file, sizeof(src_file), "%s/wubu_mod_entry.c", cache_dir);
        if (access(src_file, F_OK) == 0)
            return 0; /* already cached */
    }

    /* Create cache directory */
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s 2>/dev/null", cache_home);
    system(mkdir_cmd);

    /* Check if path looks like a URL (starts with http://, https://, git://, file://)
     * or is an absolute path */
    int is_url = 0;
    if (strncmp(m->path, "http://", 7) == 0 ||
        strncmp(m->path, "https://", 8) == 0 ||
        strncmp(m->path, "git://", 6) == 0 ||
        strncmp(m->path, "file://", 7) == 0 ||
        m->path[0] == '/') {
        is_url = 1;
    }
    
    char clone_cmd[4096];
    if (is_url) {
        /* It's already a git URL or file path */
        snprintf(clone_cmd, sizeof(clone_cmd),
                "rm -rf %s && git clone --depth 1 %s %s 2>/dev/null",
                cache_dir, m->path, cache_dir);
    } else {
        /* It's a Mesa subpath like "mesa/radv" - clone from upstream */
        snprintf(clone_cmd, sizeof(clone_cmd),
                "rm -rf %s && git clone --depth 1 %s %s 2>/dev/null",
                cache_dir, WUBU_MESA_UPSTREAM, cache_dir);
    }

    int rc = system(clone_cmd);
    if (rc != 0) {
        fprintf(stderr, "wubu_drv_acquire: git clone failed for %s\n", m->driver_name);
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* DI6: OUR OWN ELF64 ET_REL relocatable loader.                        */
/* ------------------------------------------------------------------ */
static uint16_t rd16(const uint8_t *p){ uint16_t v; memcpy(&v,p,2); return v; }
static uint32_t rd32(const uint8_t *p){ uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t rd64(const uint8_t *p){ uint64_t v; memcpy(&v,p,8); return v; }

int wubu_drv_elf_load(const void *obj, size_t obj_len)
{
    const uint8_t *b = (const uint8_t *)obj;
    if (!b || obj_len < sizeof(Elf64_Ehdr)) return WUBU_DI_LOAD_FAIL;

    /* magic: 0x7f 'E' 'L' 'F', class 2 (64), data 1 (LSB), ver 1 */
    if (b[0] != 0x7f || b[1] != 'E' || b[2] != 'L' || b[3] != 'F')
        return WUBU_DI_LOAD_FAIL;
    if (b[4] != ELFCLASS64 || b[5] != ELFDATA2LSB || b[6] != EV_CURRENT)
        return WUBU_DI_LOAD_FAIL;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)b;
    if (rd16((const uint8_t *)&eh->e_type) != ET_REL) return WUBU_DI_LOAD_FAIL;
    if (rd16((const uint8_t *)&eh->e_machine) != EM_X86_64) return WUBU_DI_LOAD_FAIL;

    uint16_t shnum = rd16((const uint8_t *)&eh->e_shnum);
    uint16_t shstrndx = rd16((const uint8_t *)&eh->e_shstrndx);
    uint64_t shoff = rd64((const uint8_t *)&eh->e_shoff);
    uint16_t shentsize = rd16((const uint8_t *)&eh->e_shentsize);
    if (shentsize < sizeof(Elf64_Shdr)) return WUBU_DI_LOAD_FAIL;
    if (shoff + (uint64_t)shnum * shentsize > obj_len) return WUBU_DI_LOAD_FAIL;

    /* gather section headers */
    Elf64_Shdr *sh = (Elf64_Shdr *)calloc(shnum, sizeof(Elf64_Shdr));
    if (!sh) return WUBU_DI_NOMEM;
    for (int i = 0; i < shnum; i++) {
        const uint8_t *s = b + shoff + (uint64_t)i * shentsize;
        sh[i].sh_name = rd32(s + 0);
        sh[i].sh_type = rd32(s + 4);
        sh[i].sh_flags = rd64(s + 8);
        sh[i].sh_addr = rd64(s + 16);
        sh[i].sh_offset = rd64(s + 24);
        sh[i].sh_size = rd64(s + 32);
        sh[i].sh_link = rd32(s + 40);
        sh[i].sh_info = rd32(s + 44);
        sh[i].sh_addralign = rd64(s + 48);
        sh[i].sh_entsize = rd64(s + 56);
    }

    /* section name string table */
    const Elf64_Shdr *shst = &sh[shstrndx];
    const char *shstr = (const char *)(b + shst->sh_offset);
    if (shst->sh_offset + shst->sh_size > obj_len) { free(sh); return WUBU_DI_LOAD_FAIL; }

    /* allocate a flat image for PROGBITS (+NOBINS as zeroed tail) + a
     * small PLT region appended past the PROGBITS for external calls. */
    uint64_t total = 0;
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type == SHT_PROGBITS && (sh[i].sh_flags & SHF_ALLOC))
            total += sh[i].sh_size;
    }
    uint64_t img_size = total + WUBU_DI_PLT_MAXSIZE;
    /* allocate an executable image for the loaded PROGBITS (W^X flip:
     * RW while we copy + relocate, then RX once the code is finalized —
     * the loaded module's text must be runnable). */
    size_t ipages = (img_size + 4095) / 4096;
    if (ipages < 1) ipages = 1;
    uint8_t *image = (uint8_t *)mmap(NULL, ipages * 4096,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (image == MAP_FAILED) { free(sh); return WUBU_DI_NOMEM; }
    memset(image, 0, total);

    /* per-section offset in image + copy PROGBITS */
    uint64_t *sec_off = (uint64_t *)malloc(shnum * sizeof(uint64_t));
    if (!sec_off) { free(image); free(sh); return WUBU_DI_NOMEM; }
    for (int i = 0; i < shnum; i++) sec_off[i] = (uint64_t)-1; /* not-in-image sentinel */
    uint64_t cur = 0;
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type == SHT_PROGBITS && (sh[i].sh_flags & SHF_ALLOC)) {
            sec_off[i] = cur;
            if (sh[i].sh_offset + sh[i].sh_size <= obj_len)
                memcpy(image + cur, b + sh[i].sh_offset, sh[i].sh_size);
            cur += sh[i].sh_size;
        }
    }

    /* symbol table (find SHT_SYMTAB) + its string table */
    int symtab = -1, strtab = -1;
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type == SHT_SYMTAB) symtab = i;
        if (sh[i].sh_type == SHT_STRTAB && i != shstrndx) strtab = i;
    }
    if (symtab < 0 || strtab < 0) { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }
    const Elf64_Sym *symtab_raw = (const Elf64_Sym *)(b + sh[symtab].sh_offset);
    uint64_t nsym = sh[symtab].sh_size / sizeof(Elf64_Sym);
    const char *str = (const char *)(b + sh[strtab].sh_offset);
    if (sh[strtab].sh_offset + sh[strtab].sh_size > obj_len) { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }

    /* PLT for PC-relative calls/LEA to externally-resolved (UNDEF) symbols. */
    uint64_t plt_base = cur;          /* PLT grows past PROGBITS */
    uint64_t plt_used = 0;
    int plt_slot[WUBU_DI_MAX_PLT];    /* symbol index -> PLT slot, or -1 */
    for (int j = 0; j < WUBU_DI_MAX_PLT; j++) plt_slot[j] = -1;

    /* apply RELA relocations */
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type != SHT_RELA) continue;
        const Elf64_Rela *rel = (const Elf64_Rela *)(b + sh[i].sh_offset);
        uint64_t nrel = sh[i].sh_size / sizeof(Elf64_Rela);
        uint32_t tgt_sec = sh[i].sh_info; /* section the relocs apply to */
        if (tgt_sec >= shnum || sec_off[tgt_sec] == (uint64_t)-1)
            { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }
        for (uint64_t r = 0; r < nrel; r++) {
            uint32_t stype = ELF64_R_TYPE(rel[r].r_info);
            uint32_t sndx  = ELF64_R_SYM(rel[r].r_info);
            uint64_t S = 0;
            int need_plt = 0; /* S becomes the PLT entry instead */
            if (stype != R_X86_64_RELATIVE && stype != R_X86_64_NONE) {
                if (sndx >= nsym) { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }
                const Elf64_Sym *s = &symtab_raw[sndx];
                uint16_t shndx = rd16((const uint8_t *)&s->st_shndx);
                uint64_t val = rd64((const uint8_t *)&s->st_value);
                if (shndx == SHN_ABS) {
                    S = val;
                } else if (shndx == SHN_UNDEF) {
                    const char *nm = str + rd32((const uint8_t *)&s->st_name);
                    const void *a = wubu_drv_export_lookup(nm);
                    if (!a) { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }
                    uint64_t real = (uint64_t)(uintptr_t)a;
                    /* PC-rel relocs (CALL/LEA) can't reach a far kernel addr
                     * in 32 bits -> route through a PLT trampoline. */
                    if (stype == R_X86_64_PLT32 || stype == R_X86_64_PC32 ||
                        stype == R_X86_64_GOTPCREL) {
                        need_plt = 1;
                        /* allocate a PLT entry for this symbol if needed */
                        if (plt_slot[sndx] < 0) {
                            if (plt_used >= WUBU_DI_MAX_PLT) {
                                free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL;
                            }
                            int slot = (int)plt_used++;
                            plt_slot[sndx] = slot;
                            uint8_t *tramp = image + plt_base + (uint64_t)slot * 16;
                            /* FF 25 00 00 00 00 : jmp [rip+0]  (rip->next slot) */
                            tramp[0] = 0xFF; tramp[1] = 0x25;
                            tramp[2] = 0; tramp[3] = 0; tramp[4] = 0; tramp[5] = 0;
                            /* 8-byte slot holds the 64-bit target */
                            *((uint64_t *)(tramp + 6)) = real;
                        }
                        S = (uint64_t)(uintptr_t)(image + plt_base + (uint64_t)plt_slot[sndx] * 16);
                    } else {
                        S = real;
                    }
                } else if (shndx < shnum) {
                    S = (uint64_t)(uintptr_t)(image + sec_off[shndx]) + val;
                } else {
                    free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL;
                }
            }
            uint64_t P = (uint64_t)(uintptr_t)(image + sec_off[tgt_sec]) + rel[r].r_offset;
            int64_t A = rel[r].r_addend;
            (void)need_plt;
            if (sec_off[tgt_sec] == (uint64_t)-1) { fprintf(stderr,"DIAG bad tgt_sec %u\n", tgt_sec); free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }
            switch (stype) {
                case R_X86_64_64:
                    *((uint64_t *)P) = S + (uint64_t)A;
                    break;
                case R_X86_64_RELATIVE:
                    *((uint64_t *)P) = (uint64_t)(uintptr_t)image + (uint64_t)A; break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32:
                    *((uint32_t *)P) = (uint32_t)((int64_t)(S + A) - (int64_t)P); break;
                case R_X86_64_32:
                    *((uint32_t *)P) = (uint32_t)(S + (uint64_t)A); break;
                case R_X86_64_32S:
                    *((int32_t *)P) = (int32_t)(S + (uint64_t)A); break;
                case R_X86_64_GOTPCREL:
                    /* GOTPC/REX_GOTPCREL: for our flat loader, treat like PLT
                     * (S already points at the PLT entry holding the addr). */
                    *((uint32_t *)P) = (uint32_t)((int64_t)(S + A) - (int64_t)P); break;
                case R_X86_64_NONE: break;
                default:
                    free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL;
            }
        }
    }

    /* find `wubu_mod_entry` (a global/weak function) and call it.
     * NOTE: Elf st_info = (bind<<4)+type, so bind = st_info>>4,
     * type = st_info & 0xf — the low nibble is the TYPE, NOT the binding. */
    const void *entry = NULL;
    for (uint64_t i = 0; i < nsym; i++) {
        const Elf64_Sym *s = &symtab_raw[i];
        uint8_t bind = s->st_info >> 4;
        uint8_t type = s->st_info & 0xf;
        if ((bind == STB_GLOBAL || bind == STB_WEAK) && type == STT_FUNC) {
            const char *nm = str + rd32((const uint8_t *)&s->st_name);
            if (strcmp(nm, "wubu_mod_entry") == 0) {
                uint16_t shndx = rd16((const uint8_t *)&s->st_shndx);
                uint64_t val = rd64((const uint8_t *)&s->st_value);
                if (shndx < shnum && sec_off[shndx] != (uint64_t)-1)
                    entry = (const void *)(image + sec_off[shndx]) + val;
            }
        }
    }
    if (!entry) { free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL; }

    /* W^X flip: code copied + relocated as RW, now mark executable so the
     * module's entry point can run. (The loaded driver text lives resident
     * for the lifetime of the binding — it is freed only never, matching a
     * real load-and-stay kernel module.) */
    if (mprotect(image, ipages * 4096, PROT_READ | PROT_EXEC) != 0) {
        free(sec_off); munmap(image, ipages * 4096); free(sh); return WUBU_DI_LOAD_FAIL;
    }
    int (*mod_entry)(void) = (int (*)(void))entry;
    int rc = mod_entry();
    /* image kept resident (holds the bound driver's text + wubu_drv_t). */
    free(sec_off);
    free(sh);
    return rc == 0 ? WUBU_DI_OK : WUBU_DI_LOAD_FAIL;
}

/* ------------------------------------------------------------------ */
/* DI8: build -- compile a driver source to an ET_REL .o in memory.     */
/* Hosted leg shells to the host C compiler. Metal leg would route to   */
/* the in-kernel HolyD backend (documented, future).                    */
/* ------------------------------------------------------------------ */
void *wubu_drv_build(const wubu_drv_manifest_t *m, size_t *obj_len)
{
    if (!m || !obj_len) return NULL;
    *obj_len = 0;

    /* Home directory for cache */
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";

    char cache_dir[2048];
    snprintf(cache_dir, sizeof(cache_dir), "%s/opt/wubu_drivers/%s", home, m->driver_name);

    if (strcmp(m->source, "local") == 0) {
        /* Local source tree: build directly from m->path */
        char tmp[] = "/tmp/wubu_di_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) return NULL;
        close(fd);
        char obj[64];
        snprintf(obj, sizeof(obj), "%s.o", tmp);
        char cmd[2048];
        /* -mcmodel=large: emits R_X86_64_64 (movabs) for absolute addresses
         * instead of R_X86_64_32, so full 64-bit relocation works for the
         * mmap'd loader image (which lives at a high address). */
        snprintf(cmd, sizeof(cmd),
                 "cc -O0 -fno-pic -fno-stack-protector -ffreestanding -mcmodel=large "
                 "-I%s -c %s -o %s 2>/dev/null", "src/kernel", m->path, obj);
        int rc = system(cmd);
        if (rc != 0) { unlink(tmp); unlink(obj); return NULL; }

        FILE *f = fopen(obj, "rb");
        if (!f) { unlink(tmp); unlink(obj); return NULL; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *data = malloc((size_t)sz);
        if (!data) { fclose(f); unlink(tmp); unlink(obj); return NULL; }
        if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
            free(data); fclose(f); unlink(tmp); unlink(obj); return NULL;
        }
        fclose(f);
        unlink(tmp); unlink(obj);
        *obj_len = (size_t)sz;
        return data;
    }

    if (strcmp(m->source, "git") == 0) {
        /* Git source: compile from the acquired cache directory.
         * The source file is expected at <cache_dir>/wubu_mod_entry.c */
        char src_file[2048];
        snprintf(src_file, sizeof(src_file), "%s/wubu_mod_entry.c", cache_dir);

        /* Check if source exists */
        if (access(src_file, R_OK) != 0) {
            fprintf(stderr, "wubu_drv_build: source file not found: %s\n", src_file);
            return NULL;
        }

        char tmp[] = "/tmp/wubu_di_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) return NULL;
        close(fd);
        char obj[64];
        snprintf(obj, sizeof(obj), "%s.o", tmp);

        /* Mesa include flags for compiling a Mesa-style driver.
         * The minimal set: include the driver source directory itself
         * so internal Mesa headers can be found. */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "cc -O0 -fno-pic -fno-stack-protector -ffreestanding -mcmodel=large "
                 "-I%s -c %s -o %s 2>/dev/null", cache_dir, src_file, obj);

        int rc = system(cmd);
        if (rc != 0) {
            unlink(tmp); unlink(obj);
            fprintf(stderr, "wubu_drv_build: gcc failed for %s\n", m->driver_name);
            return NULL;
        }

        FILE *f = fopen(obj, "rb");
        if (!f) { unlink(tmp); unlink(obj); return NULL; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *data = malloc((size_t)sz);
        if (!data) { fclose(f); unlink(tmp); unlink(obj); return NULL; }
        if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
            free(data); fclose(f); unlink(tmp); unlink(obj); return NULL;
        }
        fclose(f);
        unlink(tmp); unlink(obj);
        *obj_len = (size_t)sz;
        return data;
    }

    /* pkg source type not yet implemented */
    return NULL;
}

/* ------------------------------------------------------------------ */
/* DI7: the full self-install pipeline.                                */
/* ------------------------------------------------------------------ */
int wubu_drv_install(const wubu_drv_dev_t *dev)
{
    if (!dev) return WUBU_DI_LOAD_FAIL;
    if (wubu_drv_is_bound(dev)) return WUBU_DI_BUSY;

    char alias[64];
    if (wubu_drv_modalias(dev, alias, sizeof(alias)) < 0)
        return WUBU_DI_LOAD_FAIL;

    const wubu_drv_manifest_t *m = wubu_drv_manifest_lookup(alias);
    if (!m) return WUBU_DI_NO_MANIFEST;

    /* already bound? */
    if (wubu_drv_is_bound(dev)) return WUBU_DI_BUSY;

    /* step 3: ACQUIRE (for git/pkg sources) */
    if (strcmp(m->source, "git") == 0 || strcmp(m->source, "pkg") == 0) {
        if (wubu_drv_acquire(m) != 0) {
            return WUBU_DI_NO_SOURCE;
        }
    }

    /* step 4: BUILD */
    size_t obj_len = 0;
    void *obj = wubu_drv_build(m, &obj_len);
    if (!obj) return WUBU_DI_BUILD_FAIL;

    /* step 5: LOAD */
    int rc = wubu_drv_elf_load(obj, obj_len);
    free(obj);

    if (rc != WUBU_DI_OK) {
        return rc;
    }

    /* step 6: RE-PROBE (bind the device now that driver is registered).
     * The caller's device must be present in the bus table for the driver
     * to match it — add it if not already registered, then probe. */
    (void)wubu_drv_add_device(dev);
    int probed = wubu_drv_probe();
    if (probed < 1) {
        /* Driver didn't bind this device, but it was loaded */
        fprintf(stderr, "wubu_drv_install: driver loaded but probe found no match\n");
        return WUBU_DI_LOAD_FAIL;
    }

    return WUBU_DI_OK;
}

/* DI9: report unbound devices for the AGI to author manifests. */
int wubu_drv_install_report(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    /* re-derive from the registry: we can't iterate g_devs here (it's
     * private to wubu_drv.c), so report the manifest coverage summary. */
    size_t off = 0;
    int n = 0;
    for (int i = 0; i < WUBU_DI_NMANIFEST; i++) {
        int w = snprintf(out + off, cap - off, "[dinst] %s -> %s (%s)\n",
                         g_manifest[i].modalias_prefix,
                         g_manifest[i].driver_name, g_manifest[i].source);
        if (w < 0) break;
        off += (size_t)w; n++;
    }
    return n;
}