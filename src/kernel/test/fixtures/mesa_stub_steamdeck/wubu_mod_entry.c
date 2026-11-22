/* Minimal git-sourced driver module used by wubu_drv_install_selftest.
 *
 * Compiled with -ffreestanding into ET_REL and loaded by wubu_drv_elf_load.
 * wubu_mod_entry calls wubu_drv_register through an undefined symbol so
 * the loader's PLT relocation path is exercised, not just ELF magic. */
typedef struct {
    unsigned short vendor, device;
    unsigned char class_code, subclass;
} wubu_drv_id_t;

typedef struct {
    const char *name;
    const wubu_drv_id_t *id_table;
    int n_ids;
    int (*probe)(void *);
} wubu_drv_t;

extern int wubu_drv_register(const wubu_drv_t *drv);

static wubu_drv_t git_stub_drv = { "git_stub", 0, 0, 0 };

int wubu_mod_entry(void)
{
    return wubu_drv_register(&git_stub_drv);
}
