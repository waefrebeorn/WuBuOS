/*
 * drv_stubs.c -- SELFTEST stub drivers for the extern symbols that
 * wubu_drv_init() registers (wubu_drv_nvme, … wubu_drv_intel_platform).
 * The real driver bodies live in src/kernel/wubu_drv_*.c; the selftest
 * only exercises the *binding + self-install* path, so it stubs these 18
 * symbols with empty (no-probe) drivers so wubu_drv_init() links.
 */
#include "wubu_drv.h"

#define DEF_STUB(nm) \
    const wubu_drv_t wubu_drv_##nm = { \
        .name = "stub_" #nm, .id_table = NULL, .n_ids = 0, .probe = NULL \
    }

DEF_STUB(nvme);
DEF_STUB(ahci);
DEF_STUB(wifi);
DEF_STUB(net);
DEF_STUB(hda);
DEF_STUB(gpu);
DEF_STUB(battery);
DEF_STUB(sd);
DEF_STUB(usb_hid);
DEF_STUB(usb_msc);
DEF_STUB(usb_bt);
DEF_STUB(thermal);
DEF_STUB(virtio_blk);
DEF_STUB(virtio_net);
DEF_STUB(virtio_gpu);
DEF_STUB(virtio_input);
DEF_STUB(arm_platform);
DEF_STUB(intel_platform);
