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

    /* Load segments at their physical LMAs. */
    const Elf64_Phdr *ph = (const Elf64_Phdr *)((uint8_t *)kbuf + eh->e_phoff);
    UINTN loaded = 0;
    for (UINTN i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_offset + ph[i].p_filesz > elf_size ||
            ph[i].p_paddr + ph[i].p_memsz > 0x400000) {
            out(u"[wubufw-loader] FAIL: segment bounds\r\n");
            return EFI_LOAD_ERROR;
        }
        l_memcpy((void *)(uintptr_t)ph[i].p_paddr,
                 (uint8_t *)kbuf + ph[i].p_offset, ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz)
            l_memset((void *)(uintptr_t)(ph[i].p_paddr + ph[i].p_filesz),
                     0, ph[i].p_memsz - ph[i].p_filesz);   /* BSS */
        loaded++;
    }
    if (loaded == 0) {
        out(u"[wubufw-loader] FAIL: no PT_LOAD\r\n");
        return EFI_LOAD_ERROR;
    }
    out(u"[wubufw-loader] loaded "); outdec(loaded);
    out(u" PT_LOAD at 0x100000 (BSS zeroed)\r\n");

    /* 4. Measure the kernel: SHA-256 of the ELF as loaded. */
    uint8_t kdigest[32];
    sha256(kbuf, elf_size, kdigest);
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

    wubufw_handoff(entry_phys);   /* never returns */

    for (;;) __asm__ volatile("cli; hlt");
    return EFI_SUCCESS;
}
