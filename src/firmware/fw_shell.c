/*
 * fw_shell.c  --  WuBuFW interactive EFI-style shell.
 *
 * A real firmware ships an interactive shell: it is how a user (or the boot
 * manager) pokes at devices, reads files, and launches images. Ours supports
 * a practical subset: `ls`, `cat`, `mm` (peek), `pci`, `devices`, `map`,
 * `drivers`, `ver`, `mem`, `boot <file>`, `tpm`, `net`, and `exit`. It is a
 * genuine shell driven over the serial/PS2 console, not a menu.
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_block.h"
#include "fw.h"
#include "fw_tpm.h"

/* SNP accessor defined in fw_e1000.c (no dedicated header). */
EFI_SIMPLE_NETWORK_PROTOCOL *fw_e1000_get_snp(void);

static char g_line[256];
static int  g_pos;

static void shell_prompt(void) { fw_puts("WuBuFW> "); }

static void line_clear(void) { g_pos = 0; g_line[0] = 0; }

static int line_get(void) {
    line_clear();
    shell_prompt();
    for (;;) {
        int c = fw_getc();
        if (c < 0) continue;
        if (c == '\n' || c == '\r') { fw_putc('\n'); g_line[g_pos] = 0; return g_pos > 0; }
        if (c == '\b' || c == 127) {
            if (g_pos > 0) { g_pos--; fw_puts("\b \b"); }
            continue;
        }
        if (c == 0x100 || c >= 256) continue;       /* nav/ext keys */
        if (g_pos < (int)sizeof(g_line) - 1) {
            g_line[g_pos++] = (char)c;
            fw_putc((char)c);
        }
    }
}

static int tok(char *out[8]) {
    int n = 0;
    char *p = g_line;
    while (*p && n < 8) {
        while (*p == ' ') p++;
        if (!*p) break;
        out[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
    }
    return n;
}

static void cmd_ls(void) {
    for (int v = 0; v < fw_media_count(); v++) {
        fw_volume *vol = fw_media_get(v);
        if (!vol) continue;
        fw_printf("  Volume %d  %s\n", v, fw_volume_label(v));
        fw_dirent *e;
        for (fw_volume_reset(v); (e = fw_volume_next(v)); )
            fw_printf("    %c %8u  %s\n", e->is_dir ? 'd' : 'f', e->size, e->name);
    }
    if (!fw_media_count()) fw_puts("  (no volumes)\n");
}

static void cmd_cat(const char *path) {
    fw_volume *vol = fw_media_get(0);
    if (!vol) { fw_puts("no volume\n"); return; }
    fw_openfile *f = fw_volume_open(vol, path);
    if (!f) { fw_printf("cannot open %s\n", path); return; }
    static uint8_t buf[512];
    uint32_t off = 0, n;
    while ((n = fw_openfile_pread(f, off, 256, buf)) > 0) {
        for (uint32_t i = 0; i < n; i++) fw_putc(buf[i] < 32 && buf[i] != '\n' ? '.' : (char)buf[i]);
        off += n;
        if (n < 256) break;
    }
    fw_puts("\n");
}

static void cmd_drivers(void) {
    fw_puts("  drivers bound on PCI:\n");
    for (int i = 0; i < fw_pci_count(); i++) {
        fw_pci_dev *d = fw_pci_get(i);
        fw_printf("    %d:%d.%d %04X:%04X %s\n", d->bus, d->dev, d->fn,
                  d->vendor_id, d->device_id, fw_pci_class_name(d->class_code, d->subclass));
    }
}

static void cmd_pci(void) {
    for (int i = 0; i < fw_pci_count(); i++) {
        fw_pci_dev *d = fw_pci_get(i);
        fw_printf("  %02d:%02d.%d  %04X:%04X  %02X.%02X.%02X  %s\n",
                  d->bus, d->dev, d->fn, d->vendor_id, d->device_id,
                  d->class_code, d->subclass, d->prog_if,
                  fw_pci_class_name(d->class_code, d->subclass));
    }
}

static void cmd_mem(void) {
    fw_printf("  map entries: %d, free MB approx %lu\n",
              fw_mem_count(), fw_mem_free_mb());
}

static void cmd_tpm(void) {
    if (fw_tpm_present()) {
        uint8_t pcr[32];
        fw_tpm_pcr_read(7, pcr);
        fw_puts("  TPM present. PCR7 = ");
        for (int i = 0; i < 32; i++) fw_printf("%02X", pcr[i]);
        fw_puts("\n");
        fw_tpm_log_dump();
    } else {
        fw_puts("  TPM not present (software measurements only)\n");
    }
}

static void cmd_net(void) {
    EFI_SIMPLE_NETWORK_PROTOCOL *snp = fw_e1000_get_snp();
    if (!snp) { fw_puts("  no network device bound\n"); return; }
    fw_printf("  e1000 up, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
              snp->Mode->PermanentAddress.Addr[0], snp->Mode->PermanentAddress.Addr[1],
              snp->Mode->PermanentAddress.Addr[2], snp->Mode->PermanentAddress.Addr[3],
              snp->Mode->PermanentAddress.Addr[4], snp->Mode->PermanentAddress.Addr[5]);
}

static void cmd_mm(uint64_t addr, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) {
        if ((i & 0xF) == 0) fw_printf("  %08X:", (uint32_t)(addr + i));
        uint32_t v = *(volatile uint32_t *)(uintptr_t)(addr + i);
        fw_printf(" %08X", v);
        if ((i & 0xF) == 0xC) fw_puts("\n");
    }
    fw_puts("\n");
}

static void cmd_boot(const char *path) {
    fw_printf("  loading %s...\n", path);
    EFI_HANDLE h;
    if (fw_image_create_from_path(path, &h) == 0) {
        fw_puts("  image loaded; executing\n");
        /* Hand off via the same path as normal boot. */
        fw_boot_image(h);
    } else {
        fw_printf("  failed to load %s\n", path);
    }
}

/* Parse a hex literal like 0xC0008000. */
static uint64_t parse_hex(const char *s) {
    uint64_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        v = v * 16 + d;
    }
    return v;
}

void fw_shell_run(void) {
    fw_puts("\nWuBuFW interactive shell. Type 'help' for commands.\n");
    char *a[8];
    for (;;) {
        if (!line_get()) continue;
        int n = tok(a);
        if (!n) continue;
        if (fw_strcmp(a[0], "help") == 0 || fw_strcmp(a[0], "?") == 0) {
            fw_puts("  ls  cat <f>  pci  drivers  map  devices  mem  tpm  net\n"
                    "  mm <addr> [<n>]  boot <path>  ver  exit  help\n");
        } else if (fw_strcmp(a[0], "ls") == 0) {
            cmd_ls();
        } else if (fw_strcmp(a[0], "cat") == 0) {
            if (n > 1) cmd_cat(a[1]); else fw_puts("usage: cat <file>\n");
        } else if (fw_strcmp(a[0], "pci") == 0) {
            cmd_pci();
        } else if (fw_strcmp(a[0], "drivers") == 0) {
            cmd_drivers();
        } else if (fw_strcmp(a[0], "map") == 0 || fw_strcmp(a[0], "devices") == 0) {
            cmd_pci();
        } else if (fw_strcmp(a[0], "mem") == 0) {
            cmd_mem();
        } else if (fw_strcmp(a[0], "tpm") == 0) {
            cmd_tpm();
        } else if (fw_strcmp(a[0], "net") == 0) {
            cmd_net();
        } else if (fw_strcmp(a[0], "mm") == 0) {
            if (n > 1) cmd_mm(parse_hex(a[1]), n > 2 ? (uint32_t)parse_hex(a[2]) : 16);
            else fw_puts("usage: mm <addr> [<bytes>]\n");
        } else if (fw_strcmp(a[0], "boot") == 0) {
            if (n > 1) cmd_boot(a[1]);
            else fw_puts("usage: boot <path>\n");
        } else if (fw_strcmp(a[0], "ver") == 0) {
            fw_puts("  WuBuFW UEFI 2.10 (C11), WuBuOS\n");
        } else if (fw_strcmp(a[0], "exit") == 0) {
            fw_puts("  exiting shell\n");
            return;
        } else {
            fw_printf("  unknown command: %s (try 'help')\n", a[0]);
        }
    }
}
