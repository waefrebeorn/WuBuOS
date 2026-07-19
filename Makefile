# My Seed OS — Top-Level Makefile
# Builds: kernel, JIT, GUI, bridge, apps, tests

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g -D_POSIX_C_SOURCE=200809L -Wno-array-bounds -DWUBU_NO_LIBM -I/usr/include/libdrm -Wno-unused-result -Wno-implicit-function-declaration -Wno-return-type -Wno-unused-variable -Wno-format-truncation -Wno-unused-parameter
# Kernel objects are freestanding and use the real assembly context switch
# (WUBU_BAREMETAL), not the hosted setjmp/longjmp path.
KERNEL_CFLAGS = -DMYSEED_METAL -DWUBU_BAREMETAL=1 -DWUBU_NO_LIBM
LDFLAGS = -lcublas -lcudnn -lcudart

# ── Directories ──────────────────────────────────────────────────
KERNEL  = src/kernel
JIT     = src/jit
COMP    = src/compiler
GUI     = src/gui
BRIDGE  = src/bridge
APPS    = src/apps
WS      = src/worldsim
RT      = src/runtime
TOOLS   = src/tools
HOSTED  = src/hosted
AUDIO   = src/audio
SHELL_DIR  = src/shell
BEAR    = src/bear

# JIT source files (always linked together — self-hosted encoder/disasm/regalloc/minic)
# -ldl is needed for dlopen/dlsym in the MIR backend
JIT_SRCS = $(JIT)/jit.c $(JIT)/jit_encode.c $(JIT)/wubu_x86.c $(JIT)/wubu_disasm.c $(JIT)/x86_regalloc.c $(JIT)/jit_minic.c $(JIT)/jit_minic_token.c -ldl

# ── Kernel Objects ───────────────────────────────────────────────
KERNEL_OBJS = $(KERNEL)/memory.o $(KERNEL)/tasking.o $(KERNEL)/vbe.o \
              $(KERNEL)/input.o $(KERNEL)/interrupt.o $(KERNEL)/interrupt_pic.o $(KERNEL)/interrupt_apic.o $(KERNEL)/interrupt_pit.o $(KERNEL)/interrupt_syscall.o $(KERNEL)/interrupt_timer.o $(KERNEL)/isr_stubs.o $(KERNEL)/fat32.o $(KERNEL)/fat32_fat.o $(KERNEL)/fat32_dir.o $(KERNEL)/fat32_file.o $(KERNEL)/fat32_format.o $(KERNEL)/fat32_name.o $(KERNEL)/fat32_cluster.o $(KERNEL)/ahci.o $(KERNEL)/txfs.o $(KERNEL)/wubu_gaad.o $(KERNEL)/tasking_switch.o $(KERNEL)/ps2.o $(KERNEL)/wubu_math.o $(KERNEL)/libc.o $(KERNEL)/klog.o

# ── Metal Objects ────────────────────────────────────────────────
METAL_OBJS = $(HOSTED)/wubu_metal.o $(HOSTED)/wubu_metal_evdev.o $(HOSTED)/wubu_metal_x11.o $(HOSTED)/wubu_metal_vulkan.o $(HOSTED)/wubu_metal_drm.o

# ── Hosted Objects ───────────────────────────────────────────────
HOSTED_OBJS_LIST = $(HOSTED)/wubu_gbm.o $(HOSTED)/wubu_vulkan_loader.o $(HOSTED)/wubu_vulkan_swapchain.o $(HOSTED)/wubu_vulkan_cmd.o $(HOSTED)/wubu_vulkan_compute.o $(HOSTED)/wubu_vulkan_util.o $(HOSTED)/wubu_metal_audio.o

# ── JIT Objects ──────────────────────────────────────────────────
JIT_OBJS = $(JIT)/jit.o $(JIT)/jit_encode.o $(JIT)/wubu_x86.o $(JIT)/wubu_disasm.o $(JIT)/x86_regalloc.o $(JIT)/jit_minic.o $(JIT)/jit_minic_token.o

# ── GUI Objects ──────────────────────────────────────────────────
GUI_OBJS = $(GUI)/gui_dbuf.o $(GUI)/wubu_theme.o $(GUI)/wubu_settings.o $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.o $(GUI)/wubu_session.o $(GUI)/wubu_session_autostart.o $(GUI)/wubu_notify.o $(GUI)/wubu_clipboard.o $(GUI)/wubu_clipboard_wl.o $(GUI)/wubu_clipboard_mime.o $(GUI)/wubu_screenshot.o $(GUI)/wubu_screenshot_draw.o $(GUI)/wubu_screenshot_png.o $(GUI)/wubu_wayland_stub.o $(GUI)/wubu_mime.o $(GUI)/wubu_mime_desktop.o $(GUI)/wubu_trash.o $(GUI)/wubu_proton.o $(GUI)/wubu_proton_util.o $(RT)/wubu_fs_util.o $(GUI)/wubu_proton_dxvk.o $(RT)/wubu_proton_dxvk.o $(RT)/wubu_dxvk_conf.o $(GUI)/wubu_proton_exec.o $(GUI)/wubu_proton_config.o $(GUI)/wubu_gamelib.o $(GUI)/wubu_gamelib_config.o $(GUI)/wubu_gamelib_startmenu.o $(GUI)/wubu_gamelib_playtime.o $(GUI)/wubu_gamelib_scan.o $(GUI)/wubu_deploy.o $(GUI)/wubu_deploy_config.o $(GUI)/wubu_deploy_util.o $(GUI)/wubu_deploy_gen.o $(GUI)/wubu_pkgmgr.o $(GUI)/wubu_pkgmgr_resolve.o $(GUI)/wubu_pkgmgr_verify.o $(GUI)/wubu_pkgmgr_manifest.o $(GUI)/wubu_pkgmgr_db.o $(GUI)/wubu_pkgmgr_pkg.o $(GUI)/wubu_pkgmgr_install.o $(GUI)/wubu_pkgmgr_txn.o $(GUI)/wubu_pkgmgr_remote.o $(GUI)/wubu_wm.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_clock.c $(GUI)/dosgui_wm_ctxmenu_engine.c $(GUI)/dosgui_wm_window_state.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/wubu_wallpaper.o $(GUI)/wubu_welcome.o $(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o $(GUI)/dosgui_startmenu_db.o $(GUI)/dosgui_startmenu_search.o $(GUI)/dosgui_startmenu_tree.o $(GUI)/dosgui_startmenu_power.o $(GUI)/dosgui_explorer.o $(GUI)/dosgui_explorer_input.o $(GUI)/dosgui_explorer_ops.o $(GUI)/dosgui_explorer_info.o $(GUI)/dosgui_explorer_format.o $(GUI)/dosgui_explorer_drives.o $(GUI)/dosgui_explorer_tree.o $(GUI)/dosgui_explorer_preview.o $(GUI)/dosgui_explorer_zip.o $(GUI)/dosgui_explorer_render.o $(GUI)/dosgui_explorer_fs.o $(GUI)/dosgui_explorer_fsops.o $(GUI)/dosgui_term.o $(GUI)/dosgui_term_tabs.o $(GUI)/dosgui_term_render.o $(GUI)/dosgui_term_ansi.o $(GUI)/dosgui_term_pty.o $(GUI)/dosgui_daemon_panel.o $(GUI)/dosgui_service_mgr.o $(GUI)/dosgui_dos_window.o

# ── Bridge Objects ───────────────────────────────────────────────
BRIDGE_OBJS = $(BRIDGE)/bridge.o $(BRIDGE)/vbe_ws_bridge.o $(BRIDGE)/wubu_syscall.o $(BRIDGE)/wubu_syscall_vbe.o

# ── App Objects ──────────────────────────────────────────────────
APP_OBJS = $(APPS)/repl.o $(APPS)/notepad.o $(APPS)/wubu_editor.o $(APPS)/wubu_editor_bookmark.o $(APPS)/wubu_editor_macro.o $(APPS)/wubu_editor_undo.o $(APPS)/wubu_editor_selection.o $(APPS)/wubu_editor_find.o $(APPS)/wubu_canvas_layers.o $(APPS)/wubu_canvas_draw.o $(APPS)/wubu_canvas_filter.o $(APPS)/wubu_canvas_transform.o $(APPS)/wubu_canvas_plugin.o $(APPS)/wubu_canvas_undo.o $(APPS)/wubu_canvas_blend.o $(APPS)/wubu_canvas_io.o $(APPS)/wubu_image_codec.o $(APPS)/wubu_canvas_io_ppm.o $(APPS)/wubu_codec.o $(APPS)/dosgui_apps.o $(APPS)/cmd/cmd.o $(APPS)/app_canvas.o $(APPS)/app_explorer.o \
           $(APPS)/calc/calc.o $(APPS)/calc/calc_math.o $(APPS)/notepad/notepad.o $(APPS)/taskmgr/taskmgr.o $(APPS)/regedit/regedit.o \
           $(APPS)/fm/fm.o $(APPS)/repl/repl.o $(APPS)/control/control.o $(APPS)/editor/editor.o

# ── WorldSim Objects ─────────────────────────────────────────────
WS_OBJS = $(WS)/terrain.o $(WS)/entity.o $(WS)/physics.o $(WS)/render.o $(WS)/sim.o

COMP_OBJS = $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_parse_ast.o $(COMP)/holyc_codegen.o $(COMP)/holyc_codegen_emit.o $(COMP)/holyc_codegen_expr.o $(COMP)/holyc_codegen_stmt.o $(COMP)/holyc_codegen_api.o $(COMP)/holyc_ptx.o
RT_OBJS   = $(RT)/wubu_container.o $(RT)/wubu_exec.o $(RT)/wubu_exec_format.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_exec_container.o $(RT)/wubu_spawn.o $(RT)/wubu_fs_util.o $(RT)/wubu_vsl.o $(RT)/wubu_proton.o $(RT)/wubu_proton_api.o $(RT)/wubu_proton_dxvk.o  $(RT)/wubu_proton_dll.o $(RT)/wubu_proton_pe.o $(RT)/wubu_dxvk_conf.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_server.o $(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_util.o $(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_arch.o $(RT)/wubu_ramdisk.o $(RT)/wubu_ramdisk_format.o $(RT)/wubu_proton2.o $(RT)/wubu_proton2_gpu.o $(RT)/wubu_proton2_device.o $(RT)/wubu_proton2_gamescope.o $(RT)/wubu_proton2_launch.o $(RT)/wubu_gc.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_bwrap.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_image.o $(RT)/wubu_image_cache.o $(RT)/wubu_image_parse.o $(RT)/wubu_image_manifest.o $(RT)/wubu_image_ops.o $(RT)/wubu_image_tar.o $(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_diff.o $(RT)/wubu_snapshot_fs.o $(RT)/wubu_snapshot_copy.o $(RT)/wubu_snapshot_tag.o $(RT)/wubu_snapshot_gc.o $(RT)/wubu_snapshot_xport.o $(RT)/wubu_network.o $(RT)/wubu_network_fw.o $(RT)/wubu_network_svc.o $(RT)/wubu_network_cni.o $(RT)/wubu_network_wg.o $(RT)/wubu_network_ts.o $(RT)/wubu_network_dns.o $(RT)/wubu_network_qos.o $(RT)/wubu_network_create.o $(RT)/wubu_netlink.o $(RT)/wubu_archd_daemon.o $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.o $(RT)/wubu_ns_bridge.o $(RT)/wubu_ns_fs.o $(RT)/wubu_ns_pkg.o $(RT)/wubu_pkg.o $(RT)/wubu_ns_snap.o $(RT)/wubu_bottle_lifecycle.o $(RT)/wubu_bottle_serialize.o $(RT)/wubu_bottle_io.o $(RT)/wubu_bottle_flatpak.o $(RT)/wubu_bottle_ops.o $(RT)/wubu_bottles_json.o $(RT)/wubu_bottles_fs.o $(RT)/wubu_archd_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_holyd.o $(RT)/wubu_holyd_session.o $(RT)/wubu_holyd_exec.o $(RT)/wubu_holyd_repl.o $(RT)/wubu_cap/wubu_cap_object.o $(RT)/wubu_cap/wubu_cap_token.o $(RT)/wubu_cap/wubu_cap_revoke.o $(RT)/wubu_cap/wubu_cap_handle.o

# In-process 8086/DOS shim: public API + leaf engine modules. Always link as a unit.
WUBU_DOS_EMU_OBJS = $(RT)/wubu_dos_emu.o $(RT)/wubu_dos_emu_mem.o $(RT)/wubu_dos_emu_regs.o $(RT)/wubu_dos_emu_alu.o $(RT)/wubu_dos_emu_int.o $(RT)/wubu_dos_emu_decode.o


TOOLS_OBJS = $(TOOLS)/iso9660.o $(TOOLS)/weight_check.o $(TOOLS)/screenshot.o

# ── Shell Objects ────────────────────────────────────────────────
SHELL_OBJS = $(SHELL_DIR)/wubu_shell.o $(SHELL_DIR)/wubu_shell_history.o \
             $(SHELL_DIR)/wubu_shell_complete.o $(SHELL_DIR)/wubu_shell_exec.o

# ── Bear RL Objects ──────────────────────────────────────────────
BEAR_OBJS = $(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_env_npole.o $(BEAR)/bear_nn_policy.o $(BEAR)/bear_nn_value.o $(BEAR)/bear_nn_ckpt.o $(BEAR)/bear_ppo_traj.o $(BEAR)/bear_ppo_loss.o $(BEAR)/bear_ppo_trainer.o $(BEAR)/bear_opt.o $(BEAR)/bear_cudnn.o $(BEAR)/bear_cudnn_cublas.o $(BEAR)/bear_cudnn_cuda.o $(BEAR)/bear_vulkan_soft.o

# ── Audio Objects ─────────────────────────────────────────────────
AUDIO_OBJS = $(AUDIO)/wubu_audio.o $(AUDIO)/wubu_audio_chips.o $(AUDIO)/wubu_audio_furnace.o $(AUDIO)/wubu_audio_sf2.o $(AUDIO)/wubu_audio_daw.o $(AUDIO)/wubu_audio_engine.o

# ── Targets ─────────────────────────────────────────────────────

.PHONY: all clean test kernel jit gui bridge apps worldsim

# Header dependency tracking: every .c compile now emits a .d file (via -MMD
# -MP in the pattern rules). Including them makes `make` rebuild an object
# whenever any header it includes changes -- previously a header edit left
# stale .o files linked into test binaries, causing phantom test failures.
-include $(shell find src -name '*.d' 2>/dev/null)

shell: $(SHELL_DIR)/wubu_shell
	@echo "✅ Shell built (./src/shell/wubu_shell)"

$(SHELL_DIR)/wubu_shell: $(SHELL_OBJS)
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) \
		$(SHELL_OBJS) -o $@

$(SHELL_DIR)/%.o: $(SHELL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) -MMD -MP -c $< -o $@

all: kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs
	@echo "✅ WuBuOS built"

metal: $(METAL_OBJS)
	@echo "✅ Metal layer built"

audio: $(AUDIO_OBJS)
	@echo "✅ Audio engine built"

hosted_objs: $(HOSTED_OBJS_LIST)
	@echo "✅ Hosted objects built"

bear: $(BEAR_OBJS)
	@echo "✅ Bear RL layer built"

bear_train: $(BEAR_OBJS) $(BEAR)/bear_train.o
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) \
		$(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_env_npole.o $(BEAR)/bear_nn_policy.o $(BEAR)/bear_nn_value.o $(BEAR)/bear_nn_ckpt.o $(BEAR)/bear_ppo_traj.o $(BEAR)/bear_ppo_loss.o $(BEAR)/bear_ppo_trainer.o $(BEAR)/bear_opt.o $(BEAR)/bear_cudnn.o $(BEAR)/bear_cudnn_cublas.o $(BEAR)/bear_cudnn_cuda.o $(BEAR)/bear_train.o $(BEAR)/bear_vulkan_soft.o \
		-lm -o $(BEAR)/bear_train
	@echo "✅ Bear RL training binary built (./src/bear/bear_train)"

kernel: $(KERNEL_OBJS) $(KERNEL)/crt0.o $(KERNEL)/metal_main.o
	$(CC) $(CFLAGS) -DMYSEED_METAL -DWUBU_NO_LIBM -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -Wl,-no-pie \
		-T $(KERNEL)/kernel.ld \
		$(KERNEL)/crt0.o $(KERNEL)/metal_main.o $(KERNEL_OBJS) \
		-o $(KERNEL)/kernel.elf
	@echo "✅ Bare-metal kernel.elf built"

# ── Bare-metal boot / QEMU validation harness ──────────────────────────
# boot.S is a self-contained 16-bit BIOS bootsector that loads kernel.elf
# (concatenated at LBA 1), parses its ELF64 program headers, switches to
# 32-bit protected mode, and jumps to the kernel entry. It assembles to a
# clean 512-byte image (SeaBIOS loads a raw sector at linear 0, so `.org 0`
# is used and the AA55 signature lands at offset 510). The `qemu` target
# packs boot.bin + kernel.elf into a 1.44MB floppy image and boots it,
# capturing the serial boot trace on stdout.
$(KERNEL)/boot.bin: $(KERNEL)/boot.S
	$(CC) -c -m16 -ffreestanding -nostdlib -I$(KERNEL) $(KERNEL)/boot.S -o $(KERNEL)/boot.o
	objcopy -O binary -j .text $(KERNEL)/boot.o $(KERNEL)/boot.bin
	truncate -s 512 $(KERNEL)/boot.bin

$(KERNEL)/disk.img: $(KERNEL)/boot.bin $(KERNEL)/kernel.elf
	cat $(KERNEL)/boot.bin $(KERNEL)/kernel.elf /dev/zero | head -c 1474560 > $(KERNEL)/disk.img

boot: $(KERNEL)/boot.bin
	@echo "✅ Bootsector built (./src/kernel/boot.bin, 512 bytes)"

qemu: $(KERNEL)/disk.img
	@echo "▶ Booting WuBuOS kernel in QEMU via bootsector floppy (true bare-metal path)..."
	qemu-system-x86_64 -fda $(KERNEL)/disk.img -serial stdio \
		-display none -m 128 -no-reboot \
		-device isa-debug-exit,iobase=0xf4 || true



jit: $(JIT_OBJS)
	@echo "✅ JIT built"

compiler: $(COMP_OBJS)
	@echo "✅ HolyC compiler built"

runtime: $(RT_OBJS)
	@echo "✅ WuBuOS runtime built"

tools: $(TOOLS_OBJS)
	@echo "✅ WuBuOS tools built"

gui: $(GUI_OBJS)
	@echo "✅ GUI built"

bridge: $(BRIDGE_OBJS)
	@echo "✅ Bridge built"

apps: $(APP_OBJS) $(APP_RT_OBJS)
	@echo "✅ Apps built"

# ── Game / App Targets ───────────────────────────────────────────

canvas: apps runtime $(GUI_OBJS) $(KERNEL_OBJS) $(JIT_OBJS) $(COMP_OBJS) $(HOSTED)/archd_hosted.o $(GUI)/standalone_hosted_shim.o $(HOSTED)/primary-selection-private.o $(HOSTED)/xdg-shell-private.o
	$(CC) $(CFLAGS) -I$(APPS) -I$(GUI) -I$(KERNEL) -I$(JIT) -I$(HOSTED) \
		-Wl,--start-group \
		$(APPS)/canvas_standalone.c $(GUI_OBJS) \
		$(KERNEL)/memory.c $(KERNEL)/vbe.c $(KERNEL)/input.c \
		$(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(KERNEL)/wubu_math.c \
		$(KERNEL)/wubu_gaad.o \
		$(APP_RT_OBJS) \
		$(GUI)/standalone_hosted_shim.o \
		$(HOSTED)/primary-selection-private.o $(HOSTED)/xdg-shell-private.o \
		$(APP_OBJS) \
		$(JIT_OBJS) $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c \
		-Wl,--end-group \
		-lm -lsqlite3 -lzstd -lz -lwayland-client -lxkbcommon -ldl -lvulkan -ljson-c -o $(APPS)/canvas
	@echo "✅ Canvas (wubu_canvas) built (./src/apps/canvas)"

worldsim: $(WS_OBJS)
	@echo "✅ WorldSim built"

# ── Hosted binary (full WuBuOS GUI shell + kernel in-process) ────
# Cell 200: ZealOS kernel runs in-process, WM + desktop + taskbar + start menu
# Incremental object list for the hosted binary. Each .o is built exactly once
# by its directory's pattern rule; the link step reuses them so editing one
# file only recompiles that one .o + relinks, instead of recompiling the whole
# 60+ file translation unit on every `make hosted`.
#
# Faithful to the old inline recipe's behaviour:
#   * Kernel .c files (memory/input/tasking/interrupt/wubu_math) and vbe.c are
#     compiled with the DEFAULT (PIE-capable) CFLAGS, NOT the bare-metal kernel
#     pattern-rule flags (-fno-pie -mcmodel=kernel), because the hosted binary
#     is a userspace PIE. Hosted variants are <name>_khosted.o / vbe_hosted.o.
#   * wubu_archd.c / wubu_holyd_lifecycle.c gate their daemon `main` behind
#     #ifndef WUBD_TEST_MAIN; the hosted shell supplies the one true main, so
#     archd is compiled with -DWUBD_TEST_MAIN (archd_hosted.o).
HOSTED_OBJS = \
	$(HOSTED)/hosted.o $(HOSTED)/hosted_render.o $(HOSTED)/hosted_pe.o $(HOSTED)/hosted_run.o $(HOSTED)/hosted_wayland.o $(HOSTED)/hosted_wayland_shm.o $(HOSTED)/hosted_wayland_input.o $(HOSTED)/hosted_wayland_surface.o $(HOSTED)/vbe_hosted.o \
	$(HOSTED)/hosted_styxfs.o \
	$(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_server.o $(RT)/styxfs_util.o \
	$(HOSTED)/memory_khosted.o $(HOSTED)/input_khosted.o $(HOSTED)/tasking_khosted.o $(HOSTED)/interrupt_khosted.o $(HOSTED)/wubu_math_khosted.o \
	$(BRIDGE)/bridge.o $(BRIDGE)/wubu_syscall.o $(BRIDGE)/wubu_syscall_vbe.o \
	$(GUI)/gui_dbuf.o $(GUI)/wubu_theme.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_taskbar.c \
	$(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o $(GUI)/dosgui_startmenu_db.o $(GUI)/dosgui_startmenu_search.o $(GUI)/dosgui_startmenu_tree.o $(GUI)/dosgui_startmenu_power.o $(GUI)/dosgui_explorer.o $(GUI)/dosgui_explorer_input.o $(GUI)/dosgui_explorer_ops.o $(GUI)/dosgui_explorer_info.o $(GUI)/dosgui_explorer_format.o $(GUI)/dosgui_explorer_drives.o $(GUI)/dosgui_explorer_tree.o $(GUI)/dosgui_explorer_preview.o $(GUI)/dosgui_explorer_zip.o $(GUI)/dosgui_explorer_render.o $(GUI)/dosgui_explorer_fs.o $(GUI)/dosgui_explorer_fsops.o $(GUI)/dosgui_term.o \
	$(GUI)/dosgui_term_tabs.o $(GUI)/dosgui_term_render.o $(GUI)/dosgui_term_ansi.o \
	$(GUI)/dosgui_term_pty.o $(GUI)/dosgui_daemon_panel.o $(GUI)/dosgui_service_mgr.o $(GUI)/dosgui_dos_window.o $(GUI)/wubu_settings.o $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.o \
	$(GUI)/wubu_session.o $(GUI)/wubu_notify.o $(GUI)/wubu_clipboard.o $(GUI)/wubu_clipboard_wl.o $(GUI)/wubu_clipboard_mime.o $(GUI)/wubu_screenshot.o $(GUI)/wubu_screenshot_draw.o $(GUI)/wubu_screenshot_png.o $(GUI)/wubu_mime.o $(GUI)/wubu_mime_desktop.o \
	$(GUI)/wubu_trash.o $(GUI)/wubu_proton.o $(GUI)/wubu_proton_util.o $(GUI)/wubu_proton_dxvk.o $(RT)/wubu_proton_dxvk.o $(RT)/wubu_dxvk_conf.o $(GUI)/wubu_proton_exec.o $(GUI)/wubu_proton_config.o $(GUI)/wubu_gamelib.o $(GUI)/wubu_gamelib_config.o $(GUI)/wubu_gamelib_startmenu.o $(GUI)/wubu_gamelib_playtime.o $(GUI)/wubu_gamelib_scan.o $(GUI)/wubu_deploy.o $(GUI)/wubu_deploy_config.o $(GUI)/wubu_deploy_util.o $(GUI)/wubu_deploy_gen.o $(GUI)/wubu_pkgmgr.o $(GUI)/wubu_pkgmgr_resolve.o $(GUI)/wubu_pkgmgr_verify.o $(GUI)/wubu_pkgmgr_manifest.o $(GUI)/wubu_pkgmgr_db.o $(GUI)/wubu_pkgmgr_pkg.o $(GUI)/wubu_pkgmgr_install.o $(GUI)/wubu_pkgmgr_txn.o $(GUI)/wubu_pkgmgr_remote.o $(GUI)/wubu_wallpaper.o $(GUI)/wubu_welcome.o \
	$(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_parse_ast.o $(COMP)/holyc_codegen.o $(COMP)/holyc_codegen_emit.o \
	$(COMP)/holyc_codegen_expr.o $(COMP)/holyc_codegen_stmt.o $(COMP)/holyc_codegen_api.o \
	$(APPS)/repl.o $(APPS)/notepad.o $(APPS)/dosgui_apps.o $(APPS)/cmd/cmd.o $(APPS)/app_canvas.o $(APPS)/app_explorer.o $(APPS)/wubu_canvas_layers.o $(APPS)/wubu_canvas_draw.o $(APPS)/wubu_canvas_filter.o $(APPS)/wubu_canvas_transform.o $(APPS)/wubu_canvas_plugin.o $(APPS)/wubu_canvas_undo.o $(APPS)/wubu_canvas_blend.o $(APPS)/wubu_canvas_io.o $(APPS)/wubu_image_codec.o $(APPS)/wubu_canvas_io_ppm.o $(APPS)/wubu_codec.o \
	$(JIT)/jit.o $(JIT)/jit_encode.o $(JIT)/wubu_x86.o $(JIT)/wubu_disasm.o $(JIT)/x86_regalloc.o $(JIT)/jit_minic.o $(JIT)/jit_minic_token.o \
	$(RT)/wubu_host_exec.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_ct_isolate_cgroup.o $(RT)/wubu_ct_bwrap.o $(RT)/wubu_exec.o $(RT)/wubu_exec_format.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_exec_container.o $(RT)/wubu_container.o $(RT)/wubu_compat_db.o $(RT)/wubu_session.o $(GUI)/wubu_session_autostart.o $(RT)/wubu_arch.o $(RT)/wubu_archd_util.o \
	 $(HOSTED)/archd_hosted.o $(RT)/wubu_ramdisk.o $(RT)/wubu_ramdisk_format.o $(RT)/wubu_spawn.o \
	$(RT)/oci/oci_http_client.o \
	$(HOSTED)/xdg-shell-private.o $(HOSTED)/primary-selection-private.o

# Kernel sources for the hosted (userspace PIE) binary: compile with the default
# CFLAGS (no -fno-pie / -mcmodel=kernel) so they link into a PIE executable.
$(HOSTED)/%_khosted.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL) -I$(HOSTED) -I$(GUI) -I$(RT) -MMD -MP -c $< -o $@

# vbe.c with -DVBE_HOSTED for the in-process hosted framebuffer.
$(HOSTED)/vbe_hosted.o: $(KERNEL)/vbe.c
	$(CC) $(CFLAGS) -DVBE_HOSTED -I$(KERNEL) -I$(HOSTED) -I$(GUI) -I$(RT) -MMD -MP -c $< -o $@

# archd without its daemon main (the hosted shell owns main).
$(HOSTED)/archd_hosted.o: $(RT)/wubu_archd_daemon.c $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.c $(RT)/wubu_archd_fs.c
	$(CC) $(CFLAGS) -DWUBD_TEST_MAIN -I$(RT) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# holyd lifecycle without its daemon main (standalone apps own no archd/holyd
# daemon; this variant exposes wubu_holyd_lifecycle's helpers without main()).
$(RT)/holyd_lifecycle_app.o: $(RT)/wubu_holyd_lifecycle.c
	$(CC) $(CFLAGS) -DWUBD_TEST_MAIN -I$(RT) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# Full runtime objects for standalone GUI apps (paint/doom). Excludes the
# daemon main() objects (wubu_archd.o, wubu_holyd_lifecycle.o) and uses the
# -DWUBD_TEST_MAIN variants instead, plus the no-main archd_hosted.o.
APP_RT_OBJS = \
	$(RT)/container/wubucontainer.o $(RT)/container/wubucontainer_registry.o $(RT)/oci/oci_blob_store.o $(RT)/oci/oci_cleanup.o \
	$(RT)/oci/oci_convert.o $(RT)/oci/oci_descriptor.o $(RT)/oci/oci_hooks.o \
	$(RT)/oci/oci_http_client.o $(RT)/oci/oci_image_config.o $(RT)/oci/oci_image_index.o \
	$(RT)/oci/oci_image_manifest.o $(RT)/oci/oci_media_types.o $(RT)/oci/oci_registry.o \
	$(RT)/oci/oci_runtime_spec.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_server.o $(RT)/styxfs_util.o $(RT)/vsl/vsl.o \
	$(RT)/vsl/vsl_driver.o $(RT)/vsl/vsl_elf.o $(RT)/vsl/vsl_file.o $(RT)/vsl/vsl_gpu_vulkan.o \
	$(RT)/vsl/vsl_memory.o $(RT)/vsl/vsl_process.o $(RT)/vsl/vsl_shared.o $(RT)/vsl/vsl_syscall.o \
	$(RT)/vsl/vsl_syscall_fileio.o $(RT)/vsl/vsl_syscall_memory.o $(RT)/vsl/vsl_syscall_net.o \
	$(RT)/vsl/vsl_syscall_nt.o $(RT)/vsl/vsl_nt_atoms.o $(RT)/vsl/vsl_nt_job.o $(RT)/vsl/vsl_nt_io.o $(RT)/vsl/vsl_nt_vmem.o $(RT)/vsl/vsl_nt_process.o $(RT)/vsl/vsl_nt_thread.o $(RT)/vsl/vsl_nt_section.o $(RT)/vsl/vsl_nt_timer.o $(RT)/vsl/vsl_nt_sync.o $(RT)/vsl/vsl_nt_registry.o $(RT)/vsl/vsl_nt_token.o $(RT)/vsl/vsl_syscall_proc.o $(RT)/wubu_arch.o \
	$(RT)/wubu_archd_util.o $(RT)/wubu_container.o $(RT)/wubu_compat_db.o $(RT)/wubu_session.o $(RT)/wubu_ct_bwrap.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_ct_isolate_cgroup.o \
	$(RT)/wubu_exec.o $(RT)/wubu_exec_container.o $(RT)/wubu_exec_format.o \
	$(RT)/wubu_exec_macho.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_gc.o $(RT)/wubu_holyd.o \
	$(RT)/wubu_holyd_9p.o $(RT)/wubu_holyd_event.o $(RT)/wubu_holyd_exec.o $(RT)/wubu_holyd_input.o \
	$(RT)/wubu_holyd_save.o $(RT)/wubu_holyd_session.o $(RT)/wubu_holyd_window.o \
	$(RT)/wubu_host_exec.o $(RT)/wubu_image.o $(RT)/wubu_image_cache.o $(RT)/wubu_image_manifest.o \
	$(RT)/wubu_image_ops.o $(RT)/wubu_image_parse.o $(RT)/wubu_image_tar.o $(RT)/wubu_netlink.o \
	$(RT)/wubu_network.o $(RT)/wubu_network_cni.o $(RT)/wubu_network_create.o $(RT)/wubu_network_dns.o \
	$(RT)/wubu_network_fw.o $(RT)/wubu_network_qos.o $(RT)/wubu_network_svc.o $(RT)/wubu_network_ts.o \
	$(RT)/wubu_network_wg.o $(RT)/wubu_proton2_device.o \
	$(RT)/wubu_proton2_gamescope.o $(RT)/wubu_proton2_gpu.o $(RT)/wubu_proton2_launch.o \
	$(RT)/wubu_proton_dll.o $(RT)/wubu_proton_pe.o $(RT)/wubu_ramdisk.o $(RT)/wubu_ramdisk_format.o \
	$(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_copy.o $(RT)/wubu_snapshot_diff.o $(RT)/wubu_snapshot_fs.o \
	$(RT)/wubu_snapshot_gc.o $(RT)/wubu_snapshot_tag.o $(RT)/wubu_snapshot_xport.o $(RT)/wubu_spawn.o \
	$(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_archd_svc.o $(RT)/wubu_archd_fs.o \
	$(RT)/wubu_vsl.o $(HOSTED)/archd_hosted.o $(RT)/holyd_lifecycle_app.o

$(HOSTED)/primary-selection-private.o: $(HOSTED)/primary-selection-private.c
	$(CC) $(CFLAGS) -I$(HOSTED) -MMD -MP -c $< -o $@

$(HOSTED)/xdg-shell-private.o: $(HOSTED)/xdg-shell-private.code
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -MMD -MP -c $< -o $@

# Link-only target: reuses the .o files above. Edit one file -> one .o rebuild + relink.
hosted: $(HOSTED_OBJS) $(RT_OBJS)
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(BRIDGE) -I$(GUI) -I$(COMP) -I$(JIT) -I$(APPS) \
		$(HOSTED_OBJS) $(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_fs_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_archd_svc.o \
		-lwayland-client -lxkbcommon -lm -lsqlite3 -lzstd -lz -ldl -o $(HOSTED)/wubu
	@echo "✅ WuBuOS hosted binary built (./src/hosted/wubu)"
# ── Compilation Rules ────────────────────────────────────────────

$(KERNEL)/%.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/vsl -I$(RT)/oci -I$(BRIDGE) -MMD -MP -c $< -o $@

$(JIT)/%.o: $(JIT)/%.c
	$(CC) $(CFLAGS) -I$(JIT) -MMD -MP -c $< -o $@

$(GUI)/%.o: $(GUI)/%.c
	$(CC) $(CFLAGS) -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) `pkg-config --cflags wlroots vulkan gbm libdrm 2>/dev/null` -MMD -MP -c $< -o $@

$(BRIDGE)/%.o: $(BRIDGE)/%.c
	$(CC) $(CFLAGS) -I$(BRIDGE) -I$(KERNEL) -I$(WS) -MMD -MP -c $< -o $@

$(APPS)/%.o: $(APPS)/%.c
	$(CC) $(CFLAGS) -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(APPS)/calc/%.o: $(APPS)/calc/%.c
	$(CC) $(CFLAGS) -I$(APPS)/calc -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(APPS)/notepad/%.o: $(APPS)/notepad/%.c
	$(CC) $(CFLAGS) -I$(APPS)/notepad -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(APPS)/cmd/%.o: $(APPS)/cmd/%.c
	$(CC) $(CFLAGS) -I$(APPS)/cmd -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(APPS)/taskmgr/%.o: $(APPS)/taskmgr/%.c
	$(CC) $(CFLAGS) -I$(APPS)/taskmgr -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(APPS)/regedit/%.o: $(APPS)/regedit/%.c
	$(CC) $(CFLAGS) -I$(APPS)/regedit -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@

$(WS)/%.o: $(WS)/%.c
	$(CC) $(CFLAGS) -I$(WS) -I$(KERNEL) -MMD -MP -c $< -o $@

$(COMP)/%.o: $(COMP)/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# VSL submodule objects
$(RT)/vsl/%.o: $(RT)/vsl/%.c
	$(CC) $(CFLAGS) -DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# OCI submodule objects
$(RT)/oci/%.o: $(RT)/oci/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/oci -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# Container submodule objects
$(RT)/container/%.o: $(RT)/container/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/container -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

$(TOOLS)/%.o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) -I$(TOOLS) -MMD -MP -c $< -o $@

$(HOSTED)/%.o: $(HOSTED)/%.c
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -MMD -MP -c $< -o $@

# DRM/KMS backend: real libdrm engine is live (WUBU_USE_DRM), not dead stubs.
$(HOSTED)/wubu_metal_drm.o: $(HOSTED)/wubu_metal_drm.c
	$(CC) $(CFLAGS) -DWUBU_USE_DRM -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -MMD -MP -c $< -o $@

$(AUDIO)/%.o: $(AUDIO)/%.c
	$(CC) $(CFLAGS) -I$(AUDIO) -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@

$(BEAR)/%.o: $(BEAR)/%.c
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) -MMD -MP -c $< -o $@

# CUDA files
$(BEAR)/%.o: $(BEAR)/%.cu
	nvcc -std=c++17 -I$(BEAR) -I$(RT) -I$(KERNEL) -c $< -o $@

# Assembly files
$(KERNEL)/%.o: $(KERNEL)/%.S
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

$(KERNEL)/libc.o: $(KERNEL)/libc.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

# ── Tests ────────────────────────────────────────────────────────

# ── Tier-based Test Targets ─────────────────────────────────────────
# CRITICAL TIER: Runtime Core (containers, network, OCI, snapshots, VSL, HolyD, Proton)
test_critical_runtime: runtime test_oci test_network test_snapshot test_vsl test_holyd test_proton test_proton2 test_spawn
	@echo "✅ Critical Tier (Runtime Core) complete"

# CRITICAL TIER: Kernel / Metal (interrupt, FAT32, TXFS, AHCI, DRM, Vulkan)
test_critical_kernel: test_fat32 test_txfs test_ahci test_drm_direct
	@echo "✅ Critical Tier (Kernel/Metal) complete"

# HIGH TIER: Bridge (syscall bridge, DOS flip)
test_high_bridge: runtime test_bridge test_bridge_flip test_syscall
	@echo "✅ High Tier (Bridge) complete"

# HIGH TIER: Hosted / GUI (WM, desktop, startmenu, explorer, terminal, clipboard, compositor, shell)
test_high_gui: gui runtime test_dosgui_wm test_dosgui_dos_window test_dosgui_startmenu test_dosgui_explorer test_dosgui_term test_clipboard test_screenshot test_compositor test_dosgui_shell test_wallpaper test_control test_calc
	@echo "✅ High Tier (Hosted/GUI) complete"

# HIGH TIER: Bear RL / JIT / Compiler (JIT, memory, tasking, input, HolyC, PTX)
test_high_bear: test_jit test_memory test_tasking test_input test_holyc test_holyc_ptx
	@echo "✅ High Tier (Bear RL/JIT/Compiler) complete"

# MEDIUM/LOW TIER: Apps / Audio / Tools / WorldSim / OTHER
test_medium_other: runtime gui test_worldsim test_audio test_apps test_apps2 test_wubu test_host_exec test_gaad test_iso test_weights test_gc test_txfs test_dbuf test_styx test_styxfs test_anticheat test_bottles test_ns_bridge test_ns_snap test_ns_pkg test_ns_kernel test_ns_9p test_deploy test_daemon_panel test_math test_pkgmgr test_gamelib test_mime test_trash test_system test_launch test_compat test_shell test_cap test_txn test_cmd test_dos_emu_smoke test_manifest
	@echo "✅ Medium/Low Tier (Apps/Audio/Tools/Other) complete"

# Full test suite - runs all tiers sequentially
test: test_critical_runtime test_critical_kernel test_high_bridge test_high_gui test_high_bear test_medium_other
	@echo "✅ All tests passed (all tiers)"

test_jit:
	$(CC) -O0 -g -I$(JIT) -I$(RT) -I$(COMP) -Wno-format-truncation $(JIT_SRCS) $(RT)/wubu_spawn.c $(JIT)/jit_test.c -o $(JIT)/jit_test -ldl
	$(JIT)/jit_test

test_memory: $(KERNEL)/memory.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/memory.c $(KERNEL)/memory_test.c -o $(KERNEL)/memory_test
	$(KERNEL)/memory_test

test_tasking: $(KERNEL)/memory.o
	$(CC) $(CFLAGS) -DWUBU_BAREMETAL=0 -O0 -g -I$(KERNEL) $(KERNEL)/memory.c $(KERNEL)/tasking.c $(KERNEL)/tasking_test.c -o $(KERNEL)/tasking_test
	$(KERNEL)/tasking_test

test_input:
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/input.c $(KERNEL)/input_test.c -o $(KERNEL)/input_test
	$(KERNEL)/input_test

test_math: $(KERNEL)/wubu_math.o
	$(CC) -O0 -g -DWUBU_MATH_TEST -I$(KERNEL) $(KERNEL)/wubu_math.c -o $(KERNEL)/math_test
	$(KERNEL)/math_test

test_clipboard:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM -DWUBU_CLIPBOARD_TEST_MODE \
		-I$(GUI) -I$(KERNEL) \
		$(GUI)/wubu_clipboard.c $(GUI)/wubu_clipboard_mime.c $(GUI)/wubu_clipboard_test.c \
		-o $(GUI)/wubu_clipboard_test
	$(GUI)/wubu_clipboard_test

test_worldsim: $(KERNEL)/wubu_math.o
	$(CC) -Wall -Wextra -std=c11 -O0 -g -I$(WS) -I$(KERNEL) $(WS)/test_worldsim.c $(WS)/terrain.c $(WS)/entity.c $(WS)/physics.c $(WS)/render.c $(WS)/sim.c $(KERNEL)/wubu_math.o -o $(WS)/test_worldsim
	$(WS)/test_worldsim

test_fat32: $(KERNEL)/fat32.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/fat32.c $(KERNEL)/fat32_fat.c $(KERNEL)/fat32_dir.c $(KERNEL)/fat32_file.c $(KERNEL)/fat32_format.c $(KERNEL)/fat32_name.c $(KERNEL)/fat32_cluster.c $(KERNEL)/fat32_test.c -o $(KERNEL)/fat32_test
	$(KERNEL)/fat32_test

test_holyc: $(JIT_OBJS)
	$(CC) -O0 -g -I$(COMP) -I$(JIT) $(JIT_SRCS) $(RT)/wubu_spawn.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c $(COMP)/holyc_test.c -o $(COMP)/holyc_test -ldl
	$(COMP)/holyc_test

test_wubu: $(JIT_OBJS) $(RT)/wubu_host_exec.o $(RT)/styxfs_path.o $(RT)/styxfs_util.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/wubu_ct_isolate.o
	$(CC) -O0 -g -I$(RT) -I$(COMP) -I$(JIT) $(JIT_SRCS) $(RT)/wubu_spawn.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_exec_wasm.c $(RT)/wubu_exec_macho.c $(RT)/wubu_exec_dos.c $(RT)/wubu_exec_container.c $(RT)/wubu_exec_format.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_container_test.c $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_dos_proc.o -o $(RT)/wubu_container_test -ldl
	$(RT)/wubu_container_test

test_dos_emu: $(RT)/wubu_dos_emu.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/wubu_dos_emu.c $(RT)/wubu_dos_emu_mem.c $(RT)/wubu_dos_emu_regs.c $(RT)/wubu_dos_emu_alu.c $(RT)/wubu_dos_emu_int.c $(RT)/wubu_dos_emu_decode.c $(RT)/wubu_dos_emu_test.c -o $(RT)/wubu_dos_emu_test
	$(RT)/wubu_dos_emu_test
test_dos_emu_smoke: $(RT)/wubu_dos_emu.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/wubu_dos_emu.c $(RT)/wubu_dos_emu_mem.c $(RT)/wubu_dos_emu_regs.c $(RT)/wubu_dos_emu_alu.c $(RT)/wubu_dos_emu_int.c $(RT)/wubu_dos_emu_decode.c $(RT)/wubu_dos_emu_smoke.c -o $(RT)/wubu_dos_emu_smoke
	$(RT)/wubu_dos_emu_smoke

# Unified syscall manifest (load/resolve/cap-gate/emit). Runs from the repo
# root so it can load src/runtime/wubu_manifest/wubu_manifest.json by path.
test_manifest:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc/runtime/wubu_manifest \
		src/runtime/wubu_manifest/wubu_manifest.c \
		src/runtime/wubu_manifest/wubu_manifest_json.c \
		src/runtime/wubu_manifest/wubu_manifest_test.c -o $(RT)/wubu_manifest_test
	cd $(CURDIR) && $(RT)/wubu_manifest_test


test_dos_proc: $(RT)/wubu_container.o $(RT)/wubu_exec.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_container.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_ct_isolate_cgroup.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_path.o $(RT)/styxfs_util.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) -I$(HOSTED) \
			$(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_exec_dos.c \
			$(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) \
			$(RT)/wubu_exec_wasm.c $(RT)/wubu_exec_macho.c $(RT)/wubu_exec_format.c $(RT)/wubu_exec_container.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c \
		$(RT)/styxfs_server.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/wubu_dos_proc_test.c \
		-o $(RT)/wubu_dos_proc_test -ldl
	$(RT)/wubu_dos_proc_test
test_container_registry:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/container/wubucontainer.c $(RT)/container/wubucontainer_registry.c $(RT)/container/wubucontainer_test.c \
		-o $(RT)/container/wubucontainer_test -ljson-c
	$(RT)/container/wubucontainer_test

test_apps:
	$(CC) -O0 -g -I$(RT) -I$(COMP) -I$(JIT) -I$(RT)/vsl \
		$(RT)/wubu_pkg.c \
		$(RT)/vsl/vsl.c $(RT)/vsl/vsl_process.c \
		$(RT)/vsl/vsl_memory.c $(RT)/vsl/vsl_file.c $(RT)/vsl/vsl_driver.c \
		$(RT)/vsl/vsl_shared.c $(RT)/vsl/vsl_elf.c $(RT)/vsl/vsl_gpu_vulkan.c \
		$(RT)/wubu_proton.c $(RT)/wubu_proton_api.c $(RT)/wubu_proton_dll.c $(RT)/wubu_proton_pe.c $(RT)/wubu_dxvk_conf.c $(RT)/wubu_proton_dxvk.c $(RT)/wubu_apps_test.c -o $(RT)/wubu_apps_test -ldl -lvulkan
	$(RT)/wubu_apps_test

# Incremental object list for the VSL NT test: each .o builds via the
# $(RT)/vsl/%.o / $(RT)/%.o pattern rules ONLY when its .c changed, so
# re-running the test after a one-file edit is seconds, not a 7-minute
# single-TU recompile. (Previously the recipe fed 15 .c files to one $(CC)
# invocation, recompiling everything every time.)
VSL_NT_OBJS = \
	$(RT)/vsl/vsl.o $(RT)/vsl/vsl_syscall.o $(RT)/vsl/vsl_syscall_nt.o \
	$(RT)/vsl/vsl_nt_atoms.o $(RT)/vsl/vsl_nt_job.o $(RT)/vsl/vsl_nt_io.o \
	$(RT)/vsl/vsl_nt_vmem.o $(RT)/vsl/vsl_nt_process.o $(RT)/vsl/vsl_nt_thread.o \
	$(RT)/vsl/vsl_nt_section.o $(RT)/vsl/vsl_nt_timer.o $(RT)/vsl/vsl_nt_sync.o \
	$(RT)/vsl/vsl_nt_registry.o $(RT)/vsl/vsl_nt_token.o $(RT)/vsl/vsl_syscall_proc.o \
	$(RT)/vsl/vsl_syscall_fileio.o $(RT)/vsl/vsl_syscall_memory.o $(RT)/vsl/vsl_syscall_net.o \
	$(RT)/vsl/vsl_process.o $(RT)/vsl/vsl_memory.o $(RT)/vsl/vsl_file.o \
	$(RT)/vsl/vsl_driver.o $(RT)/vsl/vsl_shared.o $(RT)/vsl/vsl_elf.o \
	$(RT)/vsl/vsl_gpu_vulkan.o $(RT)/wubu_fs_util.o $(RT)/vsl/vsl_syscall_nt_test.o

# Same incremental-linking fix as test_vsl_nt: build each .o via the pattern
# rules so a one-file edit recompiles only that file (seconds, not minutes).
VSL_OBJS = \
	$(RT)/vsl/vsl.o $(RT)/vsl/vsl_syscall.o $(RT)/vsl/vsl_syscall_proc.o \
	$(RT)/vsl/vsl_syscall_fileio.o $(RT)/vsl/vsl_syscall_memory.o \
	$(RT)/vsl/vsl_syscall_net.o $(RT)/vsl/vsl_process.o $(RT)/vsl/vsl_memory.o \
	$(RT)/vsl/vsl_file.o $(RT)/vsl/vsl_driver.o $(RT)/vsl/vsl_shared.o \
	$(RT)/vsl/vsl_elf.o $(RT)/vsl/vsl_gpu_vulkan.o $(RT)/wubu_vsl_test.o

test_vsl: $(VSL_OBJS)
	$(CC) -O0 -g -D_GNU_SOURCE -DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl \
		$(VSL_OBJS) \
		-o $(RT)/wubu_vsl_test -ldl -lvulkan -lcuda
	$(RT)/wubu_vsl_test

test_vsl_nt: $(VSL_NT_OBJS)
	$(CC) -O0 -g -D_GNU_SOURCE -DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl \
		$(VSL_NT_OBJS) \
		-o $(RT)/wubu_vsl_nt_test -ldl -lvulkan -lcuda
	$(RT)/wubu_vsl_nt_test

test_bridge:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(BRIDGE) -I$(KERNEL) -I$(WS) \
		$(KERNEL)/vbe.c $(KERNEL)/wubu_math.c $(BRIDGE)/vbe_ws_bridge.c \
		$(WS)/terrain.c $(WS)/entity.c $(WS)/physics.c $(WS)/render.c $(WS)/sim.c \
		$(BRIDGE)/vbe_ws_bridge_test.c -o $(BRIDGE)/vbe_ws_bridge_test
	$(BRIDGE)/vbe_ws_bridge_test

test_bridge_flip:
	$(CC) -O0 -g -std=c11 -I$(BRIDGE) $(BRIDGE)/bridge.c $(BRIDGE)/bridge_test.c -o $(BRIDGE)/bridge_test
	$(BRIDGE)/bridge_test

test_syscall:
	$(CC) -O0 -g -std=c11 -D_GNU_SOURCE -I$(BRIDGE) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(COMP) -I$(JIT) -DMYSEED_METAL \
		$(JIT_SRCS) $(RT)/wubu_spawn.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c \
		$(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_exec_wasm.c $(RT)/wubu_exec_macho.c $(RT)/wubu_exec_dos.c $(RT)/wubu_exec_container.c $(RT)/wubu_exec_format.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
					$(WUBU_DOS_EMU_OBJS) $(RT)/wubu_dos_proc.o \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/vsl/vsl.c $(RT)/vsl/vsl_syscall.c $(RT)/vsl/vsl_syscall_proc.c $(RT)/vsl/vsl_syscall_fileio.c $(RT)/vsl/vsl_syscall_memory.c $(RT)/vsl/vsl_syscall_net.c $(RT)/vsl/vsl_process.c $(RT)/vsl/vsl_memory.o $(RT)/vsl/vsl_file.o $(RT)/vsl/vsl_driver.o $(RT)/vsl/vsl_shared.o $(RT)/vsl/vsl_elf.o $(RT)/vsl/vsl_gpu_vulkan.o \
		$(BRIDGE)/wubu_syscall.c $(BRIDGE)/wubu_syscall_vbe.c $(BRIDGE)/wubu_syscall_test.c \
		-o $(BRIDGE)/wubu_syscall_test -lvulkan
	$(BRIDGE)/wubu_syscall_test

test_proton:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/wubu_proton.c $(RT)/wubu_proton_api.c $(RT)/wubu_proton_dll.c $(RT)/wubu_proton_pe.c $(RT)/wubu_dxvk_conf.c $(RT)/wubu_proton_dxvk.c $(RT)/wubu_proton_test.c -o $(RT)/wubu_proton_test
	$(RT)/wubu_proton_test

test_launch: $(RT)/wubu_container.o $(RT)/wubu_proton.o $(RT)/wubu_proton_dll.o $(RT)/wubu_proton_pe.o $(RT)/wubu_dxvk_conf.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_ct_isolate_cgroup.o $(RT)/wubu_host_exec.o $(RT)/wubu_session.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_path.o $(RT)/styxfs_util.o $(RT)/wubu_launch_test.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) -I$(HOSTED) \
		$(RT)/wubu_container.c $(RT)/wubu_proton.c $(RT)/wubu_proton_api.c $(RT)/wubu_proton_dll.c $(RT)/wubu_proton_pe.c $(RT)/wubu_dxvk_conf.c $(RT)/wubu_proton_dxvk.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_session.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/wubu_launch_test.c \
		-o $(RT)/wubu_launch_test -ldl
	$(RT)/wubu_launch_test

test_compat:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) -I$(HOSTED) \
		$(RT)/wubu_compat_db.c $(RT)/wubu_compat_db_test.c \
		-o $(RT)/wubu_compat_db_test
	$(RT)/wubu_compat_db_test

test_shell: $(SHELL_DIR)/wubu_shell.o $(SHELL_DIR)/wubu_shell_history.o \
            $(SHELL_DIR)/wubu_shell_complete.o $(SHELL_DIR)/wubu_shell_exec.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_SHELL_NO_MAIN \
		-I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) \
		$(SHELL_DIR)/wubu_shell.c $(SHELL_DIR)/wubu_shell_history.c \
		$(SHELL_DIR)/wubu_shell_complete.c $(SHELL_DIR)/wubu_shell_exec.c \
		$(SHELL_DIR)/wubu_shell_test.c -o $(SHELL_DIR)/wubu_shell_test
	$(SHELL_DIR)/wubu_shell_test

test_ahci:
	$(CC) -O0 -g -std=c11 -I$(KERNEL) $(KERNEL)/ahci.c $(KERNEL)/ahci_test.c -o $(KERNEL)/ahci_test
	$(KERNEL)/ahci_test

test_iso:
	$(CC) -O0 -g -std=c11 -I$(TOOLS) $(TOOLS)/iso9660.c $(TOOLS)/iso9660_test.c -o $(TOOLS)/iso9660_test
	$(TOOLS)/iso9660_test

test_weights:
	$(CC) -O0 -g -std=c11 -I$(TOOLS) $(TOOLS)/weight_check.c $(TOOLS)/weight_check_test.c -o $(TOOLS)/weight_check_test
	$(TOOLS)/weight_check_test

test_gc:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/wubu_gc.c $(RT)/wubu_gc_test.c -o $(RT)/wubu_gc_test
	$(RT)/wubu_gc_test

test_screenshot:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_NO_LIBM \
		-I$(TOOLS) -I$(KERNEL) -I$(GUI) \
		$(TOOLS)/screenshot.c $(KERNEL)/vbe.c $(GUI)/wubu_wm.c $(GUI)/wubu_theme.c $(KERNEL)/wubu_gaad.c $(KERNEL)/wubu_math.c \
		$(TOOLS)/screenshot_test.c \
		-o $(TOOLS)/screenshot_test
	$(TOOLS)/screenshot_test

test_gui_screenshot: $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_NO_LIBM -DWUBU_SCREENSHOT_WITH_WM \
		-I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) -I$(BRIDGE) -I$(COMP) \
		$(GUI)/wubu_screenshot.c $(GUI)/wubu_screenshot_draw.c $(KERNEL)/vbe.c $(GUI)/wubu_theme.c $(GUI)/wubu_notify.c \
$(GUI)/dosgui_wm.c $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/wubu_wm.c $(HOSTED)/hosted.c $(HOSTED)/hosted_wayland.c $(HOSTED)/hosted_wayland_shm.o $(HOSTED)/hosted_wayland_input.o $(HOSTED)/hosted_wayland_surface.o $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(KERNEL)/memory.c $(KERNEL)/input.c $(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(KERNEL)/isr_stubs.S \
		$(BRIDGE)/bridge.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(APPS)/repl.c $(APPS)/dosgui_apps.c $(APPS)/app_canvas.c $(JIT_SRCS) $(RT)/wubu_spawn.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_bwrap.c $(RT)/wubu_container.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(HOSTED)/xdg-shell-private.o $(HOSTED)/primary-selection-private.o \
		$(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c \
		$(GUI)/wubu_screenshot_test.c \
		$(GUI)/wubu_screenshot_test -lwayland-client -lxkbcommon -lm
	$(GUI)/wubu_screenshot_test

# Clipboard regression: wubu_screenshot.c with its 4 GUI external refs
# stubbed in the test TU, so we verify real PNG clipboard encode without
# the full WM/Wayland stack.
test_screenshot_clipboard:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(TOOLS) -I$(RT) -I$(BRIDGE) -I$(COMP) -I$(APPS) -I$(HOSTED) \
		$(GUI)/wubu_screenshot.c $(GUI)/wubu_screenshot_draw.c $(GUI)/wubu_screenshot_png.c $(GUI)/wubu_screenshot_clipboard_test.c \
		-o $(GUI)/wubu_screenshot_clipboard_test -lz -lm
	$(GUI)/wubu_screenshot_clipboard_test

test_mime:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) \
		$(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c $(GUI)/wubu_theme.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c \
		$(GUI)/wubu_mime_test.c \
		-o $(GUI)/wubu_mime_test
	$(GUI)/wubu_mime_test

test_trash:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(RT) \
		$(GUI)/wubu_trash.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c $(GUI)/wubu_theme.c \
		$(RT)/wubu_arch.c \
		$(GUI)/wubu_trash_test.c \
		-o $(GUI)/wubu_trash_test
	$(GUI)/wubu_trash_test

test_gamelib:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(RT) \
		$(GUI)/wubu_gamelib.c $(GUI)/wubu_gamelib_startmenu.c $(GUI)/wubu_gamelib_playtime.c $(GUI)/wubu_gamelib_config.c $(GUI)/wubu_gamelib_scan.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c $(GUI)/wubu_theme.c $(GUI)/wubu_proton.c $(RT)/wubu_proton_api.c $(RT)/wubu_proton_dll.c $(GUI)/wubu_proton_util.c $(GUI)/wubu_proton_dxvk.c $(RT)/wubu_dxvk_conf.c $(RT)/wubu_proton_dxvk.o $(GUI)/wubu_proton_exec.c $(GUI)/wubu_proton_config.c $(RT)/wubu_fs_util.c \
		$(GUI)/wubu_gamelib_test.c \
		-o $(GUI)/wubu_gamelib_test
	$(GUI)/wubu_gamelib_test

test_txfs:
	$(CC) -O0 -g -std=c11 -I$(KERNEL) $(KERNEL)/txfs.c $(KERNEL)/txfs_test.c -o $(KERNEL)/txfs_test
	$(KERNEL)/txfs_test

test_dbuf:
	$(CC) -O0 -g -std=c11 -I$(GUI) $(GUI)/gui_dbuf.c $(GUI)/gui_dbuf_test.c -o $(GUI)/gui_dbuf_test
	$(GUI)/gui_dbuf_test

test_dosgui_wm: $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -D_POSIX_C_SOURCE=200809L -I$(GUI) -I$(KERNEL) -I$(COMP) -I$(JIT) -I$(HOSTED) \
$(GUI)/dosgui_wm.c $(GUI)/dosgui_wm_window.c $(GUI)/dosgui_wm_input.c $(GUI)/dosgui_wm_clock.c $(GUI)/dosgui_wm_ctxmenu_engine.c $(GUI)/dosgui_wm_window_state.c $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_holyc_term.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_desktop.c \
		$(GUI)/wubu_wallpaper.c $(GUI)/wubu_theme.c $(GUI)/dosgui_wm_test_stub.c \
		$(KERNEL)/vbe.c $(GUI)/wubu_notify.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.c $(GUI)/wubu_settings_io.c $(GUI)/wubu_json.c $(GUI)/wubu_trash.c \
		$(COMP)/holyc_codegen.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_lexer.c $(JIT_SRCS) $(RT)/wubu_spawn.c \
		$(RT)/wubu_session.c $(RT)/wubu_compat_db.c $(RT)/wubu_container.c $(RT)/wubu_arch.c \
		$(GUI)/dosgui_wm_test.c -o $(GUI)/dosgui_wm_test -lm
	$(GUI)/dosgui_wm_test

test_wallpaper:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) \
		$(GUI)/wubu_wallpaper.c $(GUI)/wubu_wallpaper_test.c -o $(GUI)/wubu_wallpaper_test
	$(GUI)/wubu_wallpaper_test

test_dosgui_dos_window: $(RT)/wubu_container.o $(RT)/wubu_exec.o $(RT)/wubu_exec_dos.o $(RT)/wubu_dos_proc.o $(RT)/wubu_dos_emu.o $(RT)/wubu_exec_wasm.o $(RT)/wubu_exec_macho.o $(RT)/wubu_exec_container.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_isolate.o $(RT)/wubu_ct_isolate_cgroup.o $(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o $(RT)/styxfs_path.o $(RT)/styxfs_util.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(GUI) -I$(RT) -I$(COMP) -I$(KERNEL) -I$(JIT) -I$(HOSTED) \
		$(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_exec_dos.c \
		$(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) $(RT)/wubu_exec_wasm.c $(RT)/wubu_exec_macho.c $(RT)/wubu_exec_format.c $(RT)/wubu_exec_container.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c \
		$(RT)/styxfs_server.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(GUI)/dosgui_dos_window.c $(GUI)/dosgui_dos_window_test.c $(GUI)/dosgui_dos_window_test_stub.c $(RT)/wubu_hc_eval_stub.c \
		-o $(GUI)/dosgui_dos_window_test -ldl
	$(GUI)/dosgui_dos_window_test
test_cmd:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -I$(APPS)/cmd -I$(KERNEL) -I$(GUI) -I$(RT) \
		$(APPS)/cmd/cmd.c $(APPS)/cmd/cmd_test.c $(APPS)/cmd/cmd_test_stub.c \
		-o $(APPS)/cmd/cmd_test -lutil
	$(APPS)/cmd/cmd_test


test_calc:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) -I$(APPS) -I$(COMP) \
		$(APPS)/calc/calc.c $(APPS)/calc/calc_math.c $(APPS)/calc/calc_test_stub.c $(APPS)/calc/calc_test.c \
		-o $(APPS)/calc/calc_test -lm
	$(APPS)/calc/calc_test

test_dosgui_startmenu:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/dosgui_startmenu.c $(GUI)/dosgui_startmenu_db.c $(GUI)/dosgui_startmenu_search.c $(GUI)/dosgui_startmenu_tree.c $(GUI)/dosgui_startmenu_power.c $(GUI)/wubu_theme.c $(GUI)/dosgui_startmenu_test_stub.c $(GUI)/dosgui_startmenu_test.c $(KERNEL)/vbe.c -o $(GUI)/dosgui_startmenu_test
	$(GUI)/dosgui_startmenu_test

test_dosgui_explorer:
		$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -I$(GUI) -I$(KERNEL) -I$(RT) $(GUI)/dosgui_explorer.c $(GUI)/dosgui_explorer_input.c $(GUI)/dosgui_explorer_ops.c $(GUI)/dosgui_explorer_info.c $(GUI)/dosgui_explorer_zip.c $(GUI)/dosgui_explorer_format.c $(GUI)/dosgui_explorer_drives.c $(GUI)/dosgui_explorer_tree.c $(GUI)/dosgui_explorer_preview.c $(GUI)/dosgui_explorer_render.c $(GUI)/dosgui_explorer_fs.c $(GUI)/dosgui_explorer_fsops.c $(GUI)/wubu_theme.c $(GUI)/dosgui_explorer_test_stub.c $(KERNEL)/vbe.c $(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c $(GUI)/dosgui_explorer_test.c -o $(GUI)/dosgui_explorer_test
	$(GUI)/dosgui_explorer_test
test_dosgui_term:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/dosgui_term.c $(GUI)/dosgui_term_tabs.c $(GUI)/dosgui_term_render.c $(GUI)/dosgui_term_ansi.c $(GUI)/dosgui_term_pty.c $(GUI)/wubu_theme.c $(GUI)/dosgui_term_test_stub.c $(KERNEL)/vbe.c $(GUI)/dosgui_term_test.c -o $(GUI)/dosgui_term_test
	$(GUI)/dosgui_term_test
test_styx:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styx_test.c -o $(RT)/styx_test
	$(RT)/styx_test

test_styxfs:
	$(CC) $(CFLAGS) -O0 -g -std=c11 -I$(RT) -I$(COMP) -I$(JIT) $(JIT_SRCS) $(RT)/wubu_spawn.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_exec_wasm.c $(RT)/wubu_exec_macho.c $(RT)/wubu_exec_dos.c $(RT)/wubu_exec_container.c $(RT)/wubu_exec_format.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/styxfs_test.c $(RT)/wubu_dos_proc.o $(WUBU_DOS_EMU_OBJS) -o $(RT)/styxfs_test
	$(RT)/styxfs_test

HOSTED_TEST_OBJS = $(HOSTED_OBJS:$(HOSTED)/hosted.o=)
# Mirrors the `hosted` link model (HOSTED_TEST_OBJS + the SAME RT fragments the
# hosted recipe pulls) so every symbol is defined exactly once. We do NOT link
# full $(RT_OBJS) — it duplicates dozens of objects already in HOSTED_TEST_OBJS
# (styx/exec/container/oci_http_client/...) and would cause `multiple definition`.
test_hosted: $(HOSTED)/xdg-shell-private.o $(HOSTED)/primary-selection-private.o $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_HOSTED_TEST -DWUBD_TEST_MAIN -I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(BRIDGE) -I$(GUI) -I$(COMP) -I$(JIT) -I$(APPS) \
		$(HOSTED_TEST_OBJS) $(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_fs_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_archd_svc.o \
		$(HOSTED)/hosted_test.c $(HOSTED)/hosted.c \
		-lwayland-client -lxkbcommon -lm -lsqlite3 -lzstd -lz -ldl -lpthread -ljson-c -lvulkan -o $(HOSTED)/hosted_test
	$(HOSTED)/hosted_test

test_host_exec:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c $(RT)/wubu_container.c $(RT)/wubu_host_exec_test.c \
		-o $(RT)/wubu_host_exec_test
	$(RT)/wubu_host_exec_test

test_arch:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(APPS) \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_container.c $(RT)/wubu_arch.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		-o $(RT)/wubu_arch_test
	$(RT)/wubu_arch_test

test_ramdisk:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(APPS) \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_container.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c $(RT)/wubu_arch.c $(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_ramdisk_test.c \
		-o $(RT)/wubu_ramdisk_test
	$(RT)/wubu_ramdisk_test

test_gaad:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(KERNEL) \
		$(KERNEL)/wubu_gaad.c $(KERNEL)/wubu_math.c $(KERNEL)/wubu_gaad_test.c \
		-o $(KERNEL)/wubu_gaad_test
	$(KERNEL)/wubu_gaad_test

test_wubu_wm:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(RT) \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(GUI)/wubu_theme.c $(GUI)/wubu_wm.c \
		$(KERNEL)/wubu_gaad.c $(KERNEL)/wubu_math.c $(KERNEL)/vbe.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(GUI)/wubu_wm_test.c \
		-o $(GUI)/wubu_wm_test
	$(GUI)/wubu_wm_test

# PTX Backend test
test_holyc_ptx:
	$(CC) -O0 -g -I$(COMP) -I$(JIT) \
		$(COMP)/holyc_ptx.c \
		$(COMP)/test_holyc_ptx.c \
		-o $(COMP)/test_holyc_ptx
	$(COMP)/test_holyc_ptx

# Cell 388/389: Direct DRM/KMS (no libdrm) + custom GBM test
test_drm_direct:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(HOSTED) -I$(RT) -I/usr/include/drm \
		$(HOSTED)/wubu_display.c $(HOSTED)/wubu_gbm.c \
		$(HOSTED)/wubu_display_test.c \
		-o $(HOSTED)/wubu_drm_direct_test
	$(HOSTED)/wubu_drm_direct_test

test_apps2:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(APPS) -I$(KERNEL) -I$(RT) \
$(APPS)/wubu_editor.c $(APPS)/wubu_editor_bookmark.c $(APPS)/wubu_editor_macro.c $(APPS)/wubu_editor_undo.c $(APPS)/wubu_editor_selection.c $(APPS)/wubu_editor_find.c $(APPS)/wubu_canvas_layers.c $(APPS)/wubu_canvas_draw.c $(APPS)/wubu_canvas_filter.c $(APPS)/wubu_canvas_transform.c $(APPS)/wubu_canvas_plugin.c $(APPS)/wubu_canvas_undo.c $(APPS)/wubu_canvas_blend.c $(APPS)/wubu_canvas_io.c $(APPS)/wubu_image_codec.c $(APPS)/wubu_canvas_io_ppm.c $(APPS)/wubu_codec.c $(RT)/wubu_spawn.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_container.c \
		$(APPS)/wubu_apps2_test.c \
		-o $(APPS)/wubu_apps2_test -lz -lm
	$(APPS)/wubu_apps2_test

test_control: $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -D_POSIX_C_SOURCE=200809L -I$(GUI) -I$(KERNEL) -I$(COMP) -I$(JIT) -I$(APPS) -I$(APPS)/control -I$(HOSTED) -I$(RT) \
$(GUI)/dosgui_wm.c $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_holyc_term.c \
		$(GUI)/wubu_wallpaper.c $(GUI)/wubu_theme.c $(GUI)/dosgui_wm_test_stub.c \
			$(KERNEL)/vbe.c $(GUI)/wubu_notify.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c $(GUI)/wubu_trash.c \
		$(COMP)/holyc_codegen.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_lexer.c $(JIT_SRCS) $(RT)/wubu_spawn.c \
		$(RT)/wubu_session.c $(RT)/wubu_compat_db.c $(RT)/wubu_container.c $(RT)/wubu_arch.c \
		$(APPS)/control/control.c \
		$(APPS)/control_test.c -o $(APPS)/control_test -lm
	$(APPS)/control_test

test_proton2:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_proton2.c $(RT)/wubu_proton2_launch.c $(RT)/wubu_proton2_gamescope.c $(RT)/wubu_proton2_device.c $(RT)/wubu_proton2_gpu.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c $(RT)/wubu_arch.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_container.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_proton2_test.c \
		-o $(RT)/wubu_proton2_test
	$(RT)/wubu_proton2_test

test_metal:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DMYSEED_METAL -DWUBU_NO_LIBM -DWUBU_HOSTED_TEST -DWUBU_USE_DRM\
		-I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -I$(SHELL_DIR) -I$(COMP) -I$(JIT) -I/usr/include/libdrm \
		$(HOSTED)/wubu_metal.c $(HOSTED)/wubu_metal_evdev.c $(HOSTED)/wubu_metal_x11.c $(HOSTED)/wubu_metal_vulkan.c $(HOSTED)/wubu_metal_drm.c $(HOSTED)/wubu_metal_audio.c $(KERNEL)/wubu_gaad.c $(KERNEL)/wubu_math.c \
		$(KERNEL)/memory.c $(KERNEL)/vbe.c $(KERNEL)/input.c $(KERNEL)/interrupt.c $(KERNEL)/interrupt_apic.c $(KERNEL)/interrupt_pit.c $(KERNEL)/interrupt_syscall.c $(KERNEL)/interrupt_timer.c $(KERNEL)/isr_stubs.S $(KERNEL)/tasking.c $(KERNEL)/tasking_switch.S \
		$(HOSTED)/wubu_metal_test.c \
		-o $(HOSTED)/wubu_metal_test -lm -ldrm -ldl
	$(HOSTED)/wubu_metal_test

test_audio:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(AUDIO) -I$(KERNEL) -I$(RT) \
		$(AUDIO)/wubu_audio_engine.c $(AUDIO)/wubu_audio_chips.c $(AUDIO)/wubu_audio_furnace.c $(AUDIO)/wubu_audio_sf2.c $(AUDIO)/wubu_audio_daw.c $(AUDIO)/wubu_audio.c $(KERNEL)/wubu_math.c \
		$(AUDIO)/wubu_audio_test.c \
		-o $(AUDIO)/wubu_audio_test -lpthread
	$(AUDIO)/wubu_audio_test

test_deploy:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) \
		$(GUI)/wubu_deploy.c $(GUI)/wubu_deploy_util.c $(GUI)/wubu_deploy_gen.c $(GUI)/wubu_deploy_config.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c $(GUI)/wubu_theme.c \
		$(GUI)/wubu_deploy_test.c \
		-o $(GUI)/wubu_deploy_test

test_bear_opt:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(BEAR) \
		$(BEAR)/bear_arena.c $(BEAR)/bear_opt.c $(BEAR)/bear_opt_test.c \
		-o $(BEAR)/bear_opt_test -lm
	$(BEAR)/bear_opt_test

test_pkgmgr:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(RT) \
		$(GUI)/wubu_pkgmgr.c $(GUI)/wubu_pkgmgr_verify.c $(GUI)/wubu_pkgmgr_resolve.c $(GUI)/wubu_pkgmgr_manifest.c $(GUI)/wubu_pkgmgr_db.c $(GUI)/wubu_pkgmgr_pkg.c $(GUI)/wubu_pkgmgr_install.c $(GUI)/wubu_pkgmgr_txn.c $(GUI)/wubu_pkgmgr_remote.c $(GUI)/wubu_settings.c $(GUI)/wubu_settings_defaults.o $(GUI)/wubu_settings_io.o $(GUI)/wubu_json.c $(GUI)/wubu_theme.c \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c $(RT)/wubu_container.c \
		$(RT)/oci/oci_http_client.c \
		$(GUI)/wubu_pkgmgr_test.c \
		-o $(GUI)/wubu_pkgmgr_test -lsqlite3 -lzstd
	$(GUI)/wubu_pkgmgr_test

test_dosgui_apps: $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(APPS) -I$(KERNEL) -I$(GUI) \
		$(APPS)/dosgui_apps.c $(APPS)/app_canvas.c \
		$(APPS)/calc/calc.c $(APPS)/calc/calc_math.c $(APPS)/notepad/notepad.c $(APPS)/wubu_canvas_layers.c $(APPS)/wubu_canvas_draw.c $(APPS)/wubu_canvas_filter.c $(APPS)/wubu_canvas_transform.c $(APPS)/wubu_canvas_plugin.c $(APPS)/wubu_canvas_undo.c $(APPS)/wubu_canvas_blend.c $(APPS)/wubu_canvas_io.c $(APPS)/wubu_image_codec.c $(APPS)/wubu_canvas_io_ppm.c $(APPS)/wubu_codec.c \
		$(APPS)/taskmgr/taskmgr.c $(APPS)/regedit/regedit.c $(APPS)/fm/fm.c \
		$(APPS)/repl/repl.c $(APPS)/control/control.c $(APPS)/editor/editor.c \
		$(KERNEL)/vbe.c $(KERNEL)/memory.c $(KERNEL)/wubu_math.c $(KERNEL)/interrupt.c $(KERNEL)/isr_stubs.S $(KERNEL)/tasking.c $(KERNEL)/tasking_switch.S \
		$(KERNEL)/wubu_gaad.c $(KERNEL)/input.c \
	$(GUI)/wubu_theme.c $(GUI)/dosgui_wm.c $(GUI)/dosgui_wm_window.o $(GUI)/dosgui_wm_input.o $(GUI)/dosgui_wm_clock.o $(GUI)/dosgui_wm_ctxmenu_engine.o $(GUI)/dosgui_wm_window_state.o $(GUI)/dosgui_wm_layout.c $(GUI)/dosgui_wm_render.c $(GUI)/dosgui_wm_taskbar.c $(GUI)/dosgui_wm_desktop.c $(GUI)/dosgui_wm_icons.c $(GUI)/dosgui_wm_systray.c $(GUI)/dosgui_wm_ctxmenu.c $(GUI)/dosgui_wm_holyc_term.c \
		$(APPS)/dosgui_apps_test.c \
		-o $(APPS)/dosgui_apps_test -lm
	$(APPS)/dosgui_apps_test

test_anticheat:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) \
		$(RT)/wubu_anticheat.c $(RT)/wubu_anticheat_test.c \
		-o $(RT)/wubu_anticheat_test
	$(RT)/wubu_anticheat_test

test_bottles:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) \
		$(RT)/wubu_bottle_lifecycle.c $(RT)/wubu_bottle_serialize.o $(RT)/wubu_bottle_io.c $(RT)/wubu_bottle_flatpak.c $(RT)/wubu_bottle_ops.c $(RT)/wubu_bottles_json.c $(RT)/wubu_bottles_fs.c $(RT)/wubu_bottles_test.c $(RT)/wubu_fs_util.c \
		-o $(RT)/wubu_bottles_test
	$(RT)/wubu_bottles_test
test_ns_bridge:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -DWUBD_TEST_MAIN \
		-I$(RT) \
		$(RT)/wubu_ns_fs.c $(RT)/wubu_ns_bridge.c \
		$(RT)/wubu_archd_daemon.c $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.c $(RT)/wubu_archd_util.c $(RT)/wubu_archd_fs.c \
		$(RT)/wubu_arch.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c $(RT)/wubu_fs_util.c \
		$(RT)/wubu_container.c $(RT)/container/wubucontainer_registry.c \
		$(RT)/wubu_bottle_lifecycle.c $(RT)/wubu_bottle_serialize.o $(RT)/wubu_bottle_io.c $(RT)/wubu_bottle_flatpak.c $(RT)/wubu_bottle_ops.c \
		$(RT)/wubu_bottles_json.c $(RT)/wubu_bottles_fs.c \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/wubu_ns_bridge_test.c \
		-o $(RT)/wubu_ns_bridge_test
	$(RT)/wubu_ns_bridge_test

test_ns_snap:
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_snap_test.c -o /tmp/wubu_ns_snap_test.o
	$(CC) -O0 -no-pie \
		$(RT)/wubu_ns_fs.o $(RT)/wubu_ns_snap.o \
		$(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_fs.o $(RT)/wubu_snapshot_copy.o \
		$(RT)/wubu_fs_util.o \
		/tmp/wubu_ns_snap_test.o \
		-o $(RT)/wubu_ns_snap_test
	$(RT)/wubu_ns_snap_test


test_ns_pkg:
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_pkg_test.c -o /tmp/wubu_ns_pkg_test.o
	$(CC) -O0 -no-pie \
		$(RT)/wubu_ns_fs.o $(RT)/wubu_ns_pkg.o $(RT)/wubu_pkg.o \
		/tmp/wubu_ns_pkg_test.o \
		-o $(RT)/wubu_ns_pkg_test
	$(RT)/wubu_ns_pkg_test


test_ns_kernel:
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_kernel_test.c -o /tmp/wubu_ns_kernel_test.o
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_kernel.c -o $(RT)/wubu_ns_kernel.o
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_fs.c -o $(RT)/wubu_ns_fs.o
	$(CC) -O0 -no-pie \
		$(RT)/wubu_ns_fs.o $(RT)/wubu_ns_kernel.o \
		/tmp/wubu_ns_kernel_test.o \
		-o $(RT)/wubu_ns_kernel_test
	$(RT)/wubu_ns_kernel_test


test_ns_9p:
	$(CC) -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation -I$(RT) -c $(RT)/wubu_ns_9p_test.c -o /tmp/wubu_ns_9p_test.o
	$(CC) -O0 -no-pie \
		$(RT)/wubu_ns_fs.o $(RT)/wubu_ns_snap.o \
		$(RT)/wubu_snapshot.o $(RT)/wubu_snapshot_fs.o $(RT)/wubu_snapshot_copy.o \
		$(RT)/wubu_fs_util.o \
		$(RT)/styx_names.o $(RT)/styx_enc.o $(RT)/styx_serve.o $(RT)/styx_parse.o $(RT)/styx_fid.o \
		$(RT)/styxfs_server.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o \
		$(RT)/styxfs_path.o $(RT)/styxfs_host.o $(RT)/styxfs_util.o $(RT)/styxfs_vfs.o $(RT)/wubu_container.o $(RT)/container/wubucontainer.o $(RT)/container/wubucontainer_registry.o \
		/tmp/wubu_ns_9p_test.o \
		-ljson-c -o $(RT)/wubu_ns_9p_test
	$(RT)/wubu_ns_9p_test


















# ── Daemon Tests ──────────────────────────────────────────────────

test_archd:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation \
		-I$(RT) \
		-DWUBD_TEST_MAIN \
		$(RT)/wubu_archd_daemon.c $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.c $(RT)/wubu_archd_util.c $(RT)/wubu_archd_fs.c $(RT)/wubu_arch.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c $(RT)/wubu_fs_util.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_container.c \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(RT)/wubu_archd_test.c \

		-o $(RT)/wubd_archd_test -lpthread
	$(RT)/wubd_archd_test

test_holyd: $(RT)/wubu_holyd_repl.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) -I$(GUI) \
		-DWUBD_TEST_MAIN \
		$(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c \
		$(JIT_SRCS) $(RT)/wubu_spawn.c \
		$(RT)/wubu_holyd.c $(RT)/wubu_holyd_session.c $(RT)/wubu_holyd_exec.c $(RT)/wubu_holyd_repl.o $(RT)/wubu_holyd_window.c $(RT)/wubu_holyd_input.c $(RT)/wubu_holyd_9p.c $(RT)/wubu_holyd_save.c $(RT)/wubu_holyd_event.c $(RT)/wubu_holyd_lifecycle.c \
		$(RT)/wubu_holyd_test.c \
		$(GUI)/dosgui_wm_test_stub.c \
		-o $(RT)/wubd_holyd_test -lpthread
	$(RT)/wubd_holyd_test

# ── wubu_holyd REPL binary (E4: embeddable HolyC REPL) ─────────────
# Built WITHOUT -DWUBD_TEST_MAIN so wubu_holyd_lifecycle.c's main is the
# entry point. `wubu_holyd --repl` runs an interactive TTY HolyC REPL that
# the Desktop terminal embeds as the HolyC tab.
wubu_holyd_bin:
	$(CC) -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation \
		-I$(RT) -I$(COMP) -I$(JIT) -I$(GUI) \
		$(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_parse_ast.c $(COMP)/holyc_codegen.c $(COMP)/holyc_codegen_emit.c $(COMP)/holyc_codegen_expr.c $(COMP)/holyc_codegen_stmt.c $(COMP)/holyc_codegen_api.c \
		$(JIT_SRCS) $(RT)/wubu_spawn.c \
		$(RT)/wubu_holyd.c $(RT)/wubu_holyd_session.c $(RT)/wubu_holyd_exec.c $(RT)/wubu_holyd_repl.o $(RT)/wubu_holyd_window.c $(RT)/wubu_holyd_input.c $(RT)/wubu_holyd_9p.c $(RT)/wubu_holyd_save.c $(RT)/wubu_holyd_event.c $(RT)/wubu_holyd_lifecycle.c \
		$(GUI)/dosgui_wm_test_stub.c \
		-o $(RT)/wubu_holyd_bin -lpthread
	@echo "✅ wubu_holyd binary built (./src/runtime/wubu_holyd_bin)"

# ── Network & Snapshot Tests ──────────────────────────────────────

test_network:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) \
		$(RT)/wubu_network.c $(RT)/wubu_network_fw.c $(RT)/wubu_network_svc.c $(RT)/wubu_network_cni.c $(RT)/wubu_network_wg.c $(RT)/wubu_network_ts.c $(RT)/wubu_network_dns.c $(RT)/wubu_network_qos.c $(RT)/wubu_network_create.c $(RT)/wubu_netlink.c $(RT)/wubu_spawn.c $(RT)/wubu_network_test.c \
		-o $(RT)/wubu_network_test -lpthread
	$(RT)/wubu_network_test

test_snapshot:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) \
		$(RT)/wubu_snapshot.c $(RT)/wubu_snapshot_gc.c $(RT)/wubu_snapshot_xport.c $(RT)/wubu_snapshot_diff.c $(RT)/wubu_snapshot_fs.c $(RT)/wubu_snapshot_copy.c $(RT)/wubu_snapshot_tag.c $(RT)/wubu_snapshot_test.c \
		-o $(RT)/wubu_snapshot_test -lpthread
	$(RT)/wubu_snapshot_test

test_system:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(COMP) -I$(JIT) \
		$(RT)/wubu_snapshot.c $(RT)/wubu_snapshot_gc.c $(RT)/wubu_snapshot_xport.c $(RT)/wubu_snapshot_diff.c $(RT)/wubu_snapshot_fs.c $(RT)/wubu_snapshot_copy.c $(RT)/wubu_snapshot_tag.c $(RT)/wubu_system.c $(RT)/wubu_system_test.c \
		-o $(RT)/wubu_system_test -lpthread
	$(RT)/wubu_system_test

# ── Daemon Panel Test ─────────────────────────────────────────────

# ── Daemon Panel Test ─────────────────────────────────────────────

test_daemon_panel:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBD_TEST_MAIN -Wno-format-truncation \
		-I$(GUI) -I$(RT) -I$(KERNEL) \
		$(GUI)/dosgui_daemon_panel.c $(GUI)/dosgui_service_mgr.c $(GUI)/dosgui_daemon_panel_test.c $(RT)/wubu_fs_util.o \
		$(RT)/wubu_archd_daemon.c $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.c $(RT)/wubu_archd_util.c $(RT)/wubu_archd_fs.c $(RT)/wubu_arch.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_container.c $(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		-o $(GUI)/dosgui_daemon_panel_test -lpthread
	$(GUI)/dosgui_daemon_panel_test

test_service_mgr:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wno-format-truncation \
		-I$(RT) -I$(GUI) -I$(KERNEL) \
		-DWUBD_TEST_MAIN \
		$(RT)/wubu_archd_daemon.c $(RT)/wubu_archd_loop.o $(RT)/wubu_archd_svc.c $(RT)/wubu_archd_util.c $(RT)/wubu_archd_fs.c $(RT)/wubu_arch.c $(RT)/wubu_ramdisk.c $(RT)/wubu_ramdisk_format.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_isolate.c $(RT)/wubu_ct_isolate_cgroup.c \
		$(RT)/wubu_container.c $(RT)/wubu_fs_util.o \
		$(RT)/styx_names.c $(RT)/styx_enc.c $(RT)/styx_serve.c $(RT)/styx_parse.c $(RT)/styx_fid.c $(RT)/styxfs_vfs.c $(RT)/styxfs_callbacks.c $(RT)/styxfs_posix.c $(RT)/styxfs_path.c $(RT)/styxfs_host.o $(RT)/styxfs_util.c \
		$(GUI)/dosgui_service_mgr.c $(GUI)/dosgui_service_mgr_test.c \
		-o $(GUI)/dosgui_service_mgr_test -lpthread
	$(GUI)/dosgui_service_mgr_test

# ── Compositor Test ───────────────────────────────────────────────

test_compositor:
	@echo "Skipping compositor test (requires wlroots dev headers)"

# ── dosgui_shell Wayland Client Test ──────────────────────────────

test_dosgui_shell:
	@echo "Skipping dosgui_shell test (requires wlroots dev headers)"

test_oci:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(KERNEL) -I$(RT)/oci \
		$(RT)/oci/oci_http_client.c $(RT)/oci/oci_image_config.c $(RT)/oci/oci_image_manifest.c $(RT)/oci/oci_image_index.c \
		$(RT)/oci/oci_blob_store.c $(RT)/oci/oci_convert.c $(RT)/oci/oci_registry.c \
		$(RT)/oci/oci_runtime_spec.c $(RT)/oci/oci_hooks.c $(RT)/oci/oci_cleanup.c \
		$(RT)/oci/oci_media_types.c $(RT)/oci/oci_descriptor.c \
		$(RT)/wubu_image.c $(RT)/wubu_image_cache.c $(RT)/wubu_image_parse.c $(RT)/wubu_image_manifest.c $(RT)/wubu_image_ops.c $(RT)/wubu_image_tar.c $(RT)/wubu_spawn.c $(RT)/wubu_container.c $(RT)/wubu_oci_test.c \
		-o $(RT)/wubu_oci_test -lm
	$(RT)/wubu_oci_test

test_spawn:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/wubu_spawn.c $(RT)/wubu_netlink.c $(RT)/wubu_spawn_test.c \
		-o $(RT)/wubu_spawn_test
	$(RT)/wubu_spawn_test

# ── EDR Engine ──────────────────────────────────────────────────

EDR_SRC = $(RT)/wubu_edr.c $(RT)/edr/edr_core.c $(RT)/edr/edr_proc_pin.c \
          $(RT)/edr/edr_fanotify.c $(RT)/edr/edr_poller.c

test_edr:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(RT) -I$(RT)/edr \
		$(EDR_SRC) $(RT)/wubu_edr_test.c \
		-o $(RT)/wubu_edr_test -lpthread
	$(RT)/wubu_edr_test

test_txn: $(RT)/wubu_txn/wubu_txn.c $(RT)/wubu_txn/wubu_txn_test.c
	$(CC) -O2 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) -I$(RT)/wubu_txn -pthread \
		$(RT)/wubu_txn/wubu_txn.c $(RT)/wubu_txn/wubu_txn_test.c \
		-o $(RT)/wubu_txn_test
	$(RT)/wubu_txn_test

test_cap: $(RT)/wubu_cap/wubu_cap_object.o $(RT)/wubu_cap/wubu_cap_token.o \
          $(RT)/wubu_cap/wubu_cap_revoke.o $(RT)/wubu_cap/wubu_cap_handle.o \
          $(RT)/wubu_cap/wubu_cap_system.o \
          $(RT)/wubu_cap/wubu_cap_test.c
	$(CC) -O2 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) -I$(RT)/wubu_cap -pthread \
		$(RT)/wubu_cap/wubu_cap_object.c $(RT)/wubu_cap/wubu_cap_token.c \
		$(RT)/wubu_cap/wubu_cap_revoke.c $(RT)/wubu_cap/wubu_cap_handle.c \
		$(RT)/wubu_cap/wubu_cap_system.c \
		$(RT)/wubu_cap/wubu_cap_test.c -o $(RT)/wubu_cap_test -lpthread
	$(RT)/wubu_cap_test

# ── Clean ────────────────────────────────────────────────────────

clean:
	rm -f $(KERNEL)/*.o $(JIT)/*.o $(COMP)/*.o $(RT)/*.o $(RT)/edr/*.o $(TOOLS)/*.o $(GUI)/*.o $(BRIDGE)/*.o $(APPS)/*.o $(WS)/*.o $(HOSTED)/*.o $(AUDIO)/*.o $(SHELL_DIR)/*.o
	rm -f $(JIT)/jit_test $(KERNEL)/memory_test $(KERNEL)/tasking_test $(KERNEL)/fat32_test $(KERNEL)/ahci_test $(KERNEL)/txfs_test $(KERNEL)/wubu_gaad_test $(COMP)/holyc_test $(RT)/wubu_container_test $(RT)/wubu_dos_emu_test $(RT)/wubu_apps_test $(RT)/wubu_vsl_test $(RT)/wubu_proton_test $(RT)/styx_test $(RT)/styxfs_test $(RT)/wubu_host_exec_test $(RT)/wubu_arch_test $(RT)/wubu_ramdisk_test $(RT)/wubu_gc_test $(RT)/wubu_anticheat_test $(RT)/wubu_bottles_test $(RT)/wubd_archd_test $(RT)/wubd_holyd_test $(RT)/wubu_network_test $(RT)/wubu_snapshot_test $(HOSTED)/hosted_test $(HOSTED)/wubu $(HOSTED)/wubu_metal_test $(WS)/test_worldsim $(BRIDGE)/vbe_ws_bridge_test $(BRIDGE)/bridge_test $(TOOLS)/iso9660_test $(TOOLS)/weight_check_test $(TOOLS)/screenshot_test $(GUI)/gui_dbuf_test $(GUI)/dosgui_wm_test $(GUI)/dosgui_startmenu_test $(GUI)/wubu_wm_test $(APPS)/wubu_apps2_test $(RT)/wubu_proton2_test $(AUDIO)/wubu_audio_test
	rm -f $(GUI)/vbe_sketch $(GUI)/dosgui_wm_test $(GUI)/sketch.ppm $(GUI)/sketch.png $(APPS)/canvas
	rm -f $(RT)/wubu_edr_test
	@echo "🧹 Clean"
