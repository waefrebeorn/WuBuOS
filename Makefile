# My Seed OS — Top-Level Makefile
# Builds: kernel, JIT, GUI, bridge, apps, tests

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g -D_POSIX_C_SOURCE=200809L -Wno-array-bounds
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

# ── Kernel Objects ───────────────────────────────────────────────
KERNEL_OBJS = $(KERNEL)/memory.o $(KERNEL)/tasking.o $(KERNEL)/vbe.o \
              $(KERNEL)/input.o $(KERNEL)/interrupt.o $(KERNEL)/fat32.o $(KERNEL)/ahci.o $(KERNEL)/txfs.o

# ── JIT Objects ──────────────────────────────────────────────────
JIT_OBJS = $(JIT)/jit.o

# ── GUI Objects ──────────────────────────────────────────────────
GUI_OBJS = $(GUI)/wm.o $(GUI)/taskbar.o $(GUI)/desktop.o $(GUI)/theme.o $(GUI)/gui_dbuf.o

# ── Bridge Objects ───────────────────────────────────────────────
BRIDGE_OBJS = $(BRIDGE)/bridge.o $(BRIDGE)/vbe_ws_bridge.o

# ── App Objects ──────────────────────────────────────────────────
APP_OBJS = $(APPS)/repl.o $(APPS)/notepad.o $(APPS)/paint.o $(APPS)/doom.o

# ── WorldSim Objects ─────────────────────────────────────────────
WS_OBJS = $(WS)/terrain.o $(WS)/entity.o $(WS)/physics.o $(WS)/render.o $(WS)/sim.o

COMP_OBJS = $(COMP)/holyc_lexer.o $(COMP)/holyc_parse.o $(COMP)/holyc_codegen.o
RT_OBJS   = $(RT)/wubu_container.o $(RT)/wubu_exec.o $(RT)/wubu_vsl.o $(RT)/wubu_proton.o $(RT)/styx.o
TOOLS_OBJS = $(TOOLS)/iso9660.o $(TOOLS)/weight_check.o

# ── All Objects ──────────────────────────────────────────────────
ALL_OBJS = $(KERNEL_OBJS) $(JIT_OBJS) $(COMP_OBJS) $(RT_OBJS) $(TOOLS_OBJS) $(GUI_OBJS) $(BRIDGE_OBJS) $(APP_OBJS) $(WS_OBJS)

# ── Targets ─────────────────────────────────────────────────────

.PHONY: all clean test kernel jit gui bridge apps worldsim

all: kernel jit compiler runtime tools gui bridge apps worldsim
	@echo "✅ WuBuOS built"

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
		-lX11 -lm -o $(APPS)/doom
	@echo "✅ Doom built (./src/apps/doom)"

worldsim: $(WS_OBJS)
	@echo "✅ WorldSim built"

# ── Hosted binary (full WuBuOS GUI shell + kernel in-process) ────
# Cell 200: ZealOS kernel runs in-process, WM + desktop + taskbar + start menu
HOSTED_OBJS = $(HOSTED)/hosted.o $(RT)/styx.o $(KERNEL)/vbe.o $(KERNEL)/memory.o \
              $(KERNEL)/input.o $(KERNEL)/tasking.o $(KERNEL)/interrupt.o $(BRIDGE)/bridge.o \
              $(GUI)/wm.o $(GUI)/taskbar.o $(GUI)/desktop.o $(GUI)/theme.o $(GUI)/startmenu.o

hosted: $(HOSTED_OBJS)
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(BRIDGE) -I$(GUI) \
		$(HOSTED)/hosted.c $(RT)/styx.c $(KERNEL)/vbe.c $(KERNEL)/memory.c \
		$(KERNEL)/input.c $(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(BRIDGE)/bridge.c \
		$(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c $(GUI)/theme.c $(GUI)/startmenu.c \
		-lX11 -lm -o $(HOSTED)/wubu
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
	$(CC) $(CFLAGS) -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -c $< -o $@

$(WS)/%.o: $(WS)/%.c
	$(CC) $(CFLAGS) -I$(WS) -c $< -o $@

$(COMP)/%.o: $(COMP)/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -c $< -o $@

$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(COMP) -I$(JIT) -c $< -o $@

$(TOOLS)/%.o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) -I$(TOOLS) -c $< -o $@

$(HOSTED)/%.o: $(HOSTED)/%.c
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(BRIDGE) -I$(GUI) -c $< -o $@

# ── Tests ────────────────────────────────────────────────────────

test: test_jit test_memory test_tasking test_worldsim test_fat32 test_holyc test_wubu test_apps test_vsl test_bridge test_bridge_flip test_proton test_ahci test_iso test_weights test_txfs test_dbuf test_wm test_startmenu test_styx test_styxfs test_hosted test_host_exec
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

test_worldsim:
	$(CC) -Wall -Wextra -std=c11 -O0 -g -I$(WS) $(WS)/test_worldsim.c $(WS)/terrain.c $(WS)/entity.c $(WS)/physics.c $(WS)/render.c $(WS)/sim.c -o $(WS)/test_worldsim -lm
	$(WS)/test_worldsim

test_fat32: $(KERNEL)/fat32.o
	$(CC) $(CFLAGS) -O0 -g -I$(KERNEL) $(KERNEL)/fat32.c $(KERNEL)/fat32_test.c -o $(KERNEL)/fat32_test
	$(KERNEL)/fat32_test

test_holyc: $(JIT)/jit.o
	$(CC) -O0 -g -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(COMP)/holyc_test.c -o $(COMP)/holyc_test
	$(COMP)/holyc_test

test_wubu: $(JIT)/jit.o
	$(CC) -O0 -g -I$(RT) -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/wubu_container_test.c -o $(RT)/wubu_container_test
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
		$(BRIDGE)/vbe_ws_bridge_test.c -o $(BRIDGE)/vbe_ws_bridge_test -lm
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

test_txfs:
	$(CC) -O0 -g -std=c11 -I$(KERNEL) $(KERNEL)/txfs.c $(KERNEL)/txfs_test.c -o $(KERNEL)/txfs_test
	$(KERNEL)/txfs_test

test_dbuf:
	$(CC) -O0 -g -std=c11 -I$(GUI) $(GUI)/gui_dbuf.c $(GUI)/gui_dbuf_test.c -o $(GUI)/gui_dbuf_test
	$(GUI)/gui_dbuf_test

test_wm:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/wm.c $(GUI)/wm_test.c $(KERNEL)/vbe.c -o $(GUI)/wm_test -lm
	$(GUI)/wm_test

test_startmenu:
	$(CC) -O0 -g -std=c11 -DVBE_HOSTED -I$(GUI) -I$(KERNEL) $(GUI)/startmenu.c $(GUI)/startmenu_test.c $(KERNEL)/vbe.c -o $(GUI)/startmenu_test -lm
	$(GUI)/startmenu_test

test_styx:
	$(CC) -O0 -g -std=c11 -I$(RT) $(RT)/styx.c $(RT)/styx_test.c -o $(RT)/styx_test
	$(RT)/styx_test

test_styxfs:
	$(CC) -O0 -g -std=c11 -I$(RT) -I$(COMP) -I$(JIT) $(JIT)/jit.c $(COMP)/holyc_lexer.c $(COMP)/holyc_parse.c $(COMP)/holyc_codegen.c $(RT)/wubu_container.c $(RT)/wubu_exec.c $(RT)/styx.c $(RT)/styxfs.c $(RT)/styxfs_test.c -o $(RT)/styxfs_test
	$(RT)/styxfs_test

test_hosted:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DVBE_HOSTED -DWUBU_HOSTED_TEST -I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(BRIDGE) -I$(GUI) \
		$(HOSTED)/hosted_test.c $(HOSTED)/hosted.c $(RT)/styx.c $(KERNEL)/vbe.c $(KERNEL)/memory.c \
		$(KERNEL)/input.c $(KERNEL)/tasking.c $(KERNEL)/interrupt.c $(BRIDGE)/bridge.c \
		$(GUI)/wm.c $(GUI)/taskbar.c $(GUI)/desktop.c $(GUI)/theme.c $(GUI)/startmenu.c \
		-o $(HOSTED)/hosted_test -lm -lX11
	$(HOSTED)/hosted_test

test_host_exec:
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) \
		$(RT)/wubu_host_exec.c $(RT)/wubu_host_exec_test.c \
		-o $(RT)/wubu_host_exec_test
	$(RT)/wubu_host_exec_test

# ── Clean ────────────────────────────────────────────────────────

clean:
	rm -f $(KERNEL)/*.o $(JIT)/*.o $(COMP)/*.o $(RT)/*.o $(TOOLS)/*.o $(GUI)/*.o $(BRIDGE)/*.o $(APPS)/*.o $(WS)/*.o $(HOSTED)/*.o
	rm -f $(JIT)/jit_test $(KERNEL)/memory_test $(KERNEL)/tasking_test $(KERNEL)/fat32_test $(KERNEL)/ahci_test $(KERNEL)/txfs_test $(COMP)/holyc_test $(RT)/wubu_container_test $(RT)/wubu_apps_test $(RT)/wubu_vsl_test $(RT)/wubu_proton_test $(RT)/styx_test $(RT)/styxfs_test $(RT)/wubu_host_exec_test $(HOSTED)/hosted_test $(HOSTED)/wubu $(WS)/test_worldsim $(BRIDGE)/vbe_ws_bridge_test $(BRIDGE)/bridge_test $(TOOLS)/iso9660_test $(TOOLS)/weight_check_test $(GUI)/gui_dbuf_test $(GUI)/wm_test $(GUI)/startmenu_test
	rm -f $(JIT)/jit_stub $(GUI)/vbe_sketch $(GUI)/sketch.ppm $(GUI)/sketch.png $(APPS)/paint $(APPS)/doom
	@echo "🧹 Clean"
