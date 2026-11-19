/*
 * loader.c -- WuBuOS WuBuFW chainloader (the REAL kernel as measured payload).
 *
 * This is the bridge between ring 1 (WuBuFW firmware root of trust) and
 * ring 2 (the bare-metal AGI kernel). It runs as a normal PE32+ EFI
 * application under WuBuFW -- itself verified by fw_agi_attest_and_boot
 * (AuthentiCode + PCR4 extend) -- and then:
 *
 *   1. locates the WUBU_AGI_ATTEST configuration table the firmware
 *      published (PCR0-7 + Secure Boot state + boot counter),
 *   2. reads \EFI\BOOT\KERNEL.ELF off the ESP via the EFI file protocol,
 *   3. parses the ELF64 program headers and loads every PT_LOAD at its
 *      physical LMA (the kernel is linked higher-half, loaded at 1MB),
 *   4. computes SHA-256 of the kernel ELF and stashes a "loader handoff"
 *      block in low memory (0x90040 ptr -> 0x91000 handoff -> 0x92000
 *      attestation) for the kernel to consume,
 *   5. calls ExitBootServices, tears down to 32-bit protected mode, and
 *      jumps to the kernel's physical entry (crt0.S _start at 0x100020).
 *
 * So the measurement chain is: firmware measures loader (PCR4) -> loader
 * measures kernel (SHA-256 in the handoff) -> kernel records both in its
 * immutable AGI trace (PCR4 + kernel digest spans). No third-party code.
 *
 * C11, freestanding, EFI Microsoft x64 ABI (EFIAPI / ms_abi).
 */
#include "../efi.h"
#include "../fw_agi_attest.h"
#include "sha256.h"
#include <stdint.h>
#include <stddef.h>

/* ---- self-contained mem helpers (freestanding) --------------------- */
static void *l_memcpy(void *d, const void *s, UINTN n)
{
    uint8_t *dd = (uint8_t *)d; const uint8_t *ss = (const uint8_t *)s;
    while (n--) *dd++ = *ss++;
    return d;
}
static void l_memset(void *d, int v, UINTN n)
{
    uint8_t *dd = (uint8_t *)d;
    while (n--) *dd++ = (uint8_t)v;
}

/* The loader links freestanding (-nostdlib); the SHA-256 module calls
 * memcpy/memset, so export real definitions. */
void *memcpy(void *d, const void *s, size_t n) { return l_memcpy(d, s, (UINTN)n); }
void *memset(void *d, int v, size_t n)         { l_memset(d, v, (UINTN)n); return d; }

/* ---- console -------------------------------------------------------- */
static EFI_SYSTEM_TABLE  *ST;
static EFI_BOOT_SERVICES *BS;

static void out(const CHAR16 *s) { ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)s); }
static void outc(CHAR16 c) { CHAR16 b[2]; b[0] = c; b[1] = 0; out(b); }
static void outhex(UINT64 v)
{
    CHAR16 buf[19]; const CHAR16 *d = u"0123456789ABCDEF";
    int i = 18; buf[18] = 0;
    if (!v) { out(u"0"); return; }
    while (v && i > 0) { buf[--i] = d[v & 0xF]; v >>= 4; }
    out(&buf[i]);
}
static void outdec(UINT64 v)
{
    CHAR16 buf[21]; int i = 20; buf[20] = 0;
    if (!v) { out(u"0"); return; }
    while (v && i > 0) { buf[--i] = (CHAR16)(u'0' + (v % 10)); v /= 10; }
    out(&buf[i]);
}
static void outhex8(const uint8_t *b, UINTN n)
{
    const CHAR16 *d = u"0123456789ABCDEF";
    for (UINTN i = 0; i < n; i++) {
        outc(d[b[i] >> 4]); outc(d[b[i] & 15]);
    }
}

/* ---- GUIDs (the loader links standalone; efi.h only extern-declares
 * the well-known ones, so we carry our own copy under a local name) ---- */
static EFI_GUID loader_sfs_guid =
    { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

/* ---- ELF64 (kernel.elf) -------------------------------------------- */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

#define PT_LOAD   1
#define ELFCLASS64 2
#define EM_X86_64 62
#define KERNEL_PHYS_BASE   0x100000ULL
#define KERNEL_HIGH_BASE   0xffffffff80000000ULL
#define KERNEL_ENTRY       0x100020ULL   /* _start after the 32-byte mb1 hdr */

/* ---- 64 -> 32 protected-mode handoff (teardown.S) ------------------- */
extern void wubufw_handoff(uint64_t kernel_entry_phys);

/* ---- GOP -> Limine framebuffer handoff (L4) --------------------------
 * Limine publishes the firmware GOP framebuffer to the kernel via the
 * framebuffer request's .response pointer. WuBuFW boots the kernel through
 * THIS loader instead of Limine, so the loader must do the same job: read
 * the live GOP mode (boot services still up) and publish a Limine-layout
 * descriptor in the reserved low-memory stash window (0x90000..0x99000,
 * marked used by the kernel vmm). After the kernel image is copied to its
 * LMAs, patch every framebuffer request in the image to point at it --
 * exactly the scan Limine itself performs. The kernel then owns a real
 * GOP framebuffer even on machines with no Bochs DISPI at all. */
#define WUBU_GOP_FB_ADDR 0x96000ULL   /* free slot: handoff 0x91000,
                                       * attest 0x92000, memmap 0x98000 */

/* Mirrors the kernel's struct limine_framebuffer (metal_main.c) byte for
 * byte. Keep in sync. */
typedef struct {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size, red_mask_shift;
    uint8_t  green_mask_size, green_mask_shift;
    uint8_t  blue_mask_size, blue_mask_shift;
    uint8_t  reserved;
} limine_fb_desc_t;

static int g_gop_fb_ready;

static void capture_gop_fb(void)
{
    static const EFI_GUID gop_guid =
        { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } };
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    if (BS->LocateProtocol((EFI_GUID *)&gop_guid, NULL, (VOID **)&gop)
            != EFI_SUCCESS || !gop || !gop->Mode || !gop->Mode->Info) {
        out(u"[wubufw-loader] GOP absent (no framebuffer handoff)\r\n");
        return;
    }
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *m = gop->Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = m->Info;
    if (!m->FrameBufferBase || !mi->HorizontalResolution ||
        !mi->VerticalResolution || !mi->PixelsPerScanLine) {
        out(u"[wubufw-loader] GOP mode degenerate (no framebuffer handoff)\r\n");
        return;
    }
    limine_fb_desc_t *d = (limine_fb_desc_t *)(uintptr_t)WUBU_GOP_FB_ADDR;
    l_memset(d, 0, sizeof(*d));
    d->address      = m->FrameBufferBase;
    d->width        = mi->HorizontalResolution;
    d->height       = mi->VerticalResolution;
    d->pitch        = (uint64_t)mi->PixelsPerScanLine * 4;
    d->bpp          = 32;
    d->memory_model = 0;   /* RGB */
    if (mi->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        d->red_mask_size = 8;   d->red_mask_shift = 16;
        d->green_mask_size = 8; d->green_mask_shift = 8;
        d->blue_mask_size = 8;  d->blue_mask_shift = 0;
    } else if (mi->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        d->red_mask_size = 8;   d->red_mask_shift = 0;
        d->green_mask_size = 8; d->green_mask_shift = 8;
        d->blue_mask_size = 8;  d->blue_mask_shift = 16;
    } else {
        /* BitMask / BltOnly: no honest linear XRGB surface to publish. */
        out(u"[wubufw-loader] GOP pixel format unsupported (no handoff)\r\n");
        return;
    }
    g_gop_fb_ready = 1;
    out(u"[wubufw-loader] GOP fb handoff: ");
    outdec(d->width); out(u"x"); outdec(d->height);
    out(u" pitch="); outdec(d->pitch);
    out(u" phys="); outhex(d->address); out(u"\r\n");
}

/* Scan the LOADED kernel image (post-copy, post-EBS) for the Limine
 * framebuffer request ID and point each request's .response at the GOP
 * descriptor. Two request layouts exist in this kernel:
 *   - crt0.S compact:  {id0, id1, response}          -> response at +16
 *   - metal_main.c:    {id[4], revision, response}   -> response at +40
 * Discriminator: the quad at +24 is the NEXT request's id0 in the compact
 * layout (nonzero) and id[3]==0 in the full layout. */
static void patch_limine_fb_requests(const Elf64_Phdr *ph, UINTN nph)
{
    if (!g_gop_fb_ready) return;
    const uint64_t id0 = 0xc7b1dd30df4c8b88ULL;
    const uint64_t id1 = 0x6e1b0a4b5b8d7c0aULL;
    UINTN n = 0;
    for (UINTN i = 0; i < nph; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint8_t *base = (uint8_t *)(uintptr_t)ph[i].p_paddr;
        UINTN len = ph[i].p_filesz;
        /* 4-byte stride: crt0.S packs its request block without .align 8,
         * so the ID can sit at a 4-aligned offset (measured: .text+0xdc). */
        for (UINTN off = 0; off + 48 <= len; off += 4) {
            const uint64_t *q = (const uint64_t *)(base + off);
            if (q[0] != id0 || q[1] != id1) continue;
            UINTN roff = (q[3] == 0) ? 40 : 16;
            if (off + roff + 8 > len) continue;
            *(volatile uint64_t *)(base + off + roff) = WUBU_GOP_FB_ADDR;
            n++;
        }
    }
    out(u"[wubufw-loader] limine fb response patched (");
    outdec(n); out(u" requests -> 0x96000)\r\n");
}

/* ---- attestation config table lookup -------------------------------- */
static fw_agi_attest_t *find_attest(void)
{
    static const EFI_GUID g = WUBU_AGI_ATTEST_GUID;
    if (!ST) return NULL;
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        const EFI_GUID *vg = &ST->ConfigurationTable[i].VendorGuid;
        if (vg->Data1 == g.Data1 && vg->Data2 == g.Data2 &&
            vg->Data3 == g.Data3 &&
            vg->Data4[0] == g.Data4[0] && vg->Data4[1] == g.Data4[1] &&
            vg->Data4[2] == g.Data4[2] && vg->Data4[3] == g.Data4[3] &&
            vg->Data4[4] == g.Data4[4] && vg->Data4[5] == g.Data4[5] &&
            vg->Data4[6] == g.Data4[6] && vg->Data4[7] == g.Data4[7])
            return (fw_agi_attest_t *)ST->ConfigurationTable[i].VendorTable;
    }
    return NULL;
}

/* ---- ESP file read --------------------------------------------------- */
static EFI_FILE_PROTOCOL *open_esp_root(void)
{
    UINTN count = 0;
    EFI_HANDLE *handles = NULL;
    if (BS->LocateHandleBuffer(ByProtocol, &loader_sfs_guid,
                               NULL, &count, &handles) != EFI_SUCCESS || count == 0)
        return NULL;
    for (UINTN i = 0; i < count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
        if (BS->HandleProtocol(handles[i], &loader_sfs_guid,
                               (VOID **)&sfs) != EFI_SUCCESS || !sfs)
            continue;
        EFI_FILE_PROTOCOL *root = NULL;
        if (sfs->OpenVolume(sfs, &root) == EFI_SUCCESS && root)
            return root;
    }
    return NULL;
}

/* Read `path` (UTF-16) into `buf` (capacity `cap`). Returns bytes read,
 * or 0 on failure. */
static UINTN read_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                       void *buf, UINTN cap)
{
    EFI_FILE_PROTOCOL *f = NULL;
    if (root->Open(root, &f, (CHAR16 *)path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS || !f)
        return 0;
    UINTN total = 0;
    uint8_t *p = (uint8_t *)buf;
    for (;;) {
        UINTN chunk = cap - total;
        if (chunk > 0x100000) chunk = 0x100000;   /* 1MB chunks */
        if (chunk == 0) break;
        UINTN got = chunk;
        if (f->Read(f, &got, p + total) != EFI_SUCCESS) { f->Close(f); return 0; }
        if (got == 0) break;                       /* EOF */
        total += got;
    }
    f->Close(f);
    return total;
}

/* ---- main ------------------------------------------------------------ */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    ST = st;
    BS = st->BootServices;

    out(u"\r\n[wubufw-loader] WuBuOS chainloader v1\r\n");

    /* 0. Publish the firmware GOP framebuffer for the kernel (L4). Must
     *    run while boot services are still up -- GOP dies at ExitBootServices. */
    capture_gop_fb();

    /* 1. Locate the firmware's attestation snapshot. */
    fw_agi_attest_t *att = find_attest();
    out(u"[wubufw-loader] attestation table: ");
    if (att && att->magic == WUBU_AGI_MAGIC && att->version == WUBU_AGI_ATTEST_VERSION) {
        out(u"FOUND (boot="); outdec(att->boot_counter);
        out(u" sb="); outdec(att->sb_enabled);
        out(u" pcr4="); outhex8(att->pcr[4], 32); out(u")\r\n");
    } else {
        att = NULL;
        out(u"ABSENT (kernel will refuse self-improvement)\r\n");
    }

    /* 2. Open the ESP and read the kernel ELF. */
    EFI_FILE_PROTOCOL *root = open_esp_root();
    if (!root) {
        out(u"[wubufw-loader] FAIL: no ESP volume\r\n");
        return EFI_DEVICE_ERROR;
    }
    out(u"[wubufw-loader] reading \\EFI\\BOOT\\KERNEL.ELF\r\n");

    void *kbuf = NULL;
    EFI_PHYSICAL_ADDRESS kbuf_pa = 0;
    if (BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1024, &kbuf_pa) != EFI_SUCCESS) {
        out(u"[wubufw-loader] FAIL: AllocatePages\r\n");
        return EFI_OUT_OF_RESOURCES;
    }
    kbuf = (void *)(uintptr_t)kbuf_pa;

    UINTN elf_size = read_file(root, u"\\EFI\\BOOT\\KERNEL.ELF", kbuf, 4 * 1024 * 1024);
    if (elf_size < 0x100 || elf_size > 4 * 1024 * 1024) {
        out(u"[wubufw-loader] FAIL: kernel.elf unreadable (");
        outdec(elf_size); out(u" bytes)\r\n");
        return EFI_LOAD_ERROR;
    }

    /* 3. Validate ELF64 and walk PT_LOAD segments. */
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)kbuf;
    if (!(eh->e_ident[0] == 0x7F && eh->e_ident[1] == 'E' && eh->e_ident[2] == 'L' &&
          eh->e_ident[3] == 'F') || eh->e_ident[4] != ELFCLASS64 ||
        eh->e_machine != EM_X86_64 || eh->e_phentsize < sizeof(Elf64_Phdr) ||
        eh->e_phnum == 0) {
        out(u"[wubufw-loader] FAIL: not an x86-64 ELF64\r\n");
        return EFI_LOAD_ERROR;
    }

    out(u"[wubufw-loader] ELF64 ok: phnum="); outdec(eh->e_phnum);
    out(u" entry="); outhex(eh->e_entry); out(u"\r\n");

    uint64_t entry_phys = (eh->e_entry >= KERNEL_HIGH_BASE)
                              ? (eh->e_entry - KERNEL_HIGH_BASE)
                              : (eh->e_entry & 0xFFFFFFFFULL);
    if (entry_phys < 0x100000 || entry_phys > 0x200000) {
        out(u"[wubufw-loader] FAIL: entry out of range (");
        outhex(entry_phys); out(u")\r\n");
        return EFI_LOAD_ERROR;
    }

    /* Validate all PT_LOAD segments now (read-only) -- but DEFER the actual
     * copy+BSS-zero until AFTER ExitBootServices.  The kernel's load window
     * (0x100000+) currently holds WuBuFW's LIVE page tables/heap; writing the
     * kernel over it while the firmware is still running (pre-EBS) wipes that
     * state and triple-faults the boot.  Once boot services are gone the RAM
     * is OS-owned and safe to clobber. */
    const Elf64_Phdr *ph = (const Elf64_Phdr *)((uint8_t *)kbuf + eh->e_phoff);
    UINTN n_pload = 0;
    for (UINTN i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        n_pload++;
        if (ph[i].p_offset + ph[i].p_filesz > elf_size ||
            ph[i].p_paddr + ph[i].p_memsz > 0x02000000) {
            out(u"[wubufw-loader] FAIL: segment bounds\r\n");
            return EFI_LOAD_ERROR;
        }
        out(u"[loader] seg ok: paddr="); outhex(ph[i].p_paddr);
        out(u" filesz="); outhex(ph[i].p_filesz);
        out(u" memsz="); outhex(ph[i].p_memsz); out(u"\r\n");
    }
    if (n_pload == 0) {
        out(u"[wubufw-loader] FAIL: no PT_LOAD\r\n");
        return EFI_LOAD_ERROR;
    }

    /* 4. Measure the kernel: SHA-256 of the ELF as loaded. Chunked with
     * progress markers — on slow TCG hosts this is the longest silent
     * stretch of the chain; the markers discriminate slow-vs-hung and
     * show the rate (devil's-advocate CI diagnosis 2026-11-19). */
    uint8_t kdigest[32];
    out(u"[wubufw-loader] measuring kernel ("); outdec(elf_size);
    out(u" bytes)\r\n");
    {
        sha256_ctx sc;
        sha256_init(&sc);
        const uint64_t CHUNK = 256 * 1024;
        for (uint64_t off = 0; off < elf_size; off += CHUNK) {
            uint64_t n = elf_size - off;
            if (n > CHUNK) n = CHUNK;
            sha256_update(&sc, (const uint8_t *)kbuf + off, n);
            out(u"[wubufw-loader] sha256: "); outdec(off + n);
            out(u"/"); outdec(elf_size); out(u"\r\n");
        }
        sha256_final(&sc, kdigest);
    }
    out(u"[wubufw-loader] kernel sha256="); outhex8(kdigest, 32); out(u"\r\n");

    /* 5. Stash the loader handoff for the kernel (low physical RAM,
     *    identity-mapped by crt0's 0..1GB PD). */
    wubu_loader_handoff_t *hd = (wubu_loader_handoff_t *)(uintptr_t)WUBU_HANDOFF_ADDR;
    l_memset(hd, 0, sizeof(*hd));
    hd->magic       = WUBU_LOADER_HANDOFF_MAGIC;
    hd->version     = 1;
    hd->kernel_size = (uint32_t)elf_size;
    l_memcpy(hd->kernel_sha256, kdigest, 32);
    hd->attest_addr = att ? WUBU_ATTEST_ADDR : 0;
    if (att) {
        fw_agi_attest_t *dst = (fw_agi_attest_t *)(uintptr_t)WUBU_ATTEST_ADDR;
        l_memcpy(dst, att, sizeof(*dst));
    }
    *(volatile uint64_t *)(uintptr_t)WUBU_HANDOFF_PTR_ADDR = WUBU_HANDOFF_ADDR;
    out(u"[wubufw-loader] handoff @ 0x91000 (attest=");
    outhex(hd->attest_addr); out(u")\r\n");

    /* 5b. Collect the FIRMWARE memory map into 0x98000 (kernel E820-style,
     *     gap I1): u16 count, then {u64 base, u64 len, u32 type=1} entries
     *     for every EfiConventionalMemory range.  The kernel's vmm then
     *     owns the REAL RAM size instead of a hardcoded 1 GB. */
    {
        UINTN msz2 = 0, mkey2 = 0, dsz2 = 0;
        UINT32 dver2 = 0;
        BS->GetMemoryMap(&msz2, NULL, &mkey2, &dsz2, &dver2);   /* size */
        if (msz2 > 0) {
            uint8_t *buf = NULL;
            if (BS->AllocatePool(EfiLoaderData, msz2 + dsz2,
                                 (void **)&buf) == EFI_SUCCESS) {
                if (BS->GetMemoryMap(&msz2, (EFI_MEMORY_DESCRIPTOR *)buf,
                                     &mkey2, &dsz2, &dver2) == EFI_SUCCESS) {
                    uint16_t n = 0;
                    uint8_t *outp = (uint8_t *)(uintptr_t)0x98000;
                    l_memset(outp, 0, 2 + 8 * 20);
                    for (UINTN i = 0; i < msz2 / dsz2 && n < 8; i++) {
                        EFI_MEMORY_DESCRIPTOR *d =
                            (EFI_MEMORY_DESCRIPTOR *)(buf + i * dsz2);
                        if (d->Type == EfiConventionalMemory) {
                            uint64_t base = d->PhysicalStart;
                            uint64_t len  = d->NumberOfPages * 4096;
                            *(volatile uint64_t *)(outp + 2 + (UINTN)n * 20 + 0) = base;
                            *(volatile uint64_t *)(outp + 2 + (UINTN)n * 20 + 8) = len;
                            *(volatile uint32_t *)(outp + 2 + (UINTN)n * 20 + 16) = 1;
                            n++;
                        }
                    }
                    *(volatile uint16_t *)outp = n;
                    out(u"[wubufw-loader] memmap @ 0x98000 (");
                    outhex(n); out(u" regions)\r\n");
                }
                BS->FreePool(buf);
            }
        }
    }

    /* 6. Exit boot services, then hand off. */
    UINTN msz = 0, mapkey = 0, dsz = 0;
    UINT32 dver = 0;
    BS->GetMemoryMap(&msz, NULL, &mapkey, &dsz, &dver);   /* key is set */
    EFI_STATUS r = BS->ExitBootServices(image, mapkey);
    if (r != EFI_SUCCESS) {
        /* Memory map changed since the key was taken: retry once. */
        BS->GetMemoryMap(&msz, NULL, &mapkey, &dsz, &dver);
        r = BS->ExitBootServices(image, mapkey);
    }
    out(u"[wubufw-loader] boot services exited (");
    outhex(r); out(u") -- handing control to kernel @ ");
    outhex(entry_phys); out(u"\r\n");

    /* Now that boot services are gone, copy the validated PT_LOAD file
     * contents to their physical LMAs.  We do NOT zero .bss here: the
     * kernel's BSS window (0x168080+) is where WuBuFW's ACTIVE page tables
     * live while this loader is still running on them, so zeroing it here
     * corrupts our own execution.  crt0.S zeroes the kernel's .bss itself
     * right after it installs its own page tables (self-sufficient under
     * any loader). */
    for (UINTN i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        l_memcpy((void *)(uintptr_t)ph[i].p_paddr,
                 (uint8_t *)kbuf + ph[i].p_offset, ph[i].p_filesz);
    }
    out(u"[wubufw-loader] copied "); outdec(n_pload);
    out(u" PT_LOAD (.bss zeroed by kernel crt0, post-EBS)\r\n");

    /* The kernel image is now at its LMAs: point its Limine framebuffer
     * requests at the GOP descriptor captured before ExitBootServices. */
    patch_limine_fb_requests(ph, eh->e_phnum);

    wubufw_handoff(entry_phys);   /* never returns */

    for (;;) __asm__ volatile("cli; hlt");
    return EFI_SUCCESS;
}
