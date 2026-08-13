/*
 * drv_demo.c -- the self-install selftest's demo driver module.
 *
 * This is compiled (by wubu_drv_build) to an ET_REL .o and loaded IN
 * MEMORY by wubu_drv_elf_load — proving the kernel installs its own
 * drivers. It registers a driver for PCI vendor 0xCAFE (a synthetic
 * device the selftest injects), calling back into the registry via the
 * exported symbol wubu_drv_register.
 *
 * wubu_mod_entry is the module entry point the loader resolves and calls.
 */
#include "wubu_drv.h"

/* imported from the kernel export table */
extern int wubu_drv_register(const wubu_drv_t *drv);

static const wubu_drv_id_t cafe_ids[] = {
    { 0xCAFE, WUBU_DRV_ANY, 0x00, 0x00 },
};

static int cafe_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    return 0; /* probed OK */
}

static const wubu_drv_t cafe_driver = {
    .name = "cafe_demo",
    .id_table = cafe_ids,
    .n_ids = 1,
    .probe = cafe_probe,
};

int wubu_mod_entry(void)
{
    return wubu_drv_register(&cafe_driver);
}
