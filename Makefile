# My Seed OS — Top-Level Makefile
# Builds: kernel, JIT, GUI, bridge, apps, tests

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g -D_POSIX_C_SOURCE=200809L -Wno-array-bounds -DWUBU_NO_LIBM
LDFLAGS =

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

# ── Kernel Objects ───────────────────────────────────────────────
KERNEL_OBJS = $(KERNEL)/memory.o $(KERNEL)/tasking.o $(KERNEL)/vbe.o \
              $(KERNEL)/input.o $(KERNEL)/interrupt.o $(KERNEL)/isr_stubs.o $(KERNEL)/fat32.o $(KERNEL)/ahci.o $(KERNEL)/txfs.o $(KERNEL)/wubu_gaad.o $(KERNEL)/tasking_switch.o $(KERNEL)/ps2.o

# ── Metal Objects ────────────────────────────────────────────────
METAL_OBJS = $(HOSTED)/wubu_metal.o

# ── Audio Objects ────────────────────────────────────────────────
AUDIO_OBJS = $(AUDIO)/wubu_audio.o

# ── Hosted Objects ───────────────────────────────────────────────
HOSTED_OBJS_LIST = $(HOSTED)/wubu_drm_direct.o

# ── JIT Objects ──────────────────────────────────────────────────
JIT_OBJS = $(JIT)/jit.o

# ── GUI Objects ──────────────────────────────────────────────────
GUI_OBJS = $(GUI)/wm.o $(GUI)/taskbar.o $(GUI)/desktop.o $(GUI)/theme.o $(GUI)/gui_dbuf.o $(GUI)/wubu_theme.o $(GUI)/wubu_settings.o $(GUI)/wubu_wm.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o

# ── Bridge Objects ───────────────────────────────────────────────
BRIDGE_OBJS = $(BRIDGE)/bridge.o $(BRIDGE)/vbe_ws_bridge.o

# ── App Objects ──────────────────────────────────────────────────
APP_OBJS = $(APPS)/repl.o $(APPS)/notepad.o $(APPS)/paint.o $(APPS)/wubu_editor.o $(APPS)/wubu_canvas.o $(APPS)/wubu_codec.o $(APPS)/dosgui_apps.o

# ── WorldSim Objects ─────────────────────────────────────────────
WS_OBJS = $(WS)/terrain.o $(WS)/entity.o $(WS)/physics.o $(WS)/render.o $(WS)/sim.o

COMP_OBJS = $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_codegen.o
RT_OBJS   = $(RT)/wubu_container.o $(RT)/wubu_exec.o $(RT)/wubu_vsl.o $(RT)/wubu_proton.o $(RT)/styx.o $(RT)/wubu_arch.o $(RT)/wubu_ramdisk.o $(RT)/wubu_proton2.o $(RT)/wubu_gc.o $(RT)/wubu_host_exec.o $(RT)/wubu_ct_bwrap.o
TOOLS_OBJS = $(TOOLS)/iso9660.o $(TOOLS)/weight_check.o $(TOOLS)/screenshot.o

# ── Shell Objects ────────────────────────────────────────────────
SHELL_OBJS = $(SHELL_DIR)/wubu_shell.o

# ── Bear RL Objects ──────────────────────────────────────────────
BEAR_OBJS = $(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_nn.o $(BEAR)/bear_ppo.o $(BEAR)/bear_opt.o

# ── Targets ─────────────────────────────────────────────────────

.PHONY: all clean test kernel jit gui bridge apps worldsim

shell: $(SHELL_OBJS)
	@echo "✅ Shell built"

$(SHELL_DIR)/%.o: $(SHELL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) -c $< -o $@

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
		$(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_nn.o $(BEAR)/bear_ppo.o $(BEAR)/bear_opt.o $(BEAR)/bear_train.o \
		-lm -o $(BEAR)/bear_train
	@echo "✅ Bear RL training binary built (./src/bear/bear_train)"

kernel: $(KERNEL_OBJS)
	@echo "✅ Kernel built"

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

apps: $(APP_OBJS)
	@echo "✅ Apps built"

# ── Game / App Targets ───────────────────────────────────────────

paint: $(APP_OBJS) $(GUI_OBJS) $(KERNEL_OBJS) $(JIT_OBJS)
	$(CC) $(CFLAGS) -I$(APPS) -I$(GUI) -I$(KERNEL) -I$(JIT) \
		$(APPS)/paint.c $(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c \
		$(GUI)/theme.c $(GUI)/gui_dbuf.c $(GUI)/startmenu.c \
		$(KERNEL)/memory.c $(KERNEL)/vbe.c $(KERNEL)/input.c \
		$(KERNEL)/tasking.c $(KERNEL)/interrupt.c \
		$(JIT)/jit.c \
		-o $(APPS)/paint
	@echo "✅ Paint built (./src/apps/paint)"

doom: $(APP_OBJS) $(GUI_OBJS) $(KERNEL_OBJS) $(JIT_OBJS)
	$(CC) $(CFLAGS) -I$(APPS) -I$(GUI) -I$(KERNEL) -I$(JIT) \
		$(APPS)/doom.c $(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c \
		$(GUI)/theme.c $(GUI)/gui_dbuf.c $(GUI)/startmenu.c \
		$(KERNEL)/memory.c $(KERNEL)/vbe.c $(KERNEL)/input.c \
		$(KERNEL)/tasking.c $(KERNEL)/interrupt.c \
		$(JIT)/jit.c \
		-lwayland-client -o $(APPS)/doom
	@echo "✅ Doom built (./src/apps/doom)"

worldsim: $(WS_OBJS)
	@echo "✅ WorldSim built"

# ── Hosted binary (full WuBuOS GUI shell + kernel in-process) ────
# Cell 200: ZealOS kernel runs in-process, WM + desktop + taskbar + start menu
HOSTED_OBJS = $(HOSTED)/hosted.o $(RT)/styx.o $(KERNEL)/vbe.o $(KERNEL)/memory.o \
              $(KERNEL)/input.o $(KERNEL)/tasking.o $(KERNEL)/interrupt.o $(KERNEL)/isr_stubs.o $(BRIDGE)/bridge.o \
              $(GUI)/wm.o $(GUI)/taskbar.o $(GUI)/desktop.o $(GUI)/theme.o $(GUI)/startmenu.o \
              $(GUI)/gui_dbuf.o $(GUI)/dosgui_wm.o $(GUI)/dosgui_desktop.o $(GUI)/dosgui_startmenu.o \
              $(GUI)/wubu_theme.o $(GUI)/wubu_settings.o \
              $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_codegen.o $(APPS)/repl.o $(APPS)/dosgui_apps.o $(JIT)/jit.o \
              $(RT)/wubu_ct_bwrap.o

$(HOSTED)/xdg-shell-private.o: $(HOSTED)/xdg-shell-private.code
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -c $< -o $@

hosted: $(HOSTED_OBJS) $(HOSTED)/xdg-shell-private.o
	$(CC) $(CFLAGS) -DVBE_HOSTED -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(BRIDGE) -I$(GUI) -I$(COMP) -I$(JIT) -I$(APPS) \
		$(HOSTED)/hosted.c $(RT)/styx.c $(RT)/styxfs.c $(KERNEL)/vbe.c $(KERNEL)/memory.c \
		$(KERNEL)/input.c $(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(BRIDGE)/bridge.c \
		$(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c $(GUI)/theme.c $(GUI)/startmenu.c \
		$(GUI)/gui_dbuf.c $(GUI)/dosgui_wm.c $(GUI)/dosgui_desktop.c $(GUI)/dosgui_startmenu.c $(GUI)/wubu_theme.c $(GUI)/wubu_settings.c \
		$(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(APPS)/repl.c $(APPS)/dosgui_apps.c $(JIT)/jit.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_bwrap.c \
		$(HOSTED)/xdg-shell-private.o \
		-lwayland-client -lxkbcommon -lm -o $(HOSTED)/wubu
	@echo "✅ WuBuOS hosted binary built (./src/hosted/wubu)"
# ── Compilation Rules ────────────────────────────────────────────

$(KERNEL)/%.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL) -c $< -o $@

$(JIT)/%.o: $(JIT)/%.c
	$(CC) $(CFLAGS) -I$(JIT) -c $< -o $@

$(GUI)/%.o: $(GUI)/%.c
	$(CC) $(CFLAGS) -I$(GUI) -I$(KERNEL) -c $< -o $@

$(BRIDGE)/%.o: $(BRIDGE)/%.c
	$(CC) $(CFLAGS) -I$(BRIDGE) -I$(KERNEL) -I$(WS) -c $< -o $@

$(APPS)/%.o: $(APPS)/%.c
	$(CC) $(CFLAGS) -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -c $< -o $@

$(WS)/%.o: $(WS)/%.c
	$(CC) $(CFLAGS) -I$(WS) -I$(KERNEL) -c $< -o $@

$(COMP)/%.o: $(COMP)/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -c $< -o $@

$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(COMP) -I$(JIT) -c $< -o $@

$(TOOLS)/%.o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) -I$(TOOLS) -c $< -o $@

$(HOSTED)/%.o: $(HOSTED)/%.c
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -c $< -o $@

$(AUDIO)/%.o: $(AUDIO)/%.c
	$(CC) $(CFLAGS) -I$(AUDIO) -I$(KERNEL) -I$(RT) -c $< -o $@

$(BEAR)/%.o: $(BEAR)/%.c
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) -c $< -o $@

# Assembly files
$(KERNEL)/%.o: $(KERNEL)/%.S
	$(CC) $(CFLAGS) -I$(KERNEL) -c $< -o $@

# ── Tests ────────────────────────────────────────────────────────

test: test_jit test_memory test_tasking test_input test_worldsim test_fat32 test_holyc test_wubu test_apps test_vsl test_bridge test_bridge_flip test_proton test_ahci test_iso test_weights test_gc test_txfs test_dbuf test_wm test_startmenu test_styx test_styxfs test_hosted test_host_exec test_arch test_ramdisk test_gaad test_wubu_wm test_apps2 test_proton2 test_audio test_screenshot test_dosgui_wm
	@echo "✅ All tests passed"

test_jit: $(JIT)/jit.o
	$(CC) -O0 -g -I$(JIT) $(JIT)/jit.c $(JIT)/jit_test.c -o $(JIT)/jit_test
	$(JIT)/jit_test

test_memory: $(KERNEL)/memory.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/memory.c $(KERNEL)/memory_test.c -o $(KERNEL)/memory_test
	$(KERNEL)/memory_test

test_tasking: $(KERNEL)/memory.o $(KERNEL)/tasking.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/memory.c $(KERNEL)/tasking.c $(KERNEL)/tasking_test.c -o $(KERNEL)/tasking_test
	$(KERNEL)/tasking_test

test_input:
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/input.c $(KERNEL)/input_test.c -o $(KERNEL)/input_test
	$(KERNEL)/input_test

test_worldsim:
	$(CC) -Wall -Wextra -std=c11 -O0 -g -I$(WS) -I$(KERNEL) $(WS)/test_worldsim.c $(WS)/terrain.c $(WS)/entity.c $(WS)/physics.c $(WS)/render.c $(WS)/sim.c -o $(WS)/test_worldsim
	$(WS)/test_worldsim

test_fat32: $(KERNEL)/fat32.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/fat32.c $(KERNEL)/fat32_test.c -o $(KERNEL)/fat32_test
	$(KERNEL)/fat32_test

test_holyc: $(JIT)/jit.o
	$(CC) -O0 -g -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(COMP)/holyc_test.c -o $(COMP)/holyc_test
	$(COMP)/holyc_test

test_wubu: $(JIT)/jit.o $(RT)/wubu_host_exec.o $(RT)/styxfs.o $(RT)/styx.o
	$(CC) -O0 -g -I$(RT) -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_host_exec.c $(RT)/styx.c $(RT)/styxfs.c $(RT)/wubu_container_test.c -o $(RT)/wubu_container_test
	$(RT)/wubu_container_test

test_apps:
	$(CC) -O0 -g -I$(RT) -I$(COMP) -I$(JIT) $(RT)/wubu_pkg.c $(RT)/wubu_vsl.c $(RT)/wubu_proton.c $(RT)/wubu_apps_test.c -o $(RT)/wubu_apps_test
	$(RT)/wubu_apps_test

test_vsl:
	$(CC) -O0 -g -I$(RT) $(RT)/wubu_vsl.c $(RT)/wubu_vsl_test.c -o $(RT)/wubu_vsl_test
	$(RT)/wubu_vsl_test

test_bridge:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(BRIDGE) -I$(KERNEL) -I$(WS) \
		$(KERNEL)/vbe.c $(BRIDGE)/vbe_ws_bridge.c \
		$(WS)/terrain.c $(WS)/entity.c $(WS)/physics.c $(WS)/render.c $(WS)/sim.c \
		$(BRIDGE)/vbe_ws_bridge_test.c -o $(BRIDGE)/vbe_ws_bridge_test
	$(BRIDGE)/vbe_ws_bridge_test

test_bridge_flip:
	$(CC) -O0 -g -std=c11 -I$(BRIDGE) $(BRIDGE)/bridge.c $(BRIDGE)/bridge_test.c -o $(BRIDGE)/bridge_test
	$(BRIDGE)/bridge_test

test_proton:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/wubu_proton.c $(RT)/wubu_proton_test.c -o $(RT)/wubu_proton_test
	$(RT)/wubu_proton_test

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
		$(TOOLS)/screenshot.c $(KERNEL)/vbe.c $(GUI)/wubu_wm.c $(GUI)/wubu_theme.c $(KERNEL)/wubu_gaad.c \
		$(TOOLS)/screenshot_test.c \
		-o $(TOOLS)/screenshot_test
	$(TOOLS)/screenshot_test

test_txfs:
	$(CC) -O0 -g -std=c11 -I$(KERNEL) $(KERNEL)/txfs.c $(KERNEL)/txfs_test.c -o $(KERNEL)/txfs_test
	$(KERNEL)/txfs_test

test_dbuf:
	$(CC) -O0 -g -std=c11 -I$(GUI) $(GUI)/gui_dbuf.c $(GUI)/gui_dbuf_test.c -o $(GUI)/gui_dbuf_test
	$(GUI)/gui_dbuf_test

test_wm:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/wm.c $(GUI)/wm_test.c $(KERNEL)/vbe.c -o $(GUI)/wm_test
	$(GUI)/wm_test

test_startmenu:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/startmenu.c $(GUI)/startmenu_test.c $(KERNEL)/vbe.c -o $(GUI)/startmenu_test
	$(GUI)/startmenu_test

test_styx:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/styx.c $(RT)/styx_test.c -o $(RT)/styx_test
	$(RT)/styx_test

test_styxfs:
	$(CC) -O0 -g -std=c11 -I$(RT) -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_host_exec.c $(RT)/styx.c $(RT)/styxfs.c $(RT)/styxfs_test.c -o $(RT)/styxfs_test
	$(RT)/styxfs_test

test_hosted: $(HOSTED)/xdg-shell-private.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_HOSTED_TEST -I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(BRIDGE) -I$(GUI) -I$(COMP) -I$(JIT) -I$(APPS) \
		$(HOSTED)/hosted_test.c $(HOSTED)/hosted.c $(RT)/styx.c $(RT)/styxfs.c $(KERNEL)/vbe.c $(KERNEL)/memory.c \
		$(KERNEL)/input.c $(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(KERNEL)/isr_stubs.S $(BRIDGE)/bridge.c \
		$(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c $(GUI)/theme.c $(GUI)/startmenu.c \
		$(GUI)/gui_dbuf.c $(GUI)/dosgui_wm.c $(GUI)/dosgui_desktop.c $(GUI)/dosgui_startmenu.c \
		$(GUI)/wubu_theme.c \
		$(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(APPS)/repl.c $(APPS)/dosgui_apps.c $(JIT)/jit.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_ct_bwrap.c \
		$(HOSTED)/xdg-shell-private.o \
		-o $(HOSTED)/hosted_test -lwayland-client -lxkbcommon
	$(HOSTED)/hosted_test

test_host_exec:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/styx.c $(RT)/styxfs.c $(RT)/wubu_host_exec.c $(RT)/wubu_host_exec_test.c \
		-o $(RT)/wubu_host_exec_test
	$(RT)/wubu_host_exec_test

test_arch:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(APPS) \
		$(RT)/styx.c $(RT)/styxfs.c $(RT)/wubu_arch.c $(RT)/wubu_host_exec.c \
		$(APPS)/wubu_freedoom.c $(RT)/wubu_arch_test.c \
		-o $(RT)/wubu_arch_test
	$(RT)/wubu_arch_test

test_ramdisk:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(APPS) \
		$(RT)/styx.c $(RT)/styxfs.c $(RT)/wubu_ramdisk.c $(RT)/wubu_arch.c $(RT)/wubu_host_exec.c \
		$(RT)/wubu_ramdisk_test.c \
		-o $(RT)/wubu_ramdisk_test
	$(RT)/wubu_ramdisk_test

test_gaad:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(KERNEL) \
		$(KERNEL)/wubu_gaad.c $(KERNEL)/wubu_gaad_test.c \
		-o $(KERNEL)/wubu_gaad_test
	$(KERNEL)/wubu_gaad_test

test_wubu_wm:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_NO_LIBM \
		-I$(GUI) -I$(KERNEL) -I$(RT) \
		$(RT)/styx.c $(RT)/styxfs.c $(GUI)/wubu_theme.c $(GUI)/wubu_wm.c \
		$(KERNEL)/wubu_gaad.c $(KERNEL)/vbe.c \
		$(RT)/wubu_host_exec.c \
		$(GUI)/wubu_wm_test.c \
		-o $(GUI)/wubu_wm_test
	$(GUI)/wubu_wm_test

# Cell 388/389: Direct DRM/KMS (no libdrm) + custom GBM test
test_drm_direct:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(HOSTED) -I$(RT) \
		$(HOSTED)/wubu_drm_direct.c \
		$(HOSTED)/wubu_display_test.c \
		-o $(HOSTED)/wubu_drm_direct_test
	$(HOSTED)/wubu_drm_direct_test

test_apps2:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(APPS) -I$(KERNEL) -I$(RT) \
		$(APPS)/wubu_editor.c $(APPS)/wubu_canvas.c $(APPS)/wubu_codec.c \
		$(RT)/wubu_host_exec.c $(RT)/styx.c $(RT)/styxfs.c \
		$(APPS)/wubu_apps2_test.c \
		-o $(APPS)/wubu_apps2_test
	$(APPS)/wubu_apps2_test

test_proton2:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_proton2.c $(RT)/wubu_ramdisk.c $(RT)/wubu_arch.c \
		$(RT)/wubu_host_exec.c $(RT)/wubu_container.c $(RT)/styx.c $(RT)/styxfs.c \
		$(RT)/wubu_proton2_test.c \
		-o $(RT)/wubu_proton2_test
	$(RT)/wubu_proton2_test

test_metal:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DMYSEED_METAL -DWUBU_NO_LIBM \
		-I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -I$(SHELL_DIR) -I$(COMP) \
		$(HOSTED)/wubu_metal.c $(KERNEL)/wubu_gaad.c \
		$(KERNEL)/memory.c $(KERNEL)/vbe.c $(KERNEL)/input.c $(KERNEL)/interrupt.c $(KERNEL)/isr_stubs.S $(KERNEL)/tasking.c $(KERNEL)/tasking_switch.S \
		$(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c $(GUI)/theme.c $(GUI)/startmenu.c $(GUI)/gui_dbuf.c \
		$(BRIDGE)/bridge.c \
		$(APPS)/repl.c \
		$(SHELL_DIR)/wubu_shell.c \
		$(HOSTED)/wubu_metal_test.c \
		-o $(HOSTED)/wubu_metal_test
	$(HOSTED)/wubu_metal_test

test_dosgui_wm:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/dosgui_wm.c $(KERNEL)/vbe.c $(GUI)/dosgui_wm_test.c -o $(GUI)/dosgui_wm_test
	$(GUI)/dosgui_wm_test
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM \
		-I$(AUDIO) -I$(KERNEL) -I$(RT) \
		$(AUDIO)/wubu_audio.c \
		$(AUDIO)/wubu_audio_test.c \
		-o $(AUDIO)/wubu_audio_test -lpthread
	$(AUDIO)/wubu_audio_test

# ── Clean ────────────────────────────────────────────────────────

clean:
	rm -f $(KERNEL)/*.o $(JIT)/*.o $(COMP)/*.o $(RT)/*.o $(TOOLS)/*.o $(GUI)/*.o $(BRIDGE)/*.o $(APPS)/*.o $(WS)/*.o $(HOSTED)/*.o $(AUDIO)/*.o $(SHELL_DIR)/*.o
	rm -f $(JIT)/jit_test $(KERNEL)/memory_test $(KERNEL)/tasking_test $(KERNEL)/fat32_test $(KERNEL)/ahci_test $(KERNEL)/txfs_test $(KERNEL)/wubu_gaad_test $(COMP)/holyc_test $(RT)/wubu_container_test $(RT)/wubu_apps_test $(RT)/wubu_vsl_test $(RT)/wubu_proton_test $(RT)/styx_test $(RT)/styxfs_test $(RT)/wubu_host_exec_test $(RT)/wubu_arch_test $(RT)/wubu_ramdisk_test $(RT)/wubu_gc_test $(HOSTED)/hosted_test $(HOSTED)/wubu $(HOSTED)/wubu_metal_test $(WS)/test_worldsim $(BRIDGE)/vbe_ws_bridge_test $(BRIDGE)/bridge_test $(TOOLS)/iso9660_test $(TOOLS)/weight_check_test $(TOOLS)/screenshot_test $(GUI)/gui_dbuf_test $(GUI)/wm_test $(GUI)/startmenu_test $(GUI)/wubu_wm_test $(APPS)/wubu_apps2_test $(RT)/wubu_proton2_test $(AUDIO)/wubu_audio_test
	rm -f $(JIT)/jit_stub $(GUI)/vbe_sketch $(GUI)/dosgui_wm_test $(GUI)/sketch.ppm $(GUI)/sketch.png $(APPS)/paint $(APPS)/doom
	@echo "🧹 Clean"
