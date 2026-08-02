<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Public API (header-sourced)
> Generated 2026-08-02 10:08 UTC -- prototypes extracted from the headers = the real interface contracts.

> 629 prototypes across the tree.

## `src/tools/iso9660.h`

- `iso_builder_build(iso_builder_t *b)`

## `src/tools/screenshot.h`

- `wubu_gif_start(const char *path, int w, int h, int delay_ms, int max_frames)`
- `wubu_gif_add_frame(int x, int y, int w, int h)`
- `wubu_gif_stop(void)`
- `wubu_snip_tool_init(void)`
- `wubu_snip_tool_shutdown(void)`
- `wubu_snip_tool_activate(WubuSnipMode mode)`
- `wubu_snip_tool_deactivate(void)`
- `wubu_snip_tool_render(uint32_t *fb, int fb_w, int fb_h)`
- `wubu_snip_tool_handle_mouse(int x, int y, int btn, int kind)`
- `wubu_snip_tool_save(const char *path, WubuShotFormat fmt)`

## `src/tools/weight_check.h`

- `weight_check(weight_check_t *result)`
- `weight_shard_path(int index, char *buf, int bufsz)`
- `weight_validate_file(const char *path, uint64_t min_size)`

## `src/worldsim/worldsim.h`

- `ws_terrain_generate(ws_terrain_t *t, uint32_t seed)`
- `ws_terrain_erode(ws_terrain_t *t, int iterations)`
- `ws_terrain_height(const ws_terrain_t *t, int x, int y)`
- `ws_terrain_biome(const ws_terrain_t *t, int x, int y)`
- `ws_physics_init(ws_physics_config_t *cfg)`
- `ws_physics_step(ws_world_t *w, const ws_terrain_t *t, const ws_physics_config_t *cfg)`
- `ws_render_terrain(const ws_terrain_t *t, ws_render_ctx_t *ctx)`
- `ws_render_entities(const ws_world_t *w, ws_render_ctx_t *ctx)`
- `ws_render_minimap(const ws_terrain_t *t, ws_render_ctx_t *ctx, int mx, int my, int size)`
- `ws_rng_int(uint64_t *state, int lo, int hi)`

## `src/jit/jit.h`

- `jit_call0(JITFunc *fn)`
- `jit_call1(JITFunc *fn, int64_t a0)`
- `jit_call2(JITFunc *fn, int64_t a0, int64_t a1)`
- `jit_free_exec(void *ptr, size_t size)`
- `jit_lock_exec(void *ptr, size_t size)`
- `jit_unlock_exec(void *ptr, size_t size)`
- `jit_stats(const JITContext *ctx, JITStats *out)`

## `src/jit/wubu_disasm.h`

- `wdisasm_dump(const uint8_t *code, size_t code_len, FILE *out)`

## `src/jit/wubu_x86.h`

- `wx86_patch_rel32(Wx86Enc *e, size_t patch_pos, size_t target_pos)`

## `src/jit/x86_regalloc.h`

- `xra_alloc(XRARegAlloc *ra, int vreg)`
- `xra_finalize(XRARegAlloc *ra)`
- `xra_callee_saved_list(const XRARegAlloc *ra, Wx86Reg *out, int max)`

## `src/audio/wubu_audio.h`

- `wubu_audio_engine_create(int sample_rate, int buffer_frames, int channels)`
- `wubu_audio_engine_destroy(void)`
- `wubu_furnace_init(int n_chips, const WubuChipType *chips)`
- `wubu_furnace_shutdown(void)`
- `wubu_sf2_load(const uint8_t *data, size_t size)`
- `wubu_sf2_load_file(const char *path)`
- `wubu_sf2_unload(void)`

## `src/kernel/ahci.h`

- `ahci_identify(ahci_hba_t *hba, int port_num)`
- `ahci_sim_disk_create(ahci_hba_t *hba, int port_num, int size_mb)`

## `src/kernel/fat32.h`

- `fat32_mount(fat32_volume *vol, const fat32_blk_ops *blk)`
- `fat32_unmount(fat32_volume *vol)`
- `fat32_next_cluster(fat32_volume *vol, uint32_t cluster)`
- `fat32_alloc_clusters(fat32_volume *vol, uint32_t count, uint32_t hint)`
- `fat32_free_chain(fat32_volume *vol, uint32_t cluster)`
- `fat32_count_free(fat32_volume *vol)`
- `fat32_delete(fat32_volume *vol, uint32_t dir_cluster, const char *name)`
- `fat32_close(fat32_file *fp)`
- `fat32_read(fat32_file *fp, void *buf, size_t n)`
- `fat32_write(fat32_file *fp, const void *buf, size_t n)`
- `fat32_seek(fat32_file *fp, int64_t offset, int whence)`
- `fat32_validate(fat32_volume *vol)`
- `fat32_info(fat32_volume *vol, char *buf, size_t buf_size)`

## `src/kernel/input.h`

- `input_key_pressed(uint32_t scancode)`

## `src/kernel/interrupt.h`

- `pit_shutdown(void)`

## `src/kernel/interrupt_pic.h`

- `pic_remap(uint8_t offset1, uint8_t offset2)`
- `pic_eoi(uint8_t vector)`
- `irq_route_remove(uint8_t src_irq)`

## `src/kernel/klog.h`

- `klog_printf(const char *fmt, ...)`

## `src/kernel/memory.h`

- `mem_free(void *ptr)`
- `mem_bloom_scan(uint32_t target_sig, MemBloomFn callback, void *ctx)`

## `src/kernel/tasking.h`

- `task_timer_tick(void)`
- `task_switch_to(CTask *target)`

## `src/kernel/txfs.h`

- `txfs_commit(txfs_t *tx)`
- `txfs_abort(txfs_t *tx)`
- `txfs_recover(txfs_t *tx)`

## `src/kernel/vbe.h`

- `vbe_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha)`
- `vbe_set_clip(int x, int y, int w, int h)`
- `vbe_reset_clip(void)`
- `vbe_get_clip(int *x, int *y, int *w, int *h)`

## `src/kernel/wubu_agi_kernel.h`

- `wubu_agi_kernel_run(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_tick(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_freeze(wubu_agi_kernel_t *k, bool frozen)`
- `wubu_agi_kernel_is_frozen(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_cycle(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_trace_count(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_promoted_total(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_region_count(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_uptime_ms(const wubu_agi_kernel_t *k)`

## `src/kernel/wubu_apic.h`

- `wubu_apic_enable(void)`
- `wubu_map_phys_range(uint64_t phys, uint32_t pages)`

## `src/kernel/wubu_attest.h`

- `wubu_attest_ingest(const void *raw)`
- `wubu_attest_ingest_handoff(const void *handoff)`
- `wubu_attest_load_scratch(void)`

## `src/kernel/wubu_bonzi.h`

- `wubu_bonzi_tick(wubu_bonzi_t *b)`

## `src/kernel/wubu_gaad.h`

- `wubu_gaad_region_scale(const WubuGaadTranslate *t, int region_idx)`

## `src/kernel/wubu_hid.h`

- `wubu_hid_filter(uint8_t device, uint32_t kind_mask)`
- `wubu_hid_disable(uint8_t device, bool disabled)`

## `src/kernel/wubu_hive.h`

- `wubu_hive_destroy(wubu_hive_t *h)`
- `wubu_hive_size(const wubu_hive_t *h)`
- `wubu_hive_capacity(const wubu_hive_t *h)`
- `wubu_hive_block_count(const wubu_hive_t *h)`
- `wubu_hive_empty(const wubu_hive_t *h)`
- `wubu_hive_erase_at(wubu_hive_t *h, wubu_hive_iter_t *it)`

## `src/kernel/wubu_pci.h`

- `wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)`

## `src/kernel/wubu_theme.h`

- `wubu_theme_node_set(const char *path, uint32_t value)`
- `wubu_theme_node_list(char *buf, int bufsz)`

## `src/kernel/wubu_usb.h`

- `wubu_usb_init(void)`

## `src/runtime/ct_iso_cgroup.h`

- `wubu_cgroup_write(const char *path, const char *value)`

## `src/runtime/ct_iso_seccomp.h`

- `wubu_ct_apply_seccomp(void *ct_ptr)`
- `wubu_seccomp_install(SeccompProfile profile)`
- `runtime_to_seccomp(CtRuntime runtime)`
- `wubu_ct_child_isolation(void)`

## `src/runtime/styxfs.h`

- `styxfs_mount(styxfs_server_t *srv, const char *path, const char *source, int is_repo)`

## `src/runtime/styxfs_internal.h`

- `styxfs_add_child(styxfs_node_t *parent, styxfs_node_t *child)`
- `styxfs_next_qid_path(styxfs_server_t *srv)`
- `styxfs_file_free(styxfs_server_t *srv, uint64_t qid_path)`

## `src/runtime/styxfs_server.h`

- `styxfs_server_destroy(styx_server_t *srv)`

## `src/runtime/wubu_arch.h`

- `wubu_arch_install(const char *root_path, const char *packages)`
- `wubu_arch_pacman(const char *root_path, const char *args)`
- `wubu_arch_update(const char *root_path)`
- `wubu_arch_enable_service(const char *root_path, const char *service)`
- `wubu_arch_mkdir_p(const char *path, mode_t mode)`
- `wubu_arch_configure(const char *root_path)`
- `wubu_arch_bootstrap_gui(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_steam(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_steam_runtime2(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_gaming(const char *root_path, const char *mirror)`
- `wubu_arch_root_valid(const char *root_path)`
- `wubu_arch_root_free(WubuArchRoot *info)`

## `src/runtime/wubu_archd.h`

- `wubu_archd_stop(WubuArchd *d)`
- `wubu_archd_shutdown(WubuArchd *d)`

## `src/runtime/wubu_archd_svc.h`

- `wubu_svc_supervisor_poll(wubu_svc_supervisor_t *s)`
- `wubu_svc_supervisor_boot(wubu_svc_supervisor_t *s)`
- `wubu_archd_svc_set_supervisor(wubu_svc_supervisor_t *s)`

## `src/runtime/wubu_bottles.h`

- `wubu_bottle_list(const char *install_dir, WubuBottle ***out_bottles, int *count)`
- `wubu_bottle_verify(WubuBottle *bottle)`

## `src/runtime/wubu_compat_db.h`

- `wubu_compat_cache_dir(const char *title, char *out_path, int path_len)`
- `wubu_compat_normalize_title(const char *raw, char *out, int out_len)`

## `src/runtime/wubu_container.h`

- `wubu_container_validate(const void *data, size_t data_size)`
- `wubu_detect_payload_type(const void *data, size_t size)`
- `wubu_launch_windows(const void *data, size_t size, const char *cmdline)`

## `src/runtime/wubu_ct_isolate.h`

- `wubu_ct_cgroup_set_memory(const char *cgroup_path, uint64_t mem_mb)`
- `wubu_ct_cgroup_set_cpu(const char *cgroup_path, int cpu_count)`
- `wubu_ct_cgroup_set_pids(const char *cgroup_path, int max_pids)`
- `wubu_ct_cgroup_set_io_max(const char *cgroup_path, uint64_t read_bps, uint64_t write_bps)`
- `wubu_ct_cgroup_set_io_weight(const char *cgroup_path, uint32_t weight)`
- `wubu_ct_cgroup_attach(const char *cgroup_path, pid_t pid)`
- `wubu_ct_cgroup_destroy(const char *cgroup_path)`
- `wubu_ct_apply_seccomp(void *ct)`
- `wubu_ct_child_isolation(void)`
- `wubu_ct_setup_isolation(void *ct_ptr)`
- `wubu_ns_unshare(int flags)`
- `wubu_cgroup_write(const char *path, const char *value)`
- `wubu_seccomp_install(SeccompProfile profile)`

## `src/runtime/wubu_dos_emu.h`

- `wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size)`
- `wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size)`
- `wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps)`
- `wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size)`

## `src/runtime/wubu_dos_proc.h`

- `wubu_dos_proc_send_key(WubuDosProc *p, const char *key)`
- `wubu_dos_proc_write_ctl(WubuDosProc *p, const void *buf, size_t len)`
- `wubu_dos_proc_status_text(const WubuDosProc *p, char *out, size_t out_size)`

## `src/runtime/wubu_dxvk_conf.h`

- `dxvk_conf_read(const char *path, char *out, size_t size)`
- `dxvk_conf_set_key(char *buf, size_t bufsz, const char *key, const char *value)`
- `dxvk_conf_get_key(const char *buf, const char *key, char *out, size_t size)`
- `dxvk_conf_build_ui(const DxvkConfigUI *ui, char *buf, size_t bufsz)`

## `src/runtime/wubu_edr.h`

- `edr_analytics_set_enabled(bool on)`
- `edr_analytics_enabled(void)`
- `edr_recent_events(EdrEventView *out, int max, int min_type, int max_type)`

## `src/runtime/wubu_exec.h`

- `wubu_exec(const void *data, size_t size, const char *filename)`
- `wubu_exec_linux_elf(const void *elf_data, size_t elf_size)`
- `wubu_exec_win_pe(const void *pe_data, size_t pe_size)`
- `wubu_exec_holyc(const char *source, size_t source_size)`
- `wubu_exec_c(const char *source, size_t source_size)`
- `wubu_exec_shell(const char *script, size_t script_size)`
- `wubu_exec_python(const char *script, size_t script_size)`
- `wubu_exec_wasm(const void *wasm_data, size_t wasm_size)`
- `wubu_vsl_init(void)`
- `wubu_vsl_shutdown(void)`
- `wubu_vsl_active(void)`
- `wubu_vsl_run(const char *cmd)`

## `src/runtime/wubu_fs_util.h`

- `wubu_fs_rm_rf(const char *path)`

## `src/runtime/wubu_gc.h`

- `wubu_gc_free(void *ptr)`
- `wubu_gc_root_add(void *ptr)`
- `wubu_gc_collect(void)`

## `src/runtime/wubu_gdpr_age.h`

- `wubu_gdpr_age_verify(void)`
- `wubu_gdpr_age_check(void)`
- `wubu_gdpr_age_of_consent(void)`

## `src/runtime/wubu_holyc_agi.h`

- `wubu_holyc_agi_init(void)`
- `wubu_holyc_eval(const char *src, char *out, size_t out_size)`
- `wubu_holyc_agent_eval(const char *src, char *out, size_t out_size)`

## `src/runtime/wubu_holyd.h`

- `wubu_holyd_daemon_stop(WubuHoly *d)`
- `wubu_holyd_shutdown(WubuHoly *d)`
- `wubu_holyd_set_pointer_handler(wubu_holyd_pointer_fn fn)`

## `src/runtime/wubu_host_exec.h`

- `wubu_ct_destroy(WubuCt *ct)`
- `wubu_ct_start(WubuCt *ct)`
- `wubu_ct_wait(WubuCt *ct)`
- `wubu_ct_kill(WubuCt *ct, int sig)`
- `wubu_ct_state(WubuCt *ct)`

## `src/runtime/wubu_image.h`

- `wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size)`
- `wubu_manifest_from_json(const char *json, WubuImageManifest *manifest)`
- `wubu_manifest_save(const WubuImageManifest *manifest, const char *path)`
- `wubu_manifest_load(const char *path, WubuImageManifest *manifest)`

## `src/runtime/wubu_image_manifest.h`

- `wubu_manifest_compute_id(WubuImageManifest *manifest)`
- `wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size)`
- `wubu_manifest_from_json(const char *json, WubuImageManifest *manifest)`
- `wubu_manifest_save(const WubuImageManifest *manifest, const char *path)`
- `wubu_manifest_load(const char *path, WubuImageManifest *manifest)`

## `src/runtime/wubu_netlink.h`

- `netlink_addr_add(const char *iface, const char *ip_with_cidr)`

## `src/runtime/wubu_ns_bridge.h`

- `wubu_ns_bridge_create(const char *ns_root)`
- `wubu_ns_publish_bottle(const WubuBottle *b, const char *name)`
- `wubu_ns_bottle_action(WubuBottle *b, const char *action)`
- `wubu_ns_publish_snapshots(WubuSnapshotManager *mgr, const char *container_id)`

## `src/runtime/wubu_proton.h`

- `wubu_proton_validate_pe(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_parse_pe(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_map_sections(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_translate_api(wubu_proton_t *p, const char *win32_name)`
- `wubu_proton_load_default_dlls(wubu_proton_t *p)`

## `src/runtime/wubu_proton2.h`

- `wubu_proton_dump(const WubuProtonManager *mgr)`
- `wubu_proton_verify_installation(const WubuProtonManager *mgr)`

## `src/runtime/wubu_proton_dxvk.h`

- `wubu_proton_dxvk_set_resolver(wubu_proton_dxvk_path_resolver r)`

## `src/runtime/wubu_ramdisk.h`

- `wubu_rd_destroy(WubuRamdisk *rd)`
- `wubu_rd_boot(WubuRamdisk *rd)`
- `wubu_rd_state(WubuRamdisk *rd)`
- `wubu_rd_usage_mb(WubuRamdisk *rd)`
- `wubu_rd_mount(WubuRamdisk *rd)`
- `wubu_rd_load(WubuRamdisk *rd)`
- `wubu_rd_unmount(WubuRamdisk *rd)`
- `wubu_rd_bootstrap_disk(WubuRamdisk *rd, const char *mirror)`
- `wubu_rd_install_to_disk(WubuRamdisk *rd, const char *disk_path)`
- `wubu_rd_snapshot(WubuRamdisk *rd, const char *output_path)`
- `wubu_rd_install(WubuRamdisk *rd, const char *packages)`
- `wubu_rd_set_ram_size(WubuRamdisk *rd, const char *size_str)`

## `src/runtime/wubu_realm.h`

- `wubu_realm_set_kernel_schema(wubu_realm_t *r, int kernel_major)`
- `wubu_realm_kernel_schema_ok(const wubu_realm_t *r)`

## `src/runtime/wubu_selfimprove.h`

- `wubu_selfimprove_destroy(wubu_selfimprove_t *s)`
- `wubu_selfimprove_cycle(wubu_selfimprove_t *s)`
- `wubu_selfimprove_set_human_gate(wubu_selfimprove_t *s, bool require)`

## `src/runtime/wubu_spawn.h`

- `wubu_run_program(const char *file, char *const argv[], bool silent)`

## `src/runtime/wubu_system.h`

- `wubu_system_init(const char *store_path)`
- `wubu_system_commit(const char *label, char *out_id, int id_len)`
- `wubu_system_rollback(const char *snapshot_id)`
- `wubu_system_is_readonly(void)`
- `wubu_system_active_label(char *out_label, int label_len)`

## `src/runtime/wubu_trace.h`

- `wubu_trace_destroy(wubu_trace_store_t *t)`
- `wubu_trace_set_consent(wubu_trace_store_t *t, bool may_collect)`
- `wubu_trace_get_consent(const wubu_trace_store_t *t)`
- `wubu_trace_set_frozen(wubu_trace_store_t *t, bool frozen)`
- `wubu_trace_is_frozen(const wubu_trace_store_t *t)`
- `wubu_trace_mirror(const wubu_trace_store_t *t, const char *path)`

## `src/runtime/wubu_verifier_bytropix.h`

- `wubu_bytropix_verifier_destroy(wubu_bytropix_verifier_t *v)`
- `wubu_bytropix_score(const wubu_trace_span_t *span, void *ud, bool *passed)`

## `src/runtime/wubu_vsl.h`

- `vsl_init(void)`
- `vsl_shutdown(void)`
- `vsl_active(void)`
- `vsl_info(char *buf, size_t buf_size)`
- `vsl_dump_state(void)`

## `src/runtime/vsl/vsl_driver.h`

- `vsl_activate_driver(int drv_id)`
- `vsl_deactivate_driver(int drv_id)`

## `src/runtime/vsl/vsl_elf.h`

- `vsl_elf_load(const void *elf_data, size_t elf_size)`

## `src/runtime/vsl/vsl_file.h`

- `vsl_open(const char *path, int flags, int mode)`
- `vsl_close(int fd)`
- `vsl_read(int fd, void *buf, size_t count)`
- `vsl_write(int fd, const void *buf, size_t count)`
- `vsl_lseek(int fd, int64_t offset, int whence)`

## `src/runtime/vsl/vsl_gpu_vulkan.h`

- `vsl_vulkan_find_memory_type(VkMemoryPropertyFlags required_flags)`

## `src/runtime/vsl/vsl_memory.h`

- `vsl_munmap(uint64_t addr, size_t size)`
- `vsl_brk(uint64_t new_brk)`

## `src/runtime/vsl/vsl_nt_bridge.h`

- `vsl_nt_handle_to_data(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle, uint64_t *out_data)`
- `vsl_nt_allocate_handle(vsl_nt_bridge_ctx_t *ctx, int vsl_fd, uint64_t styx_fid, nt_object_type_t type)`
- `vsl_nt_free_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle)`
- `vsl_nt_allocate_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`
- `vsl_nt_free_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`

## `src/runtime/vsl/vsl_nt_internal.h`

- `vsl_nt_atoms_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_job_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_io_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_vmem_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_process_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_thread_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_section_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_timer_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_sync_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_registry_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_token_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_misc_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_misc_w11_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_alpc_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_wnf_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_worker_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_enclave_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_ioring_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_partition_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_ktm_register(vsl_syscall_fn_t *tbl, int size)`

## `src/runtime/vsl/vsl_process.h`

- `vsl_create_process(const void *elf_data, size_t elf_size)`
- `vsl_create_process_macho(const void *macho_data, size_t macho_size)`
- `vsl_create_process_any(const void *binary_data, size_t binary_size)`
- `vsl_destroy_process(uint32_t pid)`
- `vsl_list_processes(VSL_PROC *out, int max_count)`

## `src/runtime/vsl/vsl_shared.h`

- `vsl_send_cmd(uint64_t cmd, uint64_t arg)`

## `src/runtime/vsl/vsl_syscall.h`

- `vsl_syscall_dispatch(uint64_t num, uint64_t *regs)`

## `src/runtime/vsl/gpu/vulkan/wubu_vsl_vulkan.h`

- `wubu_vsl_vk_init(const WubuVkInstanceCreateInfo *create_info)`
- `wubu_vsl_vk_cleanup(void)`
- `wubu_vsl_vk_get_backend(void)`
- `wubu_vsl_vk_get_gpu_device_fd(VkPhysicalDevice phys_dev, int *fd)`
- `wubu_vsl_venus_get_context(VkInstance instance, void **venus_context)`
- `wubu_vsl_venus_set_frame_pacing(VkInstance instance, uint32_t max_frames)`
- `wubu_vsl_vk_check_extension(VkPhysicalDevice phys_dev, const char *ext_name)`
- `wubu_vsl_vk_set_debug_callback(WubuVkDebugCallback callback, void *user_data)`
- `wubu_vsl_vk_dump_capabilities(VkInstance instance)`

## `src/runtime/vsl/gpu/kern/wubu_vsl_gpu.h`

- `wubu_vsl_gpu_close(WubuVslGpuDevice *device)`
- `wubu_vsl_gpu_get_info(WubuVslGpuDevice *device, WubuGpuDeviceInfo *info)`
- `wubu_vsl_gpu_buffer_destroy(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_buffer_get_info(WubuVslGpuBuffer *buffer, WubuGpuBufferInfo *info)`
- `wubu_vsl_gpu_buffer_map(WubuVslGpuBuffer *buffer, void **ptr)`
- `wubu_vsl_gpu_buffer_unmap(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_buffer_export_dmabuf(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_context_destroy(WubuVslGpuContext *context)`
- `wubu_vsl_gpu_context_submit(WubuVslGpuContext *context, const WubuGpuSubmitInfo *submit)`
- `wubu_vsl_gpu_fence_destroy(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_wait(WubuVslGpuFence *fence, uint64_t timeout_ns)`
- `wubu_vsl_gpu_fence_reset(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_get_value(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_signal(WubuVslGpuFence *fence, uint64_t value)`
- `wubu_vsl_gpu_fence_export_sync_file(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_get_connectors(WubuVslGpuDevice *device, WubuGpuConnector **connectors)`
- `wubu_vsl_gpu_get_modes(WubuGpuConnector *connector, WubuGpuMode **modes)`
- `wubu_vsl_gpu_atomic_commit(WubuVslGpuDevice *device, const WubuGpuAtomicCommit *commit, bool test_only)`
- `wubu_vsl_gpu_dump_state(WubuVslGpuDevice *device)`

## `src/runtime/edr/edr_internal.h`

- `fnv1a(const char *key)`

## `src/runtime/wubu_manifest/wubu_manifest.h`

- `wubu_manifest_emit(const wubu_manifest_t *m, const char *out_dir)`

## `src/runtime/wubu_manifest/wubu_manifest_internal.h`

- `wubu_json_parse_manifest(const char *json, size_t len, wubu_manifest_t *m)`

## `src/runtime/container/wubucontainer.h`

- `wubu_container_init(WubuContainerEngine **engine_out, const char *container_dir)`

## `src/runtime/wubu_txn/wubu_txn.h`

- `wubu_txn_commit(WubuTxn *t)`

## `src/bridge/bridge.h`

- `bridge_enter_temple(void)`
- `bridge_exit_temple(void)`

## `src/bridge/vbe_ws_bridge.h`

- `vbe_ws_bridge_wire(vbe_ws_bridge_t *br, ws_simulation_t *sim)`
- `vbe_ws_bridge_frame(vbe_ws_bridge_t *br)`
- `vbe_ws_bridge_draw_hud(vbe_ws_bridge_t *br)`

## `src/gui/dosgui_daemon_panel.h`

- `dosgui_daemon_panel_holyd_state(void)`
- `dosgui_daemon_panel_container_count(void)`
- `dosgui_daemon_panel_holyd_session_count(void)`

## `src/gui/dosgui_dos_window.h`

- `dosgui_dos_window_close(DosGuiWindow *win)`

## `src/gui/dosgui_era_apps.h`

- `dosgui_era_apps_register(void)`
- `dosgui_era_apps_launch(int idx)`

## `src/gui/dosgui_explorer_internal.h`

- `str_contains_nocase(const char *haystack, const char *needle)`
- `ex_9p_stat(const char *path, struct stat *st)`
- `ex_strcasecmp(const char *a, const char *b)`
- `ex_sort_entries(ExExplorerState *ex)`
- `ex_file_compare(const void *a, const void *b)`
- `ex_populate_tree(ExTreeNode *node, const char *path)`
- `ex_tree_free(ExTreeNode *node)`

## `src/gui/dosgui_service_mgr.h`

- `dosgui_service_mgr_init(void)`
- `dosgui_service_mgr_register_autostart(const char *root, const char *svc)`
- `dosgui_service_mgr_boot(void)`

## `src/gui/dosgui_startmenu.h`

- `dosgui_startmenu_search_init(void)`
- `dosgui_startmenu_search_update(const char *query)`
- `dosgui_startmenu_search_clear(void)`
- `dosgui_startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count)`
- `dosgui_startmenu_recent_add(const char *app_name)`
- `dosgui_startmenu_recent_get(SmProgramEntry **out_entries, int max)`
- `dosgui_startmenu_tree_build(void)`
- `dosgui_startmenu_tree_toggle(SmTreeNode *node)`
- `dosgui_startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y)`
- `dosgui_startmenu_power(PowerAction action)`
- `dosgui_startmenu_render_search_bar(uint32_t *fb, int fb_w, int fb_h, int menu_x, int menu_y, int w)`
- `dosgui_startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items)`
- `dosgui_startmenu_render_all_programs(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h)`
- `dosgui_startmenu_render_power_options(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w)`
- `dosgui_startmenu_handle_key(int key, uint32_t mods)`
- `dosgui_startmenu_handle_search_input(int key, uint32_t mods)`
- `dosgui_startmenu_build_programs_db(void)`
- `dosgui_startmenu_init_enhanced(void)`

## `src/gui/dosgui_startmenu_internal.h`

- `dosgui_startmenu_build_main_menu(void)`
- `dosgui_startmenu_build_submenus(void)`

## `src/gui/dosgui_term_internal.h`

- `term_screen_bind_pty(TermScreen *out, TermPtySession *pty)`
- `term_ansi_parse(TermScreen *scr, const char *buf, int n)`
- `term_ansi_drain_fd(int fd, TermScreen *scr)`

## `src/gui/dosgui_term_pty.h`

- `term_handle_key_container(TermState *term, uint32_t key, uint32_t mods)`

## `src/gui/dosgui_wm.h`

- `dosgui_wm_get_mouse(int *x, int *y)`
- `dosgui_systray_remove(const char *name)`
- `dosgui_systray_set_notification_count(const char *name, int count)`
- `dosgui_notif_center_mark_read(uint32_t id)`
- `dosgui_notif_center_clear(void)`
- `dosgui_notif_center_render(uint32_t *fb, int fb_w, int fb_h)`
- `dosgui_notif_center_is_open(void)`
- `dosgui_notif_center_toggle(void)`
- `dosgui_wm_restore_icon_layout(void)`
- `dosgui_wm_reload_wallpaper(void)`
- `dosgui_wm_is_initialized(void)`
- `dosgui_wm_wallpaper_mode(void)`
- `dosgui_wm_wallpaper_w(void)`
- `dosgui_wm_wallpaper_h(void)`
- `dosgui_wm_set_auto_arrange(bool on)`
- `dosgui_wm_get_auto_arrange(void)`
- `dosgui_wm_set_icons_visible(bool show)`
- `dosgui_wm_dirty_count(void)`
- `dosgui_chrome_hit_test_button(DosGuiWindow *win, int mx, int my)`

## `src/gui/dosgui_wm_holyc_term.h`

- `holyc_term_set_eval(holyc_term_eval_fn fn)`

## `src/gui/dosgui_wm_internal.h`

- `dosgui_wm_draw_icon_selection(int ox, int oy)`
- `snap_window_to_gaad(DosGuiWindow *w)`
- `dosgui_icon_hit_test(int mx, int my)`
- `spawn_window(int x, int y, int w, int h, const char *title)`

## `src/gui/gui_dbuf.h`

- `gui_dbuf_flip(gui_dbuf_t *db)`

## `src/gui/wubu_compositor.h`

- `wubu_window_close(WuBuWindow *win)`
- `wubu_compositor_gpu_init(WuBuCompositor *comp)`
- `wubu_compositor_gpu_fini(WuBuCompositor *comp)`
- `wubu_compositor_a11y_tree_get_root(WuBuCompositor *comp, WuBuA11yNode *root)`
- `wubu_compositor_a11y_node_get_children(WuBuA11yNode *parent, WuBuA11yNode *children, int max)`
- `wubu_compositor_a11y_announce(WuBuCompositor *comp, const char *message)`

## `src/gui/wubu_gamelib.h`

- `wubu_gamelib_clear_start_menu(void)`

## `src/gui/wubu_gamelib_internal.h`

- `gamelib_ensure_dir(const char *path)`

## `src/gui/wubu_pkgmgr.h`

- `wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t* out)`

## `src/gui/wubu_screenshot.h`

- `png_encode_rgba(const uint32_t *px, int w, int h, uint8_t **out)`
- `wubu_screenshot_handle_alt_printscr(void)`
- `wubu_screenshot_handle_shift_printscr(void)`

## `src/gui/wubu_session.h`

- `wubu_session_restore(void)`

## `src/gui/wubu_settings.h`

- `wubu_settings_get_cursor_size(void)`
- `wubu_settings_high_contrast(void)`
- `wubu_settings_reduce_motion(void)`
- `wubu_settings_font_size(void)`

## `src/gui/wubu_wallpaper.h`

- `wubu_wallpaper_load(const char *path, WubuWallpaper *out)`

## `src/gui/wubu_welcome.h`

- `wubu_welcome_init(void)`
- `wubu_welcome_is_dismissed(void)`
- `wubu_welcome_dismiss(void)`

## `src/gui/wubu_wm.h`

- `wubu_wm_desktop_next(void)`
- `wubu_wm_desktop_prev(void)`
- `wubu_wm_desktop_current(void)`
- `wubu_wm_desktop_count(void)`
- `wubu_wm_desktop_set_count(int count)`
- `wubu_wm_desktop_move_win(WubuWin *win, int desktop)`

## `src/compiler/holyc_codegen.h`

- `hc_register_holyc_runtime(HCGen *gen)`

## `src/compiler/holyc_codegen_internal.h`

- `emit_global_load_rax(HCGen *gen, size_t global_offset)`

## `src/compiler/holyc_lexer.h`

- `hc_lex_init(HCLexer *lex, const char *source)`
- `hc_lex_next(HCLexer *lex)`
- `hc_lex_peek(HCLexer *lex)`
- `hc_lex_expect(HCLexer *lex, HCTokenType expected)`

## `src/compiler/holyc_parser.h`

- `hc_parse_init(HCParser *p, HCLexer *lex)`
- `hc_parse_peek(HCParser *p)`

## `src/shell/wubu_shell.h`

- `wubu_shell_run(void *arg)`

## `src/shell/wubu_shell_internal.h`

- `shell_print(ShellState *st, const char *msg)`
- `shell_history_count(const ShellState *st)`
- `shell_exec_pipeline(ShellState *st, const char *line)`

## `src/framework/wubufx.h`

- `wubufx_mount(const char *id, WubuFxCap caps, WubuFxApp **out_app)`
- `wubufx_close(WubuFxApp *app)`
- `wubufx_unmount(WubuFxApp *app)`
- `wubufx_node_close(WubuFxNode *node)`
- `wubufx_state_get(WubuFxNode *node, char *out, size_t out_size)`
- `wubufx_state_set(WubuFxNode *node, const char *value)`

## `src/bear/bear_cuda.h`

- `bear_cuda_profile_enable(BearCudaContext* ctx, int enable)`
- `bear_cuda_profile_get_events(BearCudaContext* ctx, BearCudaProfileEvent* out, int max_events)`
- `bear_cuda_profile_reset(BearCudaContext* ctx)`

## `src/bear/bear_cudnn.h`

- `hc_builtin_cublas_create(void)`
- `hc_builtin_cublas_destroy(BearCublasHandle handle)`
- `hc_builtin_cudnn_create(void)`
- `hc_builtin_cudnn_destroy(BearCudnnHandle handle)`
- `hc_builtin_cudnn_destroy_tensor_desc(BearCudnnTensorDesc desc)`
- `hc_builtin_cuda_malloc(size_t bytes)`
- `hc_builtin_cuda_free(void* ptr)`
- `hc_builtin_cublas_get_error_string(int status)`
- `hc_builtin_cudnn_get_error_string(int status)`

## `src/bear/bear_holo_opt.h`

- `bear_holo_step(BearHoloOptimizer* opt, double* params, const double* grads, int n)`

## `src/bear/bear_mujoco.h`

- `bear_mujoco_init(int num_poles, int num_envs)`

## `src/bear/bear_nn.h`

- `bear_orthogonal_init_params(BearPolicyNet* net, float gain)`
- `bear_value_orthogonal_init(BearValueNet* vnet, float gain)`
- `bear_checkpoint_save(const BearPolicyNet* net, const char* path)`
- `bear_checkpoint_load(BearPolicyNet* net, const char* path)`

## `src/bear/bear_ppo.h`

- `bear_ppo_clip_grad_norm(BearPolicyNet* policy, BearValueNet* critic, float max_norm)`
- `bear_trainer_set_logger(BearTrainer* trainer, bear_log_fn fn, void* user_data)`

## `src/bear/bear_simd.h`

- `tanhf(x)`

## `src/bear/bear_vulkan.h`

- `bear_vulkan_profile_enable(BearVulkanContext* ctx, int enable)`
- `bear_vulkan_profile_get_events(BearVulkanContext* ctx, BearVulkanProfileEvent* out, int max_events)`
- `bear_vulkan_profile_reset(BearVulkanContext* ctx)`

## `src/bear/wubu_warehouse.h`

- `warehouse_build_mjcf(const WarehouseConfig* cfg)`

## `src/firmware/fw.h`

- `fw_media_count(void)`
- `fw_pe_load(const void *file, uint64_t file_size, fw_pe_image *out)`
- `fw_efi_new_handle(void)`
- `fw_efi_install(EFI_HANDLE h, EFI_GUID *guid, void *iface)`
- `fw_efi_boot_services_active(void)`

## `src/firmware/fw_acpi.h`

- `fw_acpi_init(void)`
- `fw_acpi_set_rsdp(void *rsdp)`
- `fw_acpi_tpm2_control_area(void)`
- `fw_acpi_tpm2_start_method(void)`
- `fw_acpi_tpm_log_addr(void)`
- `fw_acpi_tpm_log_size(void)`

## `src/firmware/fw_agi.h`

- `fw_agi_publish_attest(void)`
- `fw_agi_attest_and_boot(const char *path)`

## `src/firmware/fw_block.h`

- `fw_block_count(void)`
- `fw_block_read(int i, uint64_t lba, uint32_t count, void *buf)`
- `fw_block_write(int i, uint64_t lba, uint32_t count, const void *buf)`

## `src/firmware/fw_fwcfg.h`

- `fw_cfg_init(void)`
- `fw_cfg_present(void)`
- `fw_cfg_read_file(const char *name, void *buf, uint32_t max, uint32_t *out_len)`
- `fw_cfg_file_size(const char *name)`

## `src/firmware/fw_secureboot.h`

- `fw_sb_enroll_db(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_enroll_dbx(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_set_pk(void)`
- `fw_sb_secureboot_enabled(void)`
- `fw_sb_selftest(void)`
- `fw_sb_selftest_pe(void)`
- `fw_sb_setup_mode(void)`
- `fw_sb_verify(const uint8_t *image, uint32_t size, uint8_t *hash)`

## `src/firmware/fw_sha256.h`

- `sha256_init(sha256_ctx *c)`
- `sha256_update(sha256_ctx *c, const void *data, uint64_t len)`
- `sha256_final(sha256_ctx *c, uint8_t out[32])`
- `sha256(const void *data, uint64_t len, uint8_t out[32])`

## `src/firmware/fw_tpm.h`

- `fw_tpm_init(void)`
- `fw_tpm_present(void)`
- `fw_tpm_interface(void)`

## `src/firmware/loader/sha256.h`

- `sha256_init(sha256_ctx *c)`
- `sha256_update(sha256_ctx *c, const void *data, uint64_t len)`
- `sha256_final(sha256_ctx *c, uint8_t out[32])`
- `sha256(const void *data, uint64_t len, uint8_t out[32])`

## `src/hosted/hosted.h`

- `hosted_init(hosted_state_t *state, int argc, char **argv)`
- `hosted_run(hosted_state_t *state)`
- `hosted_shutdown(hosted_state_t *state)`
- `hosted_set_mode(hosted_state_t *state, hosted_mode_t mode)`

## `src/hosted/hosted_internal.h`

- `shm_buffer_create(shm_buffer_t *buf, int w, int h)`
- `shm_buffer_destroy(shm_buffer_t *buf)`
- `hosted_render_desktop(hosted_state_t *state)`
- `hosted_input_dispatch(void)`
- `hosted_pe_executor(const void *data, size_t size, const char *cmdline)`
- `hosted_run(hosted_state_t *state)`
- `hosted_shutdown(hosted_state_t *state)`
- `hosted_blit(hosted_state_t *state)`
- `hosted_set_mode(hosted_state_t *state, hosted_mode_t mode)`
- `hosted_kernel_ready(void)`
- `hosted_wm_has_windows(void)`

## `src/hosted/wubu_display.h`

- `wubu_gbm_destroy_device(wubu_gbm_device_t *gbm)`
- `wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo)`
- `wubu_gbm_bo_get_stride(wubu_gbm_bo_t *bo)`
- `wubu_gbm_bo_get_handle(wubu_gbm_bo_t *bo)`

## `src/hosted/wubu_metal_audio.h`

- `wubu_alsa_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_alsa_shutdown(void)`
- `wubu_alsa_submit(const float *buf, int frames)`
- `wubu_alsa_cpu_load(void)`
- `wubu_pulse_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pulse_shutdown(void)`
- `wubu_pulse_submit(const float *buf, int frames)`
- `wubu_pulse_cpu_load(void)`
- `wubu_pipewire_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pipewire_shutdown(void)`
- `wubu_pipewire_submit(const float *buf, int frames)`
- `wubu_pipewire_cpu_load(void)`

## `src/hosted/wubu_metal_drm.h`

- `wubu_drm_init(int width, int height)`
- `wubu_drm_shutdown(void)`
- `wubu_drm_flip(void)`
- `wubu_drm_set_mode(int width, int height, int refresh_hz)`
- `wubu_drm_get_modes(int *widths, int *heights, int max)`

## `src/hosted/wubu_metal_internal.h`

- `wubu_evdev_find_device(const char *type, int *out_fd)`
- `wubu_evdev_init_all(void)`
- `wubu_evdev_shutdown(void)`
- `wubu_evdev_poll(void)`
- `wubu_evdev_key_down(uint32_t key)`
- `wubu_evdev_mouse_pos(int *x, int *y)`
- `wubu_alsa_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_alsa_shutdown(void)`
- `wubu_alsa_submit(const float *buf, int frames)`
- `wubu_alsa_cpu_load(void)`
- `wubu_pulse_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pulse_shutdown(void)`
- `wubu_pulse_submit(const float *buf, int frames)`
- `wubu_pulse_cpu_load(void)`
- `wubu_pipewire_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pipewire_shutdown(void)`
- `wubu_pipewire_submit(const float *buf, int frames)`
- `wubu_pipewire_cpu_load(void)`
- `wubu_wsl2_disp_init(void)`
- `wubu_wsl2_audio_init(void)`
- `wubu_x11_init(int width, int height)`
- `wubu_x11_shutdown(void)`
- `wubu_x11_flip(void)`
- `wubu_x11_set_mode(int width, int height, int refresh_hz)`
- `vbe_init_fb(int width, int height)`
- `vbe_shutdown_fb(void)`
- `wubu_disp_init(int width, int height)`
- `wubu_disp_shutdown(void)`
- `wubu_disp_set_mode(int width, int height, int refresh_hz)`
- `wubu_disp_flip(void)`
- `wubu_disp_poll_events(void)`
- `wubu_disp_current(void)`
- `wubu_disp_force(WubuDispBackend backend)`
- `wubu_disp_get_modes(int *widths, int *heights, int max)`
- `wubu_disp_gaad_nearest(int w, int h, int *out_w, int *out_h)`
- `wubu_input_init(void)`
- `wubu_input_shutdown(void)`
- `wubu_input_poll(void)`
- `wubu_input_key_down(uint32_t key)`
- `wubu_input_mouse_pos(int *x, int *y)`

## `src/apps/calc.h`

- `calc_open(void)`
- `calc_init(void)`
- `calc_shutdown(void)`

## `src/apps/explorer.h`

- `explorer_open(void)`
- `explorer_init(void)`
- `explorer_shutdown(void)`

## `src/apps/repl.h`

- `repl_start(int fb_w, int fb_h)`

## `src/apps/terminal.h`

- `terminal_open(void)`
- `terminal_init(void)`
- `terminal_shutdown(void)`
- `terminal_poll(void)`

## `src/apps/wubu_canvas.h`

- `wubu_cv_layer_set_opacity(WubuCanvas *cv, int idx, uint8_t opacity)`
- `wubu_cv_layer_set_blend(WubuCanvas *cv, int idx, WubuBlendMode blend)`
- `wubu_cv_layer_set_visible(WubuCanvas *cv, int idx, bool visible)`
- `wubu_cv_layer_set_locked(WubuCanvas *cv, int idx, bool locked)`
- `wubu_cv_load_png(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_gif(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_bmp(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_ppm(WubuCanvas *cv, const char *path)`
- `wubu_cv_zoom_out(WubuCanvas *cv)`
- `wubu_cv_zoom_fit(WubuCanvas *cv)`
- `wubu_cv_pan(WubuCanvas *cv, int dx, int dy)`

## `src/apps/wubu_canvas_internal.h`

- `wubu_cv__undo_push(WubuCanvas *cv)`

## `src/apps/wubu_editor.h`

- `wubu_ed_delete_forward(WubuEditor *ed)`
- `wubu_ed_insert_text(WubuEditor *ed, const char *text)`

## `src/apps/wubu_image_codec_internal.h`

- `png_unfilter(uint8_t *raw, int w, int h, int channels)`

## `src/apps/calc/calc_internal.h`

- `calc_apply_op(int op, double a, double b, bool *err)`
- `calc_apply_func(int func, double x, bool *err)`

## `src/apps/repl/repl.h`

- `repl_create(void)`
- `repl_destroy(REPLState *repl)`
- `repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, REPLState *repl)`
- `repl_launch(void)`
- `repl_add_line(REPLState *repl, const char *line)`
- `repl_input_char(REPLState *repl, char c)`
- `repl_input_backspace(REPLState *repl)`
- `repl_submit_line(REPLState *repl)`

## `src/apps/editor/editor.h`

- `editor_create(void)`
- `editor_destroy(EditorState *ed)`
- `editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, EditorState *ed)`
- `editor_launch(void)`

## `src/apps/control/control.h`

- `control_create(void)`
- `control_destroy(ControlState *ctrl)`
- `control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl)`
- `control_launch(void)`
- `control_set_tab(ControlState *ctrl, int tab)`
- `control_set_theme(int theme_id)`

