<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Public API
> Auto-generated.  Heuristic extraction of top-level functions.

## `ahci` (src/kernel/ahci.c)

- `ahci_port_init(ahci_hba_t *hba, int port_num)`

## `fat32` (src/kernel/fat32.c)

- `datetime_to_dos(time_t t, uint16_t *dos_time, uint16_t *dos_date)`

## `fat32_cluster` (src/kernel/fat32_cluster.c)

- `fat32_next_cluster(fat32_volume *vol, uint32_t cluster)`
- `fat32_cluster_to_lba(fat32_volume *vol, uint32_t cluster)`
- `fat32_lba_to_cluster(fat32_volume *vol, uint64_t lba)`
- `fat32_alloc_clusters(fat32_volume *vol, uint32_t count, uint32_t hint)`

## `fat32_dir` (src/kernel/fat32_dir.c)

- `fat32_delete(fat32_volume *vol, uint32_t dir_cluster, const char *name)`

## `fat32_format` (src/kernel/fat32_format.c)

- `fat32_mount(fat32_volume *vol, const fat32_blk_ops *blk)`

## `fat32_name` (src/kernel/fat32_name.c)

- `name_to_83(const char *src, char name83[11])`

## `interrupt` (src/kernel/interrupt.c)

- `interrupt_register(uint8_t irq, void (*handler)(uint8_t irq, void *ctx), void *ctx)`
- `interrupt_unregister(uint8_t irq)`
- `interrupt_eoi(uint8_t irq)`
- `interrupt_disable(void)`
- `interrupt_enable(void)`
- `interrupt_fire(uint8_t irq)`
- `pit_handler(uint8_t irq, void *ctx)`
- `isr_dispatch(uint8_t vector, struct InterruptFrame *frame)`
- `interrupt_init(void)`
- `interrupt_shutdown(void)`
- `interrupt_init_full(void)`
- `interrupt_count(uint8_t irq)`
- `interrupt_get_count(uint8_t irq)`
- `syscall_handler(InterruptFrame *frame, uint64_t num)`

## `interrupt_apic` (src/kernel/interrupt_apic.c)

- `apic_init(void)`
- `ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id)`
- `lapic_timer_init(uint32_t hz, uint8_t vector)`
- `lapic_send_ipi(uint32_t dest_apic_id, uint8_t vector, uint8_t delivery_mode)`

## `interrupt_pic` (src/kernel/interrupt_pic.c)

- `pic_remap(uint8_t offset1, uint8_t offset2)`
- `pic_eoi(uint8_t vector)`
- `irq_route_add(uint8_t src_irq, uint8_t dst_vector, uint8_t dest_apic_id, uint16_t flags)`

## `interrupt_pic_test` (src/kernel/interrupt_pic_test.c)

- `ioapic_route_irq(uint8_t a, uint8_t b, uint8_t c)`
- `main(void)`

## `interrupt_pit` (src/kernel/interrupt_pit.c)

- `pit_init(uint32_t hz)`

## `interrupt_syscall` (src/kernel/interrupt_syscall.c)

- `syscall_init(void)`

## `interrupt_timer` (src/kernel/interrupt_timer.c)

- `timer_calibrate_tsc(void)`
- `timer_init_deadline(uint64_t ns)`

## `klog` (src/kernel/klog.c)

- `klog_init(void)`

## `libc` (src/kernel/libc.c)

- `free(void *ptr)`
- `fprintf(FILE *stream, const char *fmt, ...)`
- `printf(const char *fmt, ...)`
- `snprintf(char *str, size_t size, const char *fmt, ...)`

## `metal_main` (src/kernel/metal_main.c)

- `kernel_main(void *boot_info)`
- `wubu_shell_run(void *arg)`
- `wubu_gaad_init(void)`
- `kernel_panic(const char *msg)`

## `ps2` (src/kernel/ps2.c)

- `ps2_init(int screen_w, int screen_h)`
- `ps2_mouse_handler(void)`

## `tasking` (src/kernel/tasking.c)

- `task_destroy(CTask *task)`
- `task_yield(void)`

## `tasking_test` (src/kernel/tasking_test.c)

- `main(void)`

## `test_agi_kernel_stub` (src/kernel/test_agi_kernel_stub.c)

- `klog_printf(const char *fmt, ...)`

## `test_hive` (src/kernel/test_hive.c)

- `main(void)`

## `txfs` (src/kernel/txfs.c)

- `txfs_shutdown(txfs_t *tx)`
- `txfs_truncate(txfs_t *tx, const char *path, uint64_t size)`
- `txfs_create(txfs_t *tx, const char *path)`
- `txfs_delete(txfs_t *tx, const char *path)`
- `txfs_mkdir(txfs_t *tx, const char *path)`
- `txfs_rename(txfs_t *tx, const char *old_path, const char *new_path)`

## `vbe` (src/kernel/vbe.c)

- `vbe_init(int width, int height)`
- `vbe_shutdown(void)`
- `vbe_fill_rect_clip(int x, int y, int w, int h, uint32_t color)`
- `vbe_shade_rect(int x, int y, int w, int h)`
- `vbe_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom)`
- `vbe_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha)`
- `vbe_hgradient(int x, int y, int w, int h, uint32_t left, uint32_t right)`
- `vbe_fill_circle(int cx, int cy, int r, uint32_t color)`
- `vbe_draw_cursor(int mx, int my)`
- `vbe_fill_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color)`

## `wubu_agi_kernel` (src/kernel/wubu_agi_kernel.c)

- `wubu_agi_kernel_run(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_tick(wubu_agi_kernel_t *k)`

## `wubu_apic` (src/kernel/wubu_apic.c)

- `wubu_map_phys_range(uint64_t phys, uint32_t pages)`

## `wubu_attest` (src/kernel/wubu_attest.c)

- `wubu_attest_ingest(const void *raw)`
- `wubu_attest_ingest_handoff(const void *handoff)`
- `wubu_attest_load_scratch(void)`
- `wubu_attest_clear(void)`
- `wubu_attest_pcr(unsigned i, uint8_t out[WUBU_AGI_PCR_SZ])`
- `wubu_attest_pcr4_digest(uint8_t out[WUBU_AGI_PCR_SZ])`
- `wubu_attest_kernel_digest(uint8_t out[WUBU_AGI_PCR_SZ])`
- `wubu_attest_kernel_size(void)`

## `wubu_gaad` (src/kernel/wubu_gaad.c)

- `wubu_clamp(int val, int lo, int hi)`
- `wubu_gaad_region_scale(const WubuGaadTranslate *t, int region_idx)`

## `wubu_hive` (src/kernel/wubu_hive.c)

- `wubu_hive_erase_at(wubu_hive_t *h, wubu_hive_iter_t *it)`
- `wubu_hive_clear(wubu_hive_t *h)`

## `wubu_math` (src/kernel/wubu_math.c)

- `wubu_floor(double x)`

## `wubu_pci` (src/kernel/wubu_pci.c)

- `wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)`
- `wubu_pci_scan(wubu_pci_dev_t *out, int max)`

## `fw_agi` (src/firmware/fw_agi.c)

- `fw_agi_publish_attest(void)`
- `fw_agi_attest_and_boot(const char *path)`

## `fw_ahci` (src/firmware/fw_ahci.c)

- `fw_ahci_read(int idx, uint64_t lba, uint32_t count, void *buf)`
- `fw_ahci_write(int idx, uint64_t lba, uint32_t count, const void *buf)`
- `fw_ahci_sectors(int idx)`
- `fw_ahci_init(fw_pci_dev *d)`

## `fw_ata` (src/firmware/fw_ata.c)

- `fw_ata_read(fw_ata_dev *d, uint64_t lba, uint32_t count, void *buf)`
- `fw_ata_write(fw_ata_dev *d, uint64_t lba, uint32_t count, const void *buf)`

## `fw_block` (src/firmware/fw_block.c)

- `fw_block_read(int i, uint64_t lba, uint32_t count, void *buf)`
- `fw_block_write(int i, uint64_t lba, uint32_t count, const void *buf)`

## `fw_bs_mem` (src/firmware/fw_bs_mem.c)

- `fw_bs_raise_tpl(EFI_TPL n)`
- `fw_bs_free_pages(EFI_PHYSICAL_ADDRESS mem, UINTN pages)`
- `fw_bs_alloc_pool(EFI_MEMORY_TYPE type, UINTN size, VOID **buf)`
- `fw_bs_free_pool(VOID *buf)`
- `fw_bs_set_timer(EFI_EVENT e, EFI_TIMER_DELAY type, UINT64 trigger)`
- `fw_bs_check_event(EFI_EVENT e)`
- `fw_bs_wait_for_event(UINTN n, EFI_EVENT *evs, UINTN *index)`
- `fw_bs_crc32(VOID *data, UINTN size, UINT32 *out)`

## `fw_bs_proto` (src/firmware/fw_bs_proto.c)

- `fw_bs_close_protocol(EFI_HANDLE h, EFI_GUID *g, EFI_HANDLE a, EFI_HANDLE c)`
- `fw_bs_open_protocol_info(EFI_HANDLE h, EFI_GUID *g, VOID **buf, UINTN *cnt)`
- `fw_bs_register_notify(EFI_GUID *g, EFI_EVENT e, VOID **reg)`
- `fw_bs_locate_protocol(EFI_GUID *g, VOID *reg, VOID **iface)`
- `fw_bs_protocols_per_handle(EFI_HANDLE h, EFI_GUID ***out, UINTN *cnt)`
- `fw_bs_install_multiple(EFI_HANDLE *h, ...)`
- `fw_bs_start_image(EFI_HANDLE h, UINTN *exitsz, CHAR16 **exitdata)`
- `fw_bs_exit(EFI_HANDLE h, EFI_STATUS status, UINTN dsz, CHAR16 *data)`
- `fw_bs_unload_image(EFI_HANDLE h)`
- `fw_bs_exit_boot_services(EFI_HANDLE h, UINTN key)`

## `fw_drivers` (src/firmware/fw_drivers.c)

- `fw_drivers_init(void)`
- `fw_measure_gpt(void)`
- `fw_measure_secureboot(int enabled, int setup_mode)`

## `fw_fwcfg` (src/firmware/fw_fwcfg.c)

- `fw_cfg_init(void)`

## `fw_handle` (src/firmware/fw_handle.c)

- `fw_efi_new_handle(void)`
- `fw_efi_install(EFI_HANDLE h, EFI_GUID *guid, void *iface)`
- `fw_efi_uninstall(EFI_HANDLE h, EFI_GUID *guid)`
- `fw_efi_lookup(EFI_HANDLE h, EFI_GUID *guid, void **out)`

## `fw_main` (src/firmware/fw_main.c)

- `fw_image_create_from_path(const char *path, EFI_HANDLE *out)`
- `fw_boot_run(void)`

## `fw_media` (src/firmware/fw_media.c)

- `fw_vol_stat(fw_volume *v, const char *path, uint64_t *size, uint32_t *attr)`
- `fw_vol_read_file(fw_volume *v, const char *path, void **out, uint64_t *size)`
- `fw_volume_reset(int vol)`

## `fw_mem` (src/firmware/fw_mem.c)

- `fw_mem_init(void)`
- `fw_mem_free_mb(void)`
- `fw_pool_free(void *p)`

## `fw_nvme` (src/firmware/fw_nvme.c)

- `fw_nvme_init(fw_pci_dev *d)`

## `fw_pci` (src/firmware/fw_pci.c)

- `fw_pci_set_ecam(uint64_t base, uint8_t start_bus, uint8_t end_bus)`
- `fw_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off)`
- `fw_pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off)`
- `fw_pci_read8(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off)`
- `fw_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint32_t v)`
- `fw_pci_write16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint16_t v)`
- `fw_pci_dump(void)`

## `fw_pcires` (src/firmware/fw_pcires.c)

- `fw_pci_assign_resources(void)`

## `fw_pe` (src/firmware/fw_pe.c)

- `fw_pe_load(const void *file, uint64_t file_size, fw_pe_image *out)`

## `fw_rt` (src/firmware/fw_rt.c)

- `fw_rt_get_time(EFI_TIME *t, EFI_TIME_CAPABILITIES *caps)`
- `fw_rt_get_wakeup(BOOLEAN *en, BOOLEAN *pend, EFI_TIME *t)`
- `fw_rt_set_wakeup(BOOLEAN en, EFI_TIME *t)`
- `fw_rt_convert_pointer(UINTN disp, VOID **addr)`
- `fw_rt_get_next_variable(UINTN *nsz, CHAR16 *name, EFI_GUID *guid)`
- `fw_rt_get_next_high_mono(UINT32 *high)`
- `fw_rt_reset(EFI_RESET_TYPE type, EFI_STATUS status, UINTN dsz, VOID *data)`

## `fw_secureboot` (src/firmware/fw_secureboot.c)

- `fw_sb_enroll_db(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_enroll_dbx(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_set_pk(void)`
- `fw_sb_selftest(void)`
- `fw_sb_selftest_pe(void)`
- `fw_sb_verify(const uint8_t *image, uint32_t size, uint8_t *hash)`

## `fw_sha256` (src/firmware/fw_sha256.c)

- `sha256_init(sha256_ctx *c)`
- `sha256_update(sha256_ctx *c, const void *data, uint64_t len)`
- `sha256_final(sha256_ctx *c, uint8_t out[32])`
- `fw_sha256(const void *data, uint64_t len, uint8_t out[32])`

## `fw_time` (src/firmware/fw_time.c)

- `fw_time_init(void)`

## `fw_tpm` (src/firmware/fw_tpm.c)

- `fw_tpm_selftest_self(void)`
- `fw_tpm_pcr_read(uint32_t pcr, uint8_t out[32])`

## `fw_tpmlog` (src/firmware/fw_tpmlog.c)

- `fw_tpm_log_dump(void)`

## `control_test` (src/apps/control_test.c)

- `main(void)`

## `dosgui_apps` (src/apps/dosgui_apps.c)

- `dosgui_launch_temple_repl(void)`
- `dosgui_launch_notepad(void)`
- `dosgui_launch_paint(void)`

## `dosgui_apps_test` (src/apps/dosgui_apps_test.c)

- `main(void)`

## `repl` (src/apps/repl.c)

- `repl_start(int fb_w, int fb_h)`

## `wubu_canvas_draw` (src/apps/wubu_canvas_draw.c)

- `wubu_cv_fill(WubuCanvas *cv, int x, int y)`

## `wubu_canvas_filter` (src/apps/wubu_canvas_filter.c)

- `wubu_cv_filter_blur(WubuCanvas *cv, int radius)`
- `wubu_cv_filter_sharpen(WubuCanvas *cv, int amount)`

## `wubu_canvas_io_ppm` (src/apps/wubu_canvas_io_ppm.c)

- `wubu_cv_save_ppm(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_ppm(WubuCanvas *cv, const char *path)`

## `wubu_canvas_plugin` (src/apps/wubu_canvas_plugin.c)

- `wubu_cv_plugin_register(WubuCanvas *cv, const WubuPlugin *plugin)`
- `wubu_cv_plugin_run(WubuCanvas *cv, int plugin_idx)`
- `wubu_cv_plugin_unregister(WubuCanvas *cv, int plugin_idx)`

## `wubu_canvas_transform` (src/apps/wubu_canvas_transform.c)

- `wubu_cv_zoom_out(WubuCanvas *cv)`
- `wubu_cv_zoom_fit(WubuCanvas *cv)`
- `wubu_cv_pan(WubuCanvas *cv, int dx, int dy)`

## `wubu_canvas_undo` (src/apps/wubu_canvas_undo.c)

- `wubu_cv_undo(WubuCanvas *cv)`
- `wubu_cv__undo_push(WubuCanvas *cv)`

## `wubu_codec` (src/apps/wubu_codec.c)

- `wubu_dec_seek(WubuDecoder *dec, double timestamp)`

## `wubu_editor` (src/apps/wubu_editor.c)

- `wubu_ed_destroy(WubuEditor *ed)`
- `wubu_ed_fold_toggle(WubuEditor *ed, int line)`

## `wubu_editor_bookmark` (src/apps/wubu_editor_bookmark.c)

- `wubu_ed_bookmark_toggle(WubuEditor *ed, int line)`
- `wubu_ed_bookmark_next(WubuEditor *ed)`

## `wubu_editor_macro` (src/apps/wubu_editor_macro.c)

- `wubu_ed_macro_start(WubuEditor *ed)`
- `wubu_ed_macro_stop(WubuEditor *ed)`
- `wubu_ed_macro_play(WubuEditor *ed)`

## `wubu_image_codec` (src/apps/wubu_image_codec.c)

- `crc32_data(const void *data, size_t len)`
- `write_be32(uint8_t *buf, uint32_t val)`
- `write_be16(uint8_t *buf, uint16_t val)`
- `png_write_chunk(FILE *f, const char *type, const void *data, size_t len)`
- `png_unfilter(uint8_t *raw, int w, int h, int channels)`

## `hosted` (src/hosted/hosted.c)

- `hosted_init(hosted_state_t *state, int argc, char **argv)`
- `if(strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)`

## `hosted_pe` (src/hosted/hosted_pe.c)

- `hosted_pe_executor(const void *data, size_t size, const char *cmdline)`

## `hosted_run` (src/hosted/hosted_run.c)

- `hosted_run(hosted_state_t *state)`
- `hosted_blit(hosted_state_t *state)`
- `hosted_set_mode(hosted_state_t *state, hosted_mode_t mode)`
- `hosted_kernel_ready(void)`
- `hosted_wm_has_windows(void)`

## `hosted_styxfs` (src/hosted/hosted_styxfs.c)

- `styx_attach_cb(styx_server_t *srv, uint32_t fid, const char *aname)`

## `hosted_wayland` (src/hosted/hosted_wayland.c)

- `dosgui_platform_shutdown(void)`
- `hosted_wl_connect(hosted_state_t *state)`

## `hosted_wayland_shm` (src/hosted/hosted_wayland_shm.c)

- `shm_buffer_create(shm_buffer_t *buf, int w, int h)`
- `shm_buffer_destroy(shm_buffer_t *buf)`
- `hosted_wl_frame_render(void)`

## `hosted_wayland_surface` (src/hosted/hosted_wayland_surface.c)

- `wl_surface_term(void)`

## `wubu_display` (src/hosted/wubu_display.c)

- `wubu_display_swap(WubuDisplay *d)`

## `wubu_display_test` (src/hosted/wubu_display_test.c)

- `main(void)`

## `wubu_metal` (src/hosted/wubu_metal.c)

- `wubu_wsl2_disp_init(void)`
- `wubu_disp_init(int width, int height)`
- `wubu_disp_set_mode(int width, int height, int refresh_hz)`
- `wubu_disp_flip(void)`
- `wubu_disp_poll_events(void)`
- `wubu_input_init(void)`
- `wubu_input_gamepads(char names[][64])`
- `wubu_audio_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_audio_cpu_load(void)`
- `wubu_metal_init(int width, int height)`
- `wubu_disp_get_modes(int *widths, int *heights, int max)`

## `wubu_metal_audio` (src/hosted/wubu_metal_audio.c)

- `wubu_alsa_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_alsa_shutdown(void)`
- `wubu_alsa_submit(const float *buf, int frames)`
- `wubu_alsa_cpu_load(void)`
- `wubu_pulse_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pulse_shutdown(void)`
- `wubu_pulse_submit(const float *buf, int frames)`

## `wubu_metal_drm` (src/hosted/wubu_metal_drm.c)

- `wubu_drm_init(int width, int height)`
- `wubu_drm_shutdown(void)`
- `wubu_drm_flip(void)`

## `wubu_metal_evdev` (src/hosted/wubu_metal_evdev.c)

- `wubu_evdev_find_device(const char *type, int *out_fd)`

## `wubu_metal_x11` (src/hosted/wubu_metal_x11.c)

- `wubu_x11_init(int width, int height)`
- `wubu_x11_shutdown(void)`
- `wubu_x11_flip(void)`
- `wubu_x11_set_mode(int width, int height, int refresh_hz)`
- `wubu_x11_set_mode(int width, int height, int refresh_hz)`

## `wubu_vulkan_compute` (src/hosted/wubu_vulkan_compute.c)

- `wubu_vk_shader_module_destroy(WubuVkShaderModule *shader)`

## `wubu_vulkan_swapchain` (src/hosted/wubu_vulkan_swapchain.c)

- `wubu_vk_swapchain_destroy(WubuVkSwapchain *sc)`
- `wubu_vk_swapchain_acquire(WubuVkSwapchain *sc, uint64_t timeout_ns)`
- `wubu_vk_swapchain_present(WubuVkSwapchain *sc)`

