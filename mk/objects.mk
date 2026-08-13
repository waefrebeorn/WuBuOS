# ── Object Lists ────────────────────────────────────────────────

# JIT source files (always linked together — self-hosted encoder/disasm/regalloc/minic)
# -ldl is needed for dlopen/dlsym in the MIR backend
JIT_SRCS = $(JIT)/jit.c $(JIT)/jit_encode.c $(JIT)/wubu_x86.c $(JIT)/wubu_disasm.c $(JIT)/x86_regalloc.c $(JIT)/jit_minic.c $(JIT)/jit_minic_token.c -ldl

WUBU_DOS_EMU_OBJS = $(RT)/wubu_dos_emu.o $(RT)/wubu_dos_emu_mem.o $(RT)/wubu_dos_emu_regs.o $(RT)/wubu_dos_emu_alu.o $(RT)/wubu_dos_emu_int.o $(RT)/wubu_dos_emu_decode.o

# ── Kernel Objects ───────────────────────────────────────────────
# (one per line for readability + diffability)
# interrupt.c is split into 3 focused modules:
#   interrupt.o              — core IDT setup + dispatch
#   interrupt_exceptions.o   — exception handlers (double fault, NMI, page fault, GPF)
#   interrupt_syscall.o      — syscall registration + management table
KERNEL_OBJS = $(KERNEL)/memory.o $(KERNEL)/tasking.o $(KERNEL)/vbe.o \
              $(KERNEL)/input.o $(KERNEL)/interrupt.o $(KERNEL)/interrupt_pic.o \
              $(KERNEL)/interrupt_apic.o $(KERNEL)/interrupt_pit.o \
              $(KERNEL)/interrupt_syscall.o $(KERNEL)/interrupt_timer.o \
              $(KERNEL)/interrupt_exceptions.o \
              $(KERNEL)/isr_stubs.o $(KERNEL)/fat32.o $(KERNEL)/fat32_fat.o \
              $(KERNEL)/fat32_dir.o $(KERNEL)/fat32_file.o \
              $(KERNEL)/fat32_format.o $(KERNEL)/fat32_name.o \
              $(KERNEL)/fat32_cluster.o $(KERNEL)/wubu_lfn.o \
              $(KERNEL)/ahci.o $(KERNEL)/txfs.o $(KERNEL)/wubu_gaad.o \
              $(KERNEL)/wubu_agi_kernel.o $(KERNEL)/wubu_attest.o \
              $(KERNEL)/wubu_self_test.o \
              $(KERNEL)/wubu_hive.o \
              $(KERNEL)/wubu_bonzi.o $(KERNEL)/wubu_apic.o \
              $(KERNEL)/wubu_pci.o $(KERNEL)/wubu_console.o \
              $(KERNEL)/wubu_console_cmds.o \
              $(KERNEL)/wubu_drv.o $(KERNEL)/wubu_drv_nvme.o \
              $(KERNEL)/wubu_drv_net.o $(KERNEL)/wubu_drv_hda.o \
              $(KERNEL)/wubu_drv_gpu.o $(KERNEL)/wubu_drv_battery.o \
              $(KERNEL)/wubu_drv_ahci.o \
              $(KERNEL)/wubu_drv_sd.o $(KERNEL)/wubu_drv_usb.o \
              $(KERNEL)/wubu_drv_thermal.o $(KERNEL)/wubu_world.o \
              $(KERNEL)/wubu_kvfs.o \
              $(KERNEL)/wubu_drv_virtio.o $(KERNEL)/wubu_drv_arm.o \
              $(KERNEL)/wubu_drv_intel.o \
              $(KERNEL)/wubu_console_colonel.o $(KERNEL)/wubu_console_recovery.o \
              $(KERNEL)/wubu_theme.o $(KERNEL)/wubu_hid.o \
              $(KERNEL)/wubu_verifier.o $(KERNEL)/wubu_tss.o \
              $(KERNEL)/wubu_sync.o $(KERNEL)/wubu_vmm.o \
              $(KERNEL)/wubu_memmap.o $(KERNEL)/wubu_serial.o \
              $(KERNEL)/wubu_sha256.o $(KERNEL)/wubu_rtc.o \
              $(KERNEL)/wubu_acpi.o $(KERNEL)/wubu_wdt.o \
              $(KERNEL)/wubu_hpet.o $(KERNEL)/wubu_crash.o \
              $(KERNEL)/wubu_smbios.o $(KERNEL)/wubu_vdso.o $(KERNEL)/wubu_swap.o \
              $(KERNEL)/wubu_as.o \
              $(KERNEL)/wubu_user.o $(KERNEL)/wubu_iommu.o \
              $(KERNEL)/wubu_smp.o $(KERNEL)/wubu_smp_tramp.o \
              $(KERNEL)/wubu_xhci.o \
              $(KERNEL)/wubu_recovery.o \
              $(KERNEL)/wubu_psych.o $(KERNEL)/wubu_bonzi_study.o $(KERNEL)/wubu_tutor.o \
              $(KERNEL)/tasking_switch.o $(KERNEL)/ps2.o \
              $(KERNEL)/wubu_math.o $(KERNEL)/libc.o $(KERNEL)/libc_string.o $(KERNEL)/klog.o $(KERNEL)/wubu_pe.o $(KERNEL)/wubu_pe_personality.o $(KERNEL)/wubu_elf.o $(KERNEL)/wubu_macho.o \
              $(KERNEL)/wubu_lzx.o $(KERNEL)/wubu_inflate.o $(KERNEL)/wubu_zip.o $(KERNEL)/wubu_cab.o \
              $(KERNEL)/wubu_hw_detect.o \
              $(KERNEL)/wubu_gpu_icd.o \
              $(KERNEL)/wubu_probe.o \
              $(KERNEL)/wubu_probe_matrix.o $(KERNEL)/wubu_audio.o \
              $(KERNEL)/wubu_storage.o $(KERNEL)/wubu_net.o \
              $(KERNEL)/wubu_input.o $(KERNEL)/wubu_display.o \
              $(KERNEL)/wubu_usbf.o \
              $(KERNEL)/wubu_power.o \
              $(KERNEL)/wubu_peripheral.o \
              $(KERNEL)/wubu_virt.o \
              $(KERNEL)/wubu_sensor.o \
              $(KERNEL)/wubu_can.o \
              $(KERNEL)/wubu_mem.o \
              $(KERNEL)/wubu_accel.o \
              $(KERNEL)/wubu_camera.o \
              $(KERNEL)/wubu_bt.o \
              $(KERNEL)/wubu_codec.o \
              $(KERNEL)/wubu_raid.o \
              $(KERNEL)/wubu_fingerprint.o \
              $(KERNEL)/wubu_fpga.o \
              $(KERNEL)/wubu_wifi7.o \
              $(KERNEL)/wubu_pmicaudio.o \
              $(KERNEL)/wubu_switchdev.o \
              $(KERNEL)/wubu_securekey.o \
              $(KERNEL)/wubu_panel.o \
              $(KERNEL)/wubu_phy.o \
              $(KERNEL)/wubu_bus.o \
              $(KERNEL)/wubu_clock.o \
              $(KERNEL)/wubu_video.o \
              $(KERNEL)/wubu_nicoffload.o \
              $(KERNEL)/wubu_pm.o \
              $(KERNEL)/wubu_usb4.o \
              $(KERNEL)/wubu_compute.o \
              $(KERNEL)/wubu_vlanaudio.o \
              $(KERNEL)/wubu_sata.o \
              $(KERNEL)/wubu_drmx.o \
              $(KERNEL)/wubu_ptp.o \
              $(KERNEL)/wubu_tpm.o \
              $(KERNEL)/wubu_touch.o \
              $(KERNEL)/wubu_psr.o \
              $(KERNEL)/wubu_dspmode.o \
              $(KERNEL)/wubu_multigig.o \
              $(KERNEL)/wubu_gamepad.o \
              $(KERNEL)/wubu_rdma.o \
              $(KERNEL)/wubu_zoned.o \
              $(KERNEL)/wubu_vrr.o \
              $(KERNEL)/wubu_qos.o \
              $(KERNEL)/wubu_hidadv.o \
              $(KERNEL)/wubu_backlight.o \
              $(KERNEL)/wubu_mixgraph.o \
              $(KERNEL)/wubu_raidcache.o \
              $(KERNEL)/wubu_pd.o \
              $(KERNEL)/wubu_calib.o \
              $(KERNEL)/wubu_eq.o \
              $(KERNEL)/wubu_gadget.o \
              $(KERNEL)/wubu_ucode.o \
              $(KERNEL)/wubu_ptp_sync.o \
              $(KERNEL)/wubu_hdr.o \
              $(KERNEL)/wubu_wifi_reg.o \
              $(KERNEL)/wubu_trim.o \
              $(KERNEL)/wubu_mst.o \
              $(KERNEL)/wubu_thermal.o \
              $(KERNEL)/wubu_ns.o \
              $(KERNEL)/wubu_fc.o \
              $(KERNEL)/wubu_gpusensor.o \
              $(KERNEL)/wubu_fw.o \
              $(KERNEL)/wubu_ima.o \
              $(KERNEL)/wubu_colormgmt.o \
              $(KERNEL)/wubu_loudness.o \
              $(KERNEL)/wubu_gpusched.o \
              $(KERNEL)/wubu_porttiming.o \
              $(KERNEL)/wubu_codecgraph.o \
              $(KERNEL)/wubu_flush.o \
              $(KERNEL)/wubu_perf.o \
              $(KERNEL)/wubu_pcmring.o \
              $(KERNEL)/wubu_bcache.o \
              $(KERNEL)/wubu_fantml.o \
              $(KERNEL)/wubu_pcmlink.o \
              $(KERNEL)/wubu_lvm.o \
              $(KERNEL)/wubu_vram.o \
              $(KERNEL)/wubu_spdif.o \
              $(KERNEL)/wubu_cmb.o \
              $(KERNEL)/wubu_backlightpwm.o \
              $(KERNEL)/wubu_aec.o \
              $(KERNEL)/wubu_dedup.o \
              $(KERNEL)/wubu_gpuband.o \
              $(KERNEL)/wubu_compress.o \
              $(KERNEL)/wubu_filter.o \
              $(KERNEL)/wubu_ducking.o \
              $(KERNEL)/wubu_pdpolicy.o \
              $(KERNEL)/wubu_gpurst.o \
              $(KERNEL)/wubu_iosched.o \
              $(KERNEL)/wubu_wifiutil.o \
              $(KERNEL)/wubu_ddcci.o \
              $(KERNEL)/wubu_samplerate.o \
              $(KERNEL)/wubu_smart.o \
              $(KERNEL)/wubu_overclock.o \
              $(KERNEL)/wubu_dspgraph.o \
              $(KERNEL)/wubu_smr.o \
              $(KERNEL)/wubu_computectx.o \
              $(KERNEL)/wubu_chanmap.o \
              $(KERNEL)/wubu_dmcrypt.o \
              $(KERNEL)/wubu_voltagectl.o \
              $(KERNEL)/wubu_dapm.o \
              $(KERNEL)/wubu_mdraid.o \
              $(KERNEL)/wubu_gpucsched.o \
              $(KERNEL)/wubu_dsptrace.o \
              $(KERNEL)/wubu_nvmepower.o \
              $(KERNEL)/wubu_gpufwupd.o \
              $(KERNEL)/wubu_bthfp.o \
              $(KERNEL)/wubu_zoneappend.o \
              $(KERNEL)/wubu_zonefmt.o \
              $(KERNEL)/wubu_gpushader.o \
              $(KERNEL)/wubu_bta2dp.o \
              $(KERNEL)/wubu_gpufw.o \
              $(KERNEL)/wubu_btaudio.o \
              $(KERNEL)/wubu_gpumem.o \
              $(KERNEL)/wubu_btclassic.o \
              $(KERNEL)/wubu_zonecap.o \
              $(KERNEL)/wubu_vpudecode.o \
              $(KERNEL)/wubu_btamesh.o \
              $(KERNEL)/wubu_zonseqwrite.o \
              $(KERNEL)/wubu_vpuencode.o \
              $(KERNEL)/wubu_leaudio.o \
              $(KERNEL)/wubu_nvmehotplug.o \
              $(KERNEL)/wubu_gpudc.o \
              $(KERNEL)/wubu_btbeacon.o \
              $(KERNEL)/wubu_gamepaddz.o \
              $(KERNEL)/wubu_rendernode.o \
              $(KERNEL)/wubu_auracast.o \
              $(KERNEL)/wubu_nvme_gen5.o \
              $(KERNEL)/wubu_intelgpu.o \
              $(KERNEL)/wubu_bap.o \
              $(KERNEL)/wubu_nvme_gen4.o \
              $(KERNEL)/wubu_radeon_legacy.o \
              $(KERNEL)/wubu_radeon_6000.o \
              $(KERNEL)/wubu_radeon_5000.o \
              $(KERNEL)/wubu_intel_gma.o \
              $(KERNEL)/wubu_adreno700.o \
              $(KERNEL)/wubu_mali_g52.o \
              $(KERNEL)/wubu_mali_g720.o \
              $(KERNEL)/wubu_adreno600.o \
              $(KERNEL)/wubu_mali_g77.o \
              $(KERNEL)/wubu_vc4.o \
              $(KERNEL)/wubu_vc6.o \
              $(KERNEL)/wubu_powervr.o \
              $(KERNEL)/wubu_xe3.o \
              $(KERNEL)/wubu_nvidia_fermi.o \
              $(KERNEL)/wubu_nvidia_kepler.o \
              $(KERNEL)/wubu_nvidia_maxwell.o \
              $(KERNEL)/wubu_nvidia_turing.o \
              $(KERNEL)/wubu_intel_icelake.o \
              $(KERNEL)/wubu_vega.o \
              $(KERNEL)/wubu_quadro.o \
              $(KERNEL)/wubu_cuda.o \
              $(KERNEL)/wubu_vulkan14.o \
              $(KERNEL)/wubu_instinct.o \
              $(KERNEL)/wubu_opencl.o \
              $(KERNEL)/wubu_gt2xx.o \
              $(KERNEL)/wubu_ampere.o \
              $(KERNEL)/wubu_renoir.o \
              $(KERNEL)/wubu_arctic_islands.o \
              $(KERNEL)/wubu_volcanic_islands.o \
              $(KERNEL)/wubu_intel_skylake.o \
              $(KERNEL)/wubu_navi10.o \
              $(KERNEL)/wubu_nvidia_volta.o \
              $(KERNEL)/wubu_nvidia_pascal.o \
              $(KERNEL)/wubu_gpukms.o \
              $(KERNEL)/wubu_gamepadbm.o \
              $(KERNEL)/wubu_leaudioldr.o \
              $(KERNEL)/wubu_spdiftx.o \
              $(KERNEL)/wubu_blkqos.o \
              $(KERNEL)/wubu_memmgr.o \
              $(KERNEL)/wubu_jackdetect.o \
              $(KERNEL)/wubu_ioprio.o \
              $(KERNEL)/wubu_perfmon.o \
              $(KERNEL)/wubu_pcmplugin.o \
              $(KERNEL)/wubu_dax.o \
              $(KERNEL)/wubu_fbcon.o \
              $(KERNEL)/wubu_jackstate.o \
              $(KERNEL)/wubu_storagesched.o \
              $(KERNEL)/wubu_fencesync.o \
              $(KERNEL)/wubu_jackimpedance.o \
              $(KERNEL)/wubu_writeback.o \
              $(KERNEL)/wubu_thermalthrottle.o \
              $(KERNEL)/wubu_compressor.o \
              $(KERNEL)/wubu_raid5.o \
              $(KERNEL)/wubu_smc.o \
              $(KERNEL)/wubu_ieccontrol.o \
              $(KERNEL)/wubu_flush2.o \
              $(KERNEL)/wubu_mmu.o \
              $(KERNEL)/wubu_dappath.o \
              $(KERNEL)/wubu_bio.o \
              $(KERNEL)/wubu_encode.o \
              $(KERNEL)/wubu_spdifstatus.o \
              $(KERNEL)/wubu_devmapper.o \
              $(KERNEL)/wubu_decode.o \
              $(KERNEL)/wubu_audiofw.o \
              $(KERNEL)/wubu_nfsmount.o \
              $(KERNEL)/wubu_drm.o \
              $(KERNEL)/wubu_mixergraph.o \
              $(KERNEL)/wubu_nfsclient.o \
              $(KERNEL)/wubu_vblank.o \
              $(KERNEL)/wubu_spdifrx.o \
              $(KERNEL)/wubu_fusefs.o \
              $(KERNEL)/wubu_jack.o \
              $(KERNEL)/wubu_uas.o \
              $(KERNEL)/wubu_powergate.o \
              $(KERNEL)/wubu_dapmwidget.o \
              $(KERNEL)/wubu_znszone.o \
              $(KERNEL)/wubu_uac.o \
              $(KERNEL)/wubu_pcipme.o \
              $(KERNEL)/wubu_fence.o \

# ── Metal Objects ────────────────────────────────────────────────
METAL_OBJS = $(HOSTED)/wubu_metal.o $(HOSTED)/wubu_metal_evdev.o $(HOSTED)/wubu_metal_x11.o $(HOSTED)/wubu_metal_vulkan.o $(HOSTED)/wubu_metal_drm.o

# ── Hosted Objects ───────────────────────────────────────────────
HOSTED_OBJS_LIST = $(HOSTED)/wubu_gbm.o $(HOSTED)/wubu_vulkan_loader.o $(HOSTED)/wubu_vulkan_swapchain.o $(HOSTED)/wubu_vulkan_cmd.o $(HOSTED)/wubu_vulkan_compute.o $(HOSTED)/wubu_vulkan_util.o $(HOSTED)/wubu_metal_audio.o

# ── JIT Objects ──────────────────────────────────────────────────
JIT_OBJS = $(JIT)/jit.o $(JIT)/jit_encode.o $(JIT)/wubu_x86.o $(JIT)/wubu_disasm.o $(JIT)/x86_regalloc.o $(JIT)/jit_minic.o $(JIT)/jit_minic_token.o

# ── GUI Objects ──────────────────────────────────────────────────
GUI_OBJS = $(GUI)/gui_dbuf.o $(GUI)/wubu_theme.o $(GUI)/wubu_settings.o $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.o $(GUI)/wubu_session.o $(GUI)/wubu_session_autostart.o $(GUI)/wubu_notify.o $(GUI)/wubu_clipboard.o $(GUI)/wubu_clipboard_wl.o $(GUI)/wubu_clipboard_mime.o $(GUI)/wubu_screenshot.o $(GUI)/wubu_screenshot_draw.o $(GUI)/wubu_screenshot_png.o $(GUI)/wubu_wayland_stub.o $(GUI)/wubu_mime.o $(GUI)/wubu_mime_desktop.o $(GUI)/wubu_trash.o $(GUI)/wubu_proton.o $(GUI)/wubu_ui.o $(GUI)/wubu_ui_hosted.o $(GUI)/wubu_proton_util.o $(RT)/wubu_fs_util.o $(GUI)/wubu_proton_dxvk.o $(RT)/wubu_proton_dxvk.o $(RT)/wubu_dxvk_conf.o $(GUI)/wubu_proton_exec.o $(GUI)/wubu_proton_config.o $(GUI)/wubu_gamelib.o $(GUI)/wubu_gamelib_config.o $(GUI)/wubu_gamelib_startmenu.o $(GUI)/wubu_gamelib_playtime.o $(GUI)/wubu_gamelib_scan.o $(GUI)/wubu_deploy.o $(GUI)/wubu_deploy_config.o $(GUI)/wubu_deploy_util.o $(GUI)/wubu_deploy_gen.o $(GUI)/wubu_pkgmgr.o $(GUI)/wubu_pkgmgr_resolve.o $(GUI)/wubu_pkgmgr_verify.o $(GUI)/wubu_pkgmgr_manifest.o $(GUI)/wubu_pkgmgr_db.o $(GUI)/wubu_pkgmgr_pkg.o $(GUI)/wubu_pkgmgr_install.o $(GUI)/wubu_pkgmgr_txn.o $(GUI)/wubu_pkgmgr_remote.o $(GUI)/wubu_wm.o $(GUI)/wubu_wm_desktop.o $(GUI)/wubu_wm_input.o $(GUI)/wubu_wm_render.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_icon_glyphs.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_clock.c $(GUI)/dosgui_wm_ctxmenu_engine.c $(GUI)/dosgui_wm_window_state.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/wubu_wallpaper.o $(GUI)/wubu_welcome.o $(GUI)/wubu_bonzi.o $(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o $(GUI)/dosgui_startmenu_db.o $(GUI)/dosgui_startmenu_search.o $(GUI)/dosgui_startmenu_tree.o $(GUI)/dosgui_startmenu_power.o $(GUI)/dosgui_explorer.o $(GUI)/dosgui_explorer_input.o $(GUI)/dosgui_explorer_ops.o $(GUI)/dosgui_explorer_info.o $(GUI)/dosgui_explorer_format.o $(GUI)/dosgui_explorer_drives.o $(GUI)/dosgui_explorer_tree.o $(GUI)/dosgui_explorer_preview.o $(GUI)/dosgui_explorer_render.o $(GUI)/dosgui_explorer_fs.c $(GUI)/dosgui_explorer_fsops.c $(GUI)/dosgui_term.c $(GUI)/dosgui_term_tabs.c $(GUI)/dosgui_term_render.c $(GUI)/dosgui_term_ansi.c $(GUI)/dosgui_term_pty.c $(GUI)/dosgui_daemon_panel.c $(GUI)/dosgui_service_mgr.c $(GUI)/dosgui_dos_window.c $(GUI)/dosgui_window_chrome.c $(GUI)/dosgui_era_apps.c $(GUI)/dosgui_controlpanel.c $(GUI)/dosgui_cp_hardware.c $(GUI)/dosgui_cp_display.c $(GUI)/dosgui_cp_network.c $(GUI)/dosgui_cp_theme.c $(GUI)/dosgui_bpm.c $(GUI)/dosgui_bpm_games.c $(GUI)/dosgui_wm_tray_world.c

# ── Bridge Objects ───────────────────────────────────────────────
BRIDGE_OBJS = $(BRIDGE)/bridge.o $(BRIDGE)/vbe_ws_bridge.o $(BRIDGE)/wubu_syscall.o $(BRIDGE)/wubu_syscall_vbe.o

# ── App Objects ──────────────────────────────────────────────────
APP_OBJS = $(APPS)/repl.o $(APPS)/notepad.o $(APPS)/wubu_editor.o $(APPS)/wubu_editor_bookmark.o $(APPS)/wubu_editor_macro.o $(APPS)/wubu_editor_undo.o $(APPS)/wubu_editor_selection.o $(APPS)/wubu_editor_find.o $(APPS)/wubu_canvas_layers.o $(APPS)/wubu_canvas_draw.o $(APPS)/wubu_canvas_filter.o $(APPS)/wubu_canvas_transform.o $(APPS)/wubu_canvas_plugin.o $(APPS)/wubu_canvas_undo.o $(APPS)/wubu_canvas_blend.o $(APPS)/wubu_canvas_io.o $(APPS)/wubu_image_codec.o $(APPS)/wubu_canvas_io_ppm.o $(APPS)/wubu_codec.o $(APPS)/dosgui_apps.o $(APPS)/cmd/cmd.o $(APPS)/app_canvas.o $(APPS)/app_explorer.o $(APPS)/notes.o $(APPS)/todo.o $(APPS)/music.o \
           $(APPS)/calc/calc.o $(APPS)/calc/calc_math.o $(APPS)/notepad/notepad.o $(APPS)/taskmgr/taskmgr.o $(APPS)/regedit/regedit.o \
           $(APPS)/fm/fm.o $(APPS)/repl/repl.o $(APPS)/control/control.o $(APPS)/editor/editor.o \
           $(APPS)/bonzi/bonzi.o $(APPS)/comfy/comfy.o $(APPS)/tandem/tandem.o

# ── WorldSim Objects ─────────────────────────────────────────────
WS_OBJS = $(WS)/terrain.o $(WS)/entity.o $(WS)/physics.o $(WS)/render.o $(WS)/sim.o

# ── Compiler Objects ─────────────────────────────────────────────
COMP_OBJS = $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_parse_ast.o $(COMP)/holyc_codegen.o $(COMP)/holyc_codegen_emit.o $(COMP)/holyc_codegen_expr.o $(COMP)/holyc_codegen_stmt.o $(COMP)/holyc_codegen_api.o $(COMP)/wubu_preproc.o $(COMP)/holyc_runtime.o $(COMP)/holyc_ptx.o $(COMP)/wubu_mir.o $(COMP)/wubu_mir_lower.o $(COMP)/wubu_isa_driver.o $(COMP)/wubu_isa_x86_64.o $(COMP)/wubu_isa_m68k.o $(COMP)/wubu_m68k_interp.o $(COMP)/wubu_isa_8086.o $(COMP)/wubu_isa_riscv.o $(RT)/wubu_riscv_interp.o $(COMP)/wubu_isa_6502.o $(RT)/wubu_6502_interp.o $(COMP)/wubu_isa_z80.o $(COMP)/wubu_z80_interp.o

# ── Runtime Objects ──────────────────────────────────────────────
RT_OBJS   = $(RT)/wubu_secmon.o $(RT)/wubu_gpu_backend.o $(RT)/wubu_container.o $(RT)/container/wubucontainer.o $(RT)/container/wubucontainer_registry.o $(RT)/wubu_exec.o $(RT)/wubu_exec_format.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_exec_container.o $(RT)/wubu_spawn.o $(RT)/wubu_fs_util.o $(RT)/wubu_vsl.o $(RT)/wubu_proton.o $(RT)/wubu_proton_api.o $(RT)/wubu_proton_dxvk.o  $(RT)/wubu_proton_dll.o $(RT)/wubu_proton_pe.o $(RT)/wubu_dxvk_conf.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_server.o $(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_util.o $(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_arch.o $(RT)/wubu_ramdisk.o $(RT)/wubu_ramdisk_format.o $(RT)/wubu_proton2.o $(RT)/wubu_proton2_gpu.o $(RT)/wubu_proton2_device.o $(RT)/wubu_proton2_gamescope.o $(RT)/wubu_proton2_launch.o $(RT)/wubu_gc.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_bwrap.o $(RT)/wubu_ct_isolate.o $(RT)/ct_iso_seccomp.o $(RT)/ct_iso_cgroup.o $(RT)/ct_iso_ns.o $(RT)/wubu_image.o $(RT)/wubu_image_cache.o $(RT)/wubu_image_parse.o $(RT)/wubu_image_manifest.o $(RT)/wubu_image_ops.o $(RT)/wubu_image_tar.o $(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_diff.o $(RT)/wubu_snapshot_fs.o $(RT)/wubu_snapshot_copy.o $(RT)/wubu_snapshot_tag.o $(RT)/wubu_snapshot_gc.o $(RT)/wubu_snapshot_xport.o $(RT)/wubu_network.o $(RT)/wubu_network_fw.o $(RT)/wubu_network_svc.o $(RT)/wubu_network_cni.o $(RT)/wubu_network_wg.o $(RT)/wubu_network_ts.o $(RT)/wubu_network_dns.o $(RT)/wubu_network_qos.o $(RT)/wubu_network_create.o $(RT)/wubu_netlink.o $(RT)/wubu_archd_daemon.o $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.o $(RT)/wubu_archd_svc_super.o $(RT)/wubu_ns_bridge.o $(RT)/wubu_ns_fs.o $(RT)/wubu_ns_ec.o $(RT)/wubu_ns_steaminput.o $(RT)/wubu_ns_steamrt.o $(RT)/wubu_ns_session.o $(RT)/wubu_ns_world.o $(RT)/wubu_ec_control.o $(RT)/wubu_steaminput.o $(RT)/wubu_steamrt.o $(RT)/wubu_session.o $(RT)/wubu_game_launch.o $(RT)/wubu_game_session.o $(RT)/wubu_agi_play.o $(RT)/wubu_ns_pkg.o $(RT)/wubu_pkg.o $(RT)/wubu_ns_snap.o $(RT)/wubu_bottle_lifecycle.o $(RT)/wubu_bottle_serialize.o $(RT)/wubu_bottle_io.o $(RT)/wubu_bottle_flatpak.o $(RT)/wubu_bottle_ops.o $(RT)/wubu_bottles_json.o $(RT)/wubu_bottles_fs.o $(RT)/wubu_archd_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_holyd.o $(RT)/wubu_holyd_session.o $(RT)/wubu_holyd_exec.o $(RT)/wubu_holyd_repl.o $(RT)/wubu_holyd_input.o $(RT)/wubu_holyd_window.o $(RT)/wubu_holyd_9p.o $(RT)/wubu_holyd_event.o $(RT)/wubu_holyd_save.o $(RT)/wubu_cap/wubu_cap_object.o $(RT)/wubu_cap/wubu_cap_token.o $(RT)/wubu_cap/wubu_cap_revoke.o $(RT)/wubu_cap/wubu_cap_handle.o

# ── Shell Objects ────────────────────────────────────────────────
SHELL_OBJS = $(SHELL_DIR)/wubu_shell.o $(SHELL_DIR)/wubu_shell_history.o \
             $(SHELL_DIR)/wubu_shell_complete.o $(SHELL_DIR)/wubu_shell_exec.o

# ── Bear RL Objects ──────────────────────────────────────────────
BEAR_OBJS = $(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_env_npole.o $(BEAR)/bear_nn_policy.o $(BEAR)/bear_nn_value.o $(BEAR)/bear_nn_ckpt.o $(BEAR)/bear_ppo_traj.o $(BEAR)/bear_ppo_loss.o $(BEAR)/bear_ppo_trainer.o $(BEAR)/bear_opt.o $(BEAR)/bear_cudnn.o $(BEAR)/bear_cudnn_cublas.o $(BEAR)/bear_cudnn_cuda.o $(BEAR)/bear_vulkan_soft.o

# ── Audio Objects ─────────────────────────────────────────────────
AUDIO_OBJS = $(AUDIO)/wubu_audio.o $(AUDIO)/wubu_audio_chips.o $(AUDIO)/wubu_audio_furnace.o $(AUDIO)/wubu_audio_sf2.o $(AUDIO)/wubu_audio_daw.o $(AUDIO)/wubu_audio_engine.o

# ── Tools Objects ─────────────────────────────────────────────────
TOOLS_OBJS = $(TOOLS)/iso9660.o $(TOOLS)/weight_check.o $(TOOLS)/screenshot.o
