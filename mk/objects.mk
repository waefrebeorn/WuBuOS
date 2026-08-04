# ── Directories ──────────────────────────────────────────────────
KERNEL  = src/kernel
JIT     = src/jit
COMP    = src/compiler
GUI     = src/gui
BRIDGE  = src/bridge
APPS    = src/apps
WS      = src/worldsim
RT      = src/runtime
FRAMEWORK = src/framework
TOOLS   = src/tools
HOSTED  = src/hosted
AUDIO   = src/audio
SHELL_DIR  = src/shell
BEAR    = src/bear

# JIT source files (always linked together — self-hosted encoder/disasm/regalloc/minic)
# -ldl is needed for dlopen/dlsym in the MIR backend
JIT_SRCS = $(JIT)/jit.c $(JIT)/jit_encode.c $(JIT)/wubu_x86.c $(JIT)/wubu_disasm.c $(JIT)/x86_regalloc.c $(JIT)/jit_minic.c $(JIT)/jit_minic_token.c -ldl

# ── Kernel Objects ───────────────────────────────────────────────
# (one per line: the objects grow; keep the list readable + diffable)
KERNEL_OBJS = $(KERNEL)/memory.o $(KERNEL)/tasking.o $(KERNEL)/vbe.o \
              $(KERNEL)/input.o $(KERNEL)/interrupt.o $(KERNEL)/interrupt_pic.o \
              $(KERNEL)/interrupt_apic.o $(KERNEL)/interrupt_pit.o \
              $(KERNEL)/interrupt_syscall.o $(KERNEL)/interrupt_timer.o \
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
              $(KERNEL)/wubu_theme.o $(KERNEL)/wubu_hid.o \
              $(KERNEL)/wubu_verifier.o $(KERNEL)/wubu_tss.o \
              $(KERNEL)/wubu_sync.o $(KERNEL)/wubu_vmm.o \
              $(KERNEL)/wubu_memmap.o $(KERNEL)/wubu_serial.o \
              $(KERNEL)/wubu_sha256.o $(KERNEL)/wubu_rtc.o \
              $(KERNEL)/wubu_acpi.o $(KERNEL)/wubu_wdt.o \
              $(KERNEL)/wubu_hpet.o $(KERNEL)/wubu_crash.o \
              $(KERNEL)/wubu_smbios.o $(KERNEL)/wubu_vdso.o \
              $(KERNEL)/wubu_swap.o $(KERNEL)/wubu_as.o \
              $(KERNEL)/wubu_user.o $(KERNEL)/wubu_iommu.o \
              $(KERNEL)/wubu_smp.o $(KERNEL)/wubu_smp_tramp.o \
              $(KERNEL)/wubu_xhci.o \
              $(KERNEL)/wubu_recovery.o \
              $(KERNEL)/wubu_psych.o $(KERNEL)/wubu_bonzi_study.o $(KERNEL)/wubu_tutor.o \
              $(KERNEL)/tasking_switch.o $(KERNEL)/ps2.o \
              $(KERNEL)/wubu_math.o $(KERNEL)/libc.o $(KERNEL)/klog.o

# ── Metal Objects ────────────────────────────────────────────────
METAL_OBJS = $(HOSTED)/wubu_metal.o $(HOSTED)/wubu_metal_evdev.o $(HOSTED)/wubu_metal_x11.o $(HOSTED)/wubu_metal_vulkan.o $(HOSTED)/wubu_metal_drm.o

# ── Hosted Objects ───────────────────────────────────────────────
HOSTED_OBJS_LIST = $(HOSTED)/wubu_gbm.o $(HOSTED)/wubu_vulkan_loader.o $(HOSTED)/wubu_vulkan_swapchain.o $(HOSTED)/wubu_vulkan_cmd.o $(HOSTED)/wubu_vulkan_compute.o $(HOSTED)/wubu_vulkan_util.o $(HOSTED)/wubu_metal_audio.o

# ── JIT Objects ──────────────────────────────────────────────────
JIT_OBJS = $(JIT)/jit.o $(JIT)/jit_encode.o $(JIT)/wubu_x86.o $(JIT)/wubu_disasm.o $(JIT)/x86_regalloc.o $(JIT)/jit_minic.o $(JIT)/jit_minic_token.o

# ── GUI Objects ──────────────────────────────────────────────────
GUI_OBJS = $(GUI)/gui_dbuf.o $(GUI)/wubu_theme.o $(GUI)/wubu_settings.o $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.o $(GUI)/wubu_session.o $(GUI)/wubu_session_autostart.o $(GUI)/wubu_notify.o $(GUI)/wubu_clipboard.o $(GUI)/wubu_clipboard_wl.o $(GUI)/wubu_clipboard_mime.o $(GUI)/wubu_screenshot.o $(GUI)/wubu_screenshot_draw.o $(GUI)/wubu_screenshot_png.o $(GUI)/wubu_wayland_stub.o $(GUI)/wubu_mime.o $(GUI)/wubu_mime_desktop.o $(GUI)/wubu_trash.o $(GUI)/wubu_proton.o $(GUI)/wubu_ui.o $(GUI)/wubu_ui_hosted.o $(GUI)/wubu_proton_util.o $(RT)/wubu_fs_util.o $(GUI)/wubu_proton_dxvk.o $(RT)/wubu_proton_dxvk.o $(RT)/wubu_dxvk_conf.o $(GUI)/wubu_proton_exec.o $(GUI)/wubu_proton_config.o $(GUI)/wubu_gamelib.o $(GUI)/wubu_gamelib_config.o $(GUI)/wubu_gamelib_startmenu.o $(GUI)/wubu_gamelib_playtime.o $(GUI)/wubu_gamelib_scan.o $(GUI)/wubu_deploy.o $(GUI)/wubu_deploy_config.o $(GUI)/wubu_deploy_util.o $(GUI)/wubu_deploy_gen.o $(GUI)/wubu_pkgmgr.o $(GUI)/wubu_pkgmgr_resolve.o $(GUI)/wubu_pkgmgr_verify.o $(GUI)/wubu_pkgmgr_manifest.o $(GUI)/wubu_pkgmgr_db.o $(GUI)/wubu_pkgmgr_pkg.o $(GUI)/wubu_pkgmgr_install.o $(GUI)/wubu_pkgmgr_txn.o $(GUI)/wubu_pkgmgr_remote.o $(GUI)/wubu_wm.o $(GUI)/wubu_wm_desktop.o $(GUI)/wubu_wm_input.o $(GUI)/wubu_wm_render.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_icon_glyphs.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_clock.c $(GUI)/dosgui_wm_ctxmenu_engine.c $(GUI)/dosgui_wm_window_state.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/wubu_wallpaper.o $(GUI)/wubu_welcome.o $(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o $(GUI)/dosgui_startmenu_db.o $(GUI)/dosgui_startmenu_search.o $(GUI)/dosgui_startmenu_tree.o $(GUI)/dosgui_startmenu_power.o $(GUI)/dosgui_explorer.o $(GUI)/dosgui_explorer_input.o $(GUI)/dosgui_explorer_ops.o $(GUI)/dosgui_explorer_info.o $(GUI)/dosgui_explorer_format.o $(GUI)/dosgui_explorer_drives.o $(GUI)/dosgui_explorer_tree.o $(GUI)/dosgui_explorer_preview.o $(GUI)/dosgui_explorer_render.o $(GUI)/dosgui_explorer_fs.o $(GUI)/dosgui_explorer_fsops.o $(GUI)/dosgui_term.o $(GUI)/dosgui_term_tabs.o $(GUI)/dosgui_term_render.o $(GUI)/dosgui_term_ansi.o $(GUI)/dosgui_term_pty.o $(GUI)/dosgui_daemon_panel.o $(GUI)/dosgui_service_mgr.o $(GUI)/dosgui_dos_window.o $(GUI)/dosgui_window_chrome.o $(GUI)/dosgui_era_apps.o

# ── Bridge Objects ───────────────────────────────────────────────
BRIDGE_OBJS = $(BRIDGE)/bridge.o $(BRIDGE)/vbe_ws_bridge.o $(BRIDGE)/wubu_syscall.o $(BRIDGE)/wubu_syscall_vbe.o

# ── App Objects ──────────────────────────────────────────────────
APP_OBJS = $(APPS)/repl.o $(APPS)/notepad.o $(APPS)/wubu_editor.o $(APPS)/wubu_editor_bookmark.o $(APPS)/wubu_editor_macro.o $(APPS)/wubu_editor_undo.o $(APPS)/wubu_editor_selection.o $(APPS)/wubu_editor_find.o $(APPS)/wubu_canvas_layers.o $(APPS)/wubu_canvas_draw.o $(APPS)/wubu_canvas_filter.o $(APPS)/wubu_canvas_transform.o $(APPS)/wubu_canvas_plugin.o $(APPS)/wubu_canvas_undo.o $(APPS)/wubu_canvas_blend.o $(APPS)/wubu_canvas_io.o $(APPS)/wubu_image_codec.o $(APPS)/wubu_canvas_io_ppm.o $(APPS)/wubu_codec.o $(APPS)/dosgui_apps.o $(APPS)/cmd/cmd.o $(APPS)/app_canvas.o $(APPS)/app_explorer.o \
           $(APPS)/calc/calc.o $(APPS)/calc/calc_math.o $(APPS)/notepad/notepad.o $(APPS)/taskmgr/taskmgr.o $(APPS)/regedit/regedit.o \
           $(APPS)/fm/fm.o $(APPS)/repl/repl.o $(APPS)/control/control.o $(APPS)/editor/editor.o \
           $(APPS)/bonzi/bonzi.o $(APPS)/comfy/comfy.o $(APPS)/tandem/tandem.o

# ── WorldSim Objects ─────────────────────────────────────────────
WS_OBJS = $(WS)/terrain.o $(WS)/entity.o $(WS)/physics.o $(WS)/render.o $(WS)/sim.o

COMP_OBJS = $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_parse_ast.o $(COMP)/holyc_codegen.o $(COMP)/holyc_codegen_emit.o $(COMP)/holyc_codegen_expr.o $(COMP)/holyc_codegen_stmt.o $(COMP)/holyc_codegen_api.o $(COMP)/holyc_runtime.o $(COMP)/holyc_ptx.o
RT_OBJS   = $(RT)/wubu_container.o $(RT)/container/wubucontainer.o $(RT)/container/wubucontainer_registry.o $(RT)/wubu_exec.o $(RT)/wubu_exec_format.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_exec_container.o $(RT)/wubu_spawn.o $(RT)/wubu_fs_util.o $(RT)/wubu_vsl.o $(RT)/wubu_proton.o $(RT)/wubu_proton_api.o $(RT)/wubu_proton_dxvk.o  $(RT)/wubu_proton_dll.o $(RT)/wubu_proton_pe.o $(RT)/wubu_dxvk_conf.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_server.o $(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_util.o $(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_arch.o $(RT)/wubu_ramdisk.o $(RT)/wubu_ramdisk_format.o $(RT)/wubu_proton2.o $(RT)/wubu_proton2_gpu.o $(RT)/wubu_proton2_device.o $(RT)/wubu_proton2_gamescope.o $(RT)/wubu_proton2_launch.o $(RT)/wubu_gc.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_bwrap.o $(RT)/wubu_ct_isolate.o $(RT)/ct_iso_seccomp.o $(RT)/ct_iso_cgroup.o $(RT)/ct_iso_ns.o $(RT)/wubu_image.o $(RT)/wubu_image_cache.o $(RT)/wubu_image_parse.o $(RT)/wubu_image_manifest.o $(RT)/wubu_image_ops.o $(RT)/wubu_image_tar.o $(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_diff.o $(RT)/wubu_snapshot_fs.o $(RT)/wubu_snapshot_copy.o $(RT)/wubu_snapshot_tag.o $(RT)/wubu_snapshot_gc.o $(RT)/wubu_snapshot_xport.o $(RT)/wubu_network.o $(RT)/wubu_network_fw.o $(RT)/wubu_network_svc.o $(RT)/wubu_network_cni.o $(RT)/wubu_network_wg.o $(RT)/wubu_network_ts.o $(RT)/wubu_network_dns.o $(RT)/wubu_network_qos.o $(RT)/wubu_network_create.o $(RT)/wubu_netlink.o $(RT)/wubu_archd_daemon.o $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.o $(RT)/wubu_ns_bridge.o $(RT)/wubu_ns_fs.o $(RT)/wubu_ns_pkg.o $(RT)/wubu_pkg.o $(RT)/wubu_ns_snap.o $(RT)/wubu_bottle_lifecycle.o $(RT)/wubu_bottle_serialize.o $(RT)/wubu_bottle_io.o $(RT)/wubu_bottle_flatpak.o $(RT)/wubu_bottle_ops.o $(RT)/wubu_bottles_json.o $(RT)/wubu_bottles_fs.o $(RT)/wubu_archd_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_holyd.o $(RT)/wubu_holyd_session.o $(RT)/wubu_holyd_exec.o $(RT)/wubu_holyd_repl.o $(RT)/wubu_holyd_input.o $(RT)/wubu_holyd_window.o $(RT)/wubu_holyd_9p.o $(RT)/wubu_holyd_event.o $(RT)/wubu_holyd_save.o $(RT)/wubu_cap/wubu_cap_object.o $(RT)/wubu_cap/wubu_cap_token.o $(RT)/wubu_cap/wubu_cap_revoke.o $(RT)/wubu_cap/wubu_cap_handle.o

# In-process 8086/DOS shim: public API + leaf engine modules. Always link as a unit.
WUBU_DOS_EMU_OBJS = $(RT)/wubu_dos_emu.o $(RT)/wubu_dos_emu_mem.o $(RT)/wubu_dos_emu_regs.o $(RT)/wubu_dos_emu_alu.o $(RT)/wubu_dos_emu_int.o $(RT)/wubu_dos_emu_decode.o


TOOLS_OBJS = $(TOOLS)/iso9660.o $(TOOLS)/weight_check.o $(TOOLS)/screenshot.o

# ── Shell Objects ────────────────────────────────────────────────
SHELL_OBJS = $(SHELL_DIR)/wubu_shell.o $(SHELL_DIR)/wubu_shell_history.o \
             $(SHELL_DIR)/wubu_shell_complete.o $(SHELL_DIR)/wubu_shell_exec.o

# ── Bear RL Objects ──────────────────────────────────────────────
BEAR_OBJS = $(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_env_npole.o $(BEAR)/bear_nn_policy.o $(BEAR)/bear_nn_value.o $(BEAR)/bear_nn_ckpt.o $(BEAR)/bear_ppo_traj.o $(BEAR)/bear_ppo_loss.o $(BEAR)/bear_ppo_trainer.o $(BEAR)/bear_opt.o $(BEAR)/bear_cudnn.o $(BEAR)/bear_cudnn_cublas.o $(BEAR)/bear_cudnn_cuda.o $(BEAR)/bear_vulkan_soft.o

# ── Audio Objects ─────────────────────────────────────────────────