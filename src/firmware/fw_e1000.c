/*
 * fw_e1000.c  --  WuBuFW Intel 8254x (e1000) Ethernet driver.
 *
 * QEMU's default NIC is exactly this part (8086:10D3, which we can see on the
 * PCI bus), so a real driver makes the firmware network-capable rather than
 * merely listing the device. We bring up the transmit and receive descriptor
 * rings and expose EFI_SIMPLE_NETWORK_PROTOCOL so a boot loader can PXE.
 *
 * This is the legacy e1000 MAC: a single TX ring and a single RX ring with
 * software-managed head/tail pointers, no MSI, polled completion. That is
 * the whole surface QEMU's e1000 emulates.
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_block.h"   /* not used, just to keep includes symmetric */

/* -- MMIO register offsets (only what we touch) ----------------------- */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_RCTL      0x0100
#define E1000_TCTL      0x0400
#define E1000_TIPG      0x0410
#define E1000_RDBAL     0x2800
#define E1000_RDBAH     0x2804
#define E1000_RDLEN     0x2808
#define E1000_RDH       0x2810
#define E1000_RDT       0x2818
#define E1000_TDBAL     0x3800
#define E1000_TDBAH     0x3804
#define E1000_TDLEN     0x3808
#define E1000_TDH       0x3810
#define E1000_TDT       0x3818
#define E1000_RA        0x5400   /* 2x4 bytes of MAC address filters */

#define RCTL_EN         (1u << 1)
#define RCTL_BAM        (1u << 15)
#define RCTL_SECRC      (1u << 26)
#define RCTL_BSIZE_2048 (0u << 16)
#define TCTL_EN         (1u << 1)
#define TCTL_PSP        (1u << 3)
#define CTRL_SLU        (1u << 6)    /* set link up */

#define RX_RING  16
#define TX_RING  16
#define RX_BUF   2048

typedef struct {
    uint64_t addr;
    uint16_t len;
    uint16_t cksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc;

typedef struct {
    uint64_t addr;
    uint16_t len;
    uint8_t  cksum_off;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  cksum_start;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc;

static volatile uint8_t *g_bar;
static e1000_rx_desc   *g_rx;
static e1000_tx_desc   *g_tx;
static uint8_t         *g_rxb[RX_RING];
static uint16_t         g_rdt, g_tdt;
static uint8_t          g_mac[6];
static int              g_up;

static uint32_t r32(uint32_t o) { return *(volatile uint32_t *)(g_bar + o); }
static void     w32(uint32_t o, uint32_t v) { *(volatile uint32_t *)(g_bar + o) = v; }

static void e1000_reset(void) {
    w32(E1000_CTRL, r32(E1000_CTRL) | CTRL_SLU);
    w32(E1000_IMS, 0);                       /* no interrupts; poll ICR */
    w32(E1000_ICR, 0xFFFFFFFFu);
}

static uint8_t *alloc_aligned(size_t n, size_t a) {
    uint8_t *p = fw_alloc_pages_aligned((n + a - 1) / 4096 ? (n + a - 1) / 4096 : 1, a);
    return p;
}

/* -- EFI_SIMPLE_NETWORK_PROTOCOL surface ------------------------------ */

static EFI_SIMPLE_NETWORK_PROTOCOL g_snp;
static UINT8 g_mode_buf[sizeof(EFI_SIMPLE_NETWORK_MODE) + 32];
static EFI_SIMPLE_NETWORK_MODE *g_mode = (EFI_SIMPLE_NETWORK_MODE *)g_mode_buf;

static EFI_STATUS EFIAPI snp_start(EFI_SIMPLE_NETWORK_PROTOCOL *t) {
    (void)t;
    if (g_up) return EFI_SUCCESS;
    e1000_reset();
    w32(E1000_RCTL, r32(E1000_RCTL) | RCTL_EN);
    w32(E1000_TCTL, r32(E1000_TCTL) | TCTL_EN | TCTL_PSP);
    g_up = 1;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI snp_stop(EFI_SIMPLE_NETWORK_PROTOCOL *t) {
    (void)t;
    w32(E1000_RCTL, r32(E1000_RCTL) & ~RCTL_EN);
    w32(E1000_TCTL, r32(E1000_TCTL) & ~TCTL_EN);
    g_up = 0;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI snp_init(EFI_SIMPLE_NETWORK_PROTOCOL *t, UINTN ExtraRx, UINTN ExtraTx) {
    (void)t; (void)ExtraRx; (void)ExtraTx;
    g_mode->State = EfiSimpleNetworkInitialized;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI snp_reset(EFI_SIMPLE_NETWORK_PROTOCOL *t, BOOLEAN x) {
    (void)t; (void)x;
    g_rdt = 0; g_tdt = 0;
    for (int i = 0; i < RX_RING; i++) g_rx[i].status = 0;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI snp_get_status(EFI_SIMPLE_NETWORK_PROTOCOL *t, UINT32 *irq, VOID **txbuf) {
    (void)t;
    if (irq) *irq = 0;
    if (txbuf) *txbuf = NULL;
    return EFI_SUCCESS;
}
/* Transmit one frame synchronously via the next TX descriptor. */
static EFI_STATUS EFIAPI snp_transmit(EFI_SIMPLE_NETWORK_PROTOCOL *t, UINTN hdr,
                                      UINTN len, VOID *data, EFI_MAC_ADDRESS *src, EFI_MAC_ADDRESS *dst, UINT16 *proto) {
    (void)t; (void)hdr; (void)src; (void)dst; (void)proto;
    if (len == 0 || len > 1514) return EFI_INVALID_PARAMETER;
    static uint8_t *b;
    if (!b) b = alloc_aligned(2048, 16);
    fw_memcpy(b, data, len);
    e1000_tx_desc *d = &g_tx[g_tdt];
    d->addr = (uint64_t)(uintptr_t)b;
    d->len = (uint16_t)len;
    d->cmd = 0x09;                            /* EOP | RS */
    d->status = 0;
    uint16_t next = (uint16_t)((g_tdt + 1) % TX_RING);
    w32(E1000_TDT, next);
    g_tdt = next;
    for (int i = 0; i < 500000; i++) {
        if (d->status & 0x01) return EFI_SUCCESS;
        fw_stall_us(10);
    }
    return EFI_TIMEOUT;
}
/* Receive one frame if a descriptor has completed, else EFI_NOT_READY. */
static EFI_STATUS EFIAPI snp_receive(EFI_SIMPLE_NETWORK_PROTOCOL *t, UINTN *hdr,
                                     UINTN *len, VOID *data, EFI_MAC_ADDRESS *src, EFI_MAC_ADDRESS *dst, UINT16 *proto) {
    (void)t; (void)hdr; (void)src; (void)dst; (void)proto;
    e1000_rx_desc *d = &g_rx[g_rdt];
    if (!(d->status & 0x01)) return EFI_NOT_READY;
    UINTN n = d->len;
    if (len) *len = n;
    if (n > 1514) n = 1514;
    if (data) fw_memcpy(data, g_rxb[g_rdt], n);
    d->status = 0;
    uint16_t next = (uint16_t)((g_rdt + 1) % RX_RING);
    w32(E1000_RDT, next);
    g_rdt = next;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI snp_mcast(EFI_SIMPLE_NETWORK_PROTOCOL *t, BOOLEAN IsIPv6, EFI_IP_ADDRESS *Ip, EFI_MAC_ADDRESS *Mac) {
    (void)t; (void)IsIPv6; (void)Ip; (void)Mac;
    return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI snp_filter(EFI_SIMPLE_NETWORK_PROTOCOL *t, UINT32 Enable, UINT32 Disable, BOOLEAN Reset, UINTN McastCount, EFI_MAC_ADDRESS *Mcast) {
    (void)t; (void)Enable; (void)Disable; (void)Reset; (void)McastCount; (void)Mcast;
    return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI snp_stats(EFI_SIMPLE_NETWORK_PROTOCOL *t, BOOLEAN rst, UINTN *sz, VOID *tbl) {
    (void)t; (void)rst; (void)sz; (void)tbl;
    return EFI_UNSUPPORTED;
}

EFI_SIMPLE_NETWORK_PROTOCOL *fw_e1000_get_snp(void) {
    return g_up ? &g_snp : NULL;
}

int fw_e1000_init(fw_pci_dev *d) {
    if (!d || !d->bar[0].addr) return -1;
    g_bar = (volatile uint8_t *)(uintptr_t)d->bar[0].addr;

    uint8_t *rxmem = alloc_aligned(sizeof(e1000_rx_desc) * RX_RING, 16);
    uint8_t *txmem = alloc_aligned(sizeof(e1000_tx_desc) * TX_RING, 16);
    if (!rxmem || !txmem) return -1;
    g_rx = (e1000_rx_desc *)rxmem;
    g_tx = (e1000_tx_desc *)txmem;
    fw_memset(g_rx, 0, sizeof(e1000_rx_desc) * RX_RING);
    fw_memset(g_tx, 0, sizeof(e1000_tx_desc) * TX_RING);
    for (int i = 0; i < RX_RING; i++) {
        g_rxb[i] = alloc_aligned(RX_BUF, 16);
        if (!g_rxb[i]) return -1;
        g_rx[i].addr = (uint64_t)(uintptr_t)g_rxb[i];
        g_rx[i].status = 0;
    }

    e1000_reset();

    /* Program the RX ring. */
    w32(E1000_RDBAL, (uint32_t)(uintptr_t)g_rx);
    w32(E1000_RDBAH, (uint32_t)(((uint64_t)(uintptr_t)g_rx) >> 32));
    w32(E1000_RDLEN, (uint32_t)(sizeof(e1000_rx_desc) * RX_RING));
    w32(E1000_RDH, 0);
    w32(E1000_RDT, RX_RING - 1);

    /* Program the TX ring. */
    w32(E1000_TDBAL, (uint32_t)(uintptr_t)g_tx);
    w32(E1000_TDBAH, (uint32_t)(((uint64_t)(uintptr_t)g_tx) >> 32));
    w32(E1000_TDLEN, (uint32_t)(sizeof(e1000_tx_desc) * TX_RING));
    w32(E1000_TDH, 0);
    w32(E1000_TDT, 0);

    w32(E1000_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);
    w32(E1000_TCTL, TCTL_EN | TCTL_PSP);
    w32(E1000_TIPG, 0x0060200A);

    /* Read the factory MAC from RA. */
    uint32_t lo = r32(E1000_RA), hi = r32(E1000_RA + 4);
    g_mac[0] = lo & 0xFF; g_mac[1] = (lo >> 8) & 0xFF;
    g_mac[2] = (lo >> 16) & 0xFF; g_mac[3] = (lo >> 24) & 0xFF;
    g_mac[4] = hi & 0xFF; g_mac[5] = (hi >> 8) & 0xFF;

    fw_memset(g_mode, 0, sizeof(*g_mode));
    g_mode->State = EfiSimpleNetworkStopped;
    g_mode->HwAddressSize = 6;
    fw_memcpy(&g_mode->PermanentAddress, g_mac, 6);
    fw_memcpy(&g_mode->CurrentAddress, g_mac, 6);
    g_mode->MaxPacketSize = 1514;
    g_mode->IfType = 1;                       /* Ethernet */
    g_mode->MacAddressChangeable = 0;

    g_snp.Revision        = 0x00010000;
    g_snp.Start           = snp_start;
    g_snp.Stop            = snp_stop;
    g_snp.Initialize      = snp_init;
    g_snp.Reset           = snp_reset;
    g_snp.Shutdown        = snp_stop;
    g_snp.ReceiveFilters  = snp_filter;
    g_snp.StationAddress  = NULL;
    g_snp.Statistics      = snp_stats;
    g_snp.MCastIPtoMAC    = snp_mcast;
    g_snp.NvData          = NULL;
    g_snp.GetStatus       = snp_get_status;
    g_snp.Transmit        = snp_transmit;
    g_snp.Receive         = snp_receive;
    g_snp.Mode            = g_mode;
    g_snp.WaitForPacket   = NULL;

    fw_printf("[e1000] MAC %02X:%02X:%02X:%02X:%02X:%02X, link %s\n",
              g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5],
              (r32(E1000_STATUS) & (1u << 1)) ? "up" : "down");

    EFI_HANDLE h = fw_efi_new_handle();
    if (h) fw_efi_install(h, &gEfiSimpleNetworkProtocolGuid, &g_snp);
    g_up = 1;
    return 0;
}
