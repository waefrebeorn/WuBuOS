# ── Build Targets ────────────────────────────────────────────────
#
# Each subsystem builds its object list via the pattern rules in
# mk/pattern-rules.mk. The `all` umbrella links them in dependency order:
# kernel first (no deps), then compiler/jit (dep: kernel), runtime (dep: rt),
# tools, gui (dep: kernel + rt), bridge, apps (dep: all), worldsim, metal,
# audio, shell, bear, and the hosted Wayland objects.

.PHONY: all clean test kernel jit compiler runtime tools gui bridge apps worldsim firmware uefi test_uefi test_agi_metal docs shell bear hosted_objs hosted worldspace install uninstall window metal audio

all: kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs hosted
	@echo "✅ WuBuOS built"

# ── WuBuFW: our own C11 UEFI firmware (no EDK2 / no OVMF) ────────────────
FW = src/firmware

firmware uefi:
	$(FW)/build.sh

# Regenerate the programmatically-created compendium sections (01-reference)
# from the source tree. Run after every code-change batch.
docs:
	python3 tools/gen_docs.py

# Builds firmware + PE payload + FAT32/GPT ESP, boots in QEMU, asserts
# the payload's 28-check conformance run passes.
test_uefi:
	$(FW)/run.sh

# Boots the REAL WuBuOS bare-metal AGI kernel as a WuBuFW measured payload:
# firmware attestation -> chainloader (SHA-256 + handoff) -> kernel_main ->
# AGI supervisor with the root-of-trust gate live. Asserts every hop.
test_agi_metal:
	$(FW)/run-agi.sh

# Secure Boot enrollment + enforcement: mksb (sbctl equivalent) host round-trip
# plus the firmware's ESL enrollment selftest and signed/unsigned PE gate.
test_secureboot:
	$(FW)/sb-gate.sh

# Real RSA-2048 signature enforcement: openssl-generated key, the chainloader
# signed with a WRSB trailer, the firmware's RSA db enrollment + verify hook.
# Signed loader boots; a tampered loader is refused.
test_rsa:
	$(FW)/rsa-gate.sh

# ── Metal / Audio / Hosted ────────────────────────────────────────────────
metal: $(METAL_OBJS)
	@echo "✅ Metal layer built"

audio: $(AUDIO_OBJS)
	@echo "✅ Audio engine built"

hosted_objs: $(HOSTED_OBJS_LIST)
	@echo "✅ Hosted objects built"

# ── Install the hosted desktop ─────────────────────────────────────────────
# The product is the hosted body; install it like any other desktop app so
# you run it instead of cloning and hacking. PREFIX defaults to ~/.local;
# set PREFIX=/usr/local for a system install.
PREFIX ?= $(HOME)/.local
ICON_SRC ?= docs/logo/wubuos-desktop.png

install: hosted wubu_inject
	@install -d $(PREFIX)/bin $(PREFIX)/share/applications $(PREFIX)/share/pixmaps
	@install -m 755 $(HOSTED)/wubu $(PREFIX)/bin/wubuos
	@install -m 755 $(HOSTED)/wubu-weston.sh $(PREFIX)/bin/wubuos-window
	@install -m 755 $(HOSTED)/wubu_inject $(PREFIX)/bin/wubu-inject
	@sed -e 's|@PREFIX@|$(PREFIX)|g' mk/wubuos.desktop.in > $(PREFIX)/share/applications/wubuos.desktop
	@if [ -f $(ICON_SRC) ]; then install -m 644 $(ICON_SRC) $(PREFIX)/share/pixmaps/wubuos.png; fi
	@echo "✅ WuBuOS desktop installed to $(PREFIX)"
	@echo "   binary  : $(PREFIX)/bin/wubuos"
	@echo "   launcher: $(PREFIX)/share/applications/wubuos.desktop"
	@echo "   icon    : $(PREFIX)/share/pixmaps/wubuos.png"
	@echo "   window  : $(PREFIX)/bin/wubuos-window [WxH]   (a scaled weston window)"
	@echo "   run it  : wubuos     (a Wayland compositor is needed for the GUI;"
	@echo "                        $(PREFIX)/bin/wubuos --screenshot shot.ppm renders a frame headless)"

# Inject input into the workspace window (XTEST; the compositor delivers it
# to the focused Wayland surface). Built as part of `hosted`.
wubu_inject: $(HOSTED)/wubu_inject.c
	$(CC) -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L $(HOSTED)/wubu_inject.c -o $(HOSTED)/wubu_inject -lX11 -ldl
	@echo "✅ Input injector built: ./src/hosted/wubu_inject"

# Run the hosted desktop in a scaled weston window on the host display.
# Usage: make window [SIZE=1280x720]
SIZE ?= 1280x720
window: hosted
	@chmod +x $(HOSTED)/wubu-weston.sh
	@$(HOSTED)/wubu-weston.sh $(SIZE)

# ── Boot the WuBuOS desktop on OUR compositor (slerm'd, no wlroots). ─────
# Usage: make compositor [SIZE=1024x768], or make compositor -w 960 540
compositor:
	@tools/run-wubucompositor.sh $(SIZE)

uninstall:
	@rm -f $(PREFIX)/bin/wubuos $(PREFIX)/bin/wubuos-window $(PREFIX)/bin/wubu-inject $(PREFIX)/share/applications/wubuos.desktop $(PREFIX)/share/pixmaps/wubuos.png
	@echo "✅ WuBuOS desktop removed from $(PREFIX)"

# ── The ~WuBu~ space engine (the WWW submodule) ─────────────────────────────
# The body does not embed the space: it builds it and reaches it. The engine
# is a self-contained C11 tree with its own gates (`make test` there runs 20).
worldspace:
	$(MAKE) -C $(LUA)/www/~WuBu~
	@echo "✅ WorldSpace engine built"

# ── The hosted binary ────────────────────────────────────────────────────────
# The product: the Win98/XP shell running on a host Wayland display. Same object
# graph as the gui_shot capture harness, but hosted.c is compiled WITHOUT
# -DWUBU_HOSTED_TEST so its main() is live (the harness compiles it out and
# supplies its own main).
HOSTED_BIN   = $(HOSTED)/wubu
HOSTED_SUBSYS = $(HOSTED)/hosted_render.c $(HOSTED)/hosted_run.c $(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c \
                $(HOSTED)/hosted_pe.c $(HOSTED)/wubu_display.c $(HOSTED)/hosted_wayland.c \
                $(HOSTED)/hosted_wayland_input.c $(HOSTED)/hosted_wayland_shm.c \
                $(HOSTED)/hosted_wayland_surface.c
# The closure the shell actually reaches that the shared object lists omit:
# the hosted hardware sense, the a11y/sound/psych services, the EDR poller,
# the WET applet framework + its slerm-Lua, and three compiler backends.
HOSTED_EXTRA = $(COMP)/holyd_semantic.c $(COMP)/wubu_tgemm_avx512.o $(COMP)/jit/wubu_isa_wasm.c \
               $(GUI)/cp_sound.c $(GUI)/wubu_a11y.c $(GUI)/wubu_sound.c \
               $(RT)/edr/edr_poller.c $(RT)/wubu_gdpr_age.c $(RT)/wubu_wine_env.c $(RT)/wubu_compat_db.c \
               $(RT)/wubu_agent.c $(RT)/wubu_ns_worldspace.c $(RT)/wubu_gamemode.c \
               $(RT)/wubu_steaminput.c $(KERNEL)/wubu_evdev.c \
               $(AUDIO)/wubu_audio_backend.c $(AUDIO)/wubu_audio_sink.c $(AUDIO)/bigmac/bigmac_out.c $(AUDIO_OBJS) \
               $(AUDIO)/wubu_glott.c $(AUDIO)/glott/wubuglott_glottis.c \
               $(AUDIO)/glott/wubuglott_network.c $(AUDIO)/glott/wubuglott_decimator.c \
               $(AUDIO)/glott/wubuglott_voice.c \
               $(KERNEL)/memory.c $(KERNEL)/wubu_bonzi_study.c $(KERNEL)/wubu_psych.c $(KERNEL)/wubu_sha256.c \
               $(FRAMEWORK)/wubufx_apps.c $(FRAMEWORK)/wubufx.c \
               src/gui/vws/externals/wet/wet_app.c src/gui/vws/externals/wet/wet_backend_vbe.c \
               src/gui/vws/externals/wet/wet_controls.c src/gui/vws/externals/wet/wet_input.c \
               src/gui/vws/externals/wet/wet_layout.c src/gui/vws/externals/wet/wet_lua.c \
               src/gui/vws/externals/wet/wet_render.c src/gui/vws/externals/wet/wet_widget.c \
               $(LUAGUI_LV_SRCS) src/brain/src/wubu_uuid.c
HOSTED_INC  = -I$(HOSTED) -I$(LUAWWW) -Isrc/gui/vws/externals/wet \
              -Isrc/gui/vws/externals/wet/externals/wua/src
hosted: $(HOSTED)/hosted.c $(HOSTED_SUBSYS) $(HOSTED_OBJS_LIST) $(RT_OBJS) $(GUI_OBJS) $(APP_OBJS) $(COMP_OBJS) $(JIT_SRCS)
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) -include wubu_gnu_compat.h -DWUBU_EDR_AGENT -DVBE_HOSTED -DWUBU_NO_LIBM -I$(GUI) -I$(RT) -I$(KERNEL) -I$(HOSTED) -I$(BRIDGE) -I$(COMP) -I$(JIT) -I$(APPS) -I$(APPS)/calc -I$(APPS)/control -I$(APPS)/notepad -I$(TOOLS) -c $(HOSTED)/hosted.c -o /tmp/hosted_main.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(HOSTED) -x c -c $(HOSTED)/xdg-shell-private.code -o /tmp/xdg-shell-private.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(HOSTED) -x c -c $(HOSTED)/viewporter-private.code -o /tmp/viewporter-private.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(HOSTED) -x c -c $(HOSTED)/primary-selection-private.code -o /tmp/primary-selection-private.o
	$(CC) -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -I$(RT) -include wubu_gnu_compat.h -DWUBU_EDR_AGENT -DVBE_HOSTED -DWUBU_NO_LIBM -I$(GUI) -I$(RT) -I$(KERNEL) $(HOSTED_INC) -I$(BRIDGE) -I$(COMP) -I$(JIT) -I$(APPS) -I$(APPS)/calc -I$(APPS)/control -I$(APPS)/notepad -I$(TOOLS) \
	/tmp/hosted_main.o $(HOSTED_SUBSYS) $(HOSTED_EXTRA) src/kernel/vbe.c $(KERNEL)/wubu_kvfs.c $(KERNEL)/wubu_world.c $(KERNEL)/input.c $(HOSTED)/wubu_hosted_hw.c $(RT)/wubu_ct_isolate_cgroup.c $(BRIDGE)/bridge.c $(RT)/wubu_ns_kv.c \
	/tmp/xdg-shell-private.o /tmp/viewporter-private.o /tmp/primary-selection-private.o $(HOSTED)/primary-selection-private.c \
	$(HOSTED_OBJS_LIST) $(RT_OBJS) $(EDR_SRC) $(APPS)/edr_dash.o \
	$(RT)/wubu_onboarding.c $(GUI)/shell/wubu_shell_greeter.o \
	$(HOSTED)/hosted_agent_channel.c \
	$(RT)/styxfs_vfs.o $(RT)/styxfs_callbacks.o $(RT)/styxfs_posix.o $(RT)/wubu_fs_util.o $(RT)/wubu_archd_fs.o $(RT)/wubu_archd_svc.o $(RT)/wubu_archd_svc_super.o \
	$(GUI)/wubu_ui_hosted.o \
	$(RT)/wubu_holyd_agi.o $(RT)/wubu_holyd.o $(RT)/wubu_holyd_session.o $(RT)/wubu_holyd_exec.o $(RT)/wubu_holyd_repl.o $(RT)/wubu_holyd_event.o $(RT)/wubu_holyd_save.o $(RT)/wubu_holyd_input.o $(RT)/wubu_holyd_window.o $(RT)/wubu_holyd_9p.o $(RT)/wubu_holyd_lifecycle.o \
	$(GUI_OBJS) $(APP_OBJS) $(COMP_OBJS) $(JIT_SRCS) src/kernel/wubu_gaad.c src/kernel/wubu_math.c $(TOOLS)/screenshot.c \
	$(RT)/oci/oci_blob_store.o $(RT)/oci/oci_cleanup.o $(RT)/oci/oci_convert.o $(RT)/oci/oci_descriptor.o $(RT)/oci/oci_hooks.o $(RT)/oci/oci_http_client.o $(RT)/oci/oci_image_config.o $(RT)/oci/oci_image_index.o $(RT)/oci/oci_image_manifest.o $(RT)/oci/oci_media_types.o $(RT)/oci/oci_registry.o $(RT)/oci/oci_runtime_spec.o \
	$(APPS)/calc/calc.o $(APPS)/calc/calc_math.o $(APPS)/control/control.o $(APPS)/notepad.c $(APPS)/notepad.o \
	-o $(HOSTED_BIN) -no-pie -Wl,--allow-multiple-definition -fopenmp -lwayland-client -lxkbcommon -lm -lsqlite3 -lzstd -lz -ldl -ljson-c -lvulkan -lgbm -ldrm -lpulse-simple -lpulse -lasound
	@echo "✅ Hosted binary built: ./src/hosted/wubu"

bear: $(BEAR_OBJS)
	@echo "✅ Bear RL layer built"

# ── Subsystem Targets ──────────────────────────────────────────────────────
kernel: $(KERNEL_OBJS) $(KERNEL)/crt0.o $(KERNEL)/metal_main.o
	$(CC) $(CFLAGS) -DMYSEED_METAL -DWUBU_NO_LIBM -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -Wl,-no-pie \
		-T $(KERNEL)/kernel.ld \
		$(KERNEL)/crt0.o $(KERNEL)/metal_main.o $(KERNEL_OBJS) \
		-o $(KERNEL)/kernel.elf
	@echo "✅ Bare-metal kernel.elf built"

jit: $(JIT_OBJS)
	@echo "✅ JIT built"

compiler: $(COMP_OBJS)
	@echo "✅ HolyC compiler built"

runtime: $(RT_OBJS)
	@echo "✅ WuBuOS runtime built"

# The regex engine is a submodule (Waefrebeorn/wubugrep) -- ONE owner, no
# second copy. The old flat vendored .c files are gone.
WUBUGREP_SRC = $(TOOLS)/wubugrep
WUBUGREP_OBJS = $(WUBUGREP_SRC)/wubugrep.o $(WUBUGREP_SRC)/wubre.o \
		$(WUBUGREP_SRC)/wubre_compile.o $(WUBUGREP_SRC)/wubre_dfa.o \
		$(WUBUGREP_SRC)/wubre_bre.o $(WUBUGREP_SRC)/wubre_match.o \
		$(WUBUGREP_SRC)/wubre_litpref.o $(WUBUGREP_SRC)/wubre_simd.o

wubugrep: $(WUBUGREP_OBJS)
	$(CC) $(CFLAGS) $(WUBUGREP_OBJS) -o build/wubugrep

# Fully static build, as documented in src/tools/README_wubugrep.md.
wubugrep_static: $(WUBUGREP_OBJS)
	$(CC) $(CFLAGS) -static $(WUBUGREP_OBJS) -o build/wubugrep.static

tools: $(TOOLS_OBJS)
	@echo "✅ WuBuOS tools built"

gui: $(GUI_OBJS)
	@echo "✅ GUI built"

bridge: $(BRIDGE_OBJS)
	@echo "✅ Bridge built"

apps: $(APP_OBJS) $(APP_RT_OBJS)
	@echo "✅ Apps built"

worldsim: $(WS_OBJS)
	@echo "✅ WorldSim built"

shell: $(SHELL_DIR)/wubu_shell
	@echo "✅ Shell built (./src/shell/wubu_shell)"

$(SHELL_DIR)/wubu_shell: $(SHELL_OBJS)
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) \
		$(SHELL_OBJS) -o $@

bear_train: $(BEAR_OBJS) $(BEAR)/bear_train.o
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) \
		$(BEAR)/bear_arena.o $(BEAR)/bear_env.o $(BEAR)/bear_env_npole.o $(BEAR)/bear_nn_policy.o $(BEAR)/bear_nn_value.o $(BEAR)/bear_nn_ckpt.o $(BEAR)/bear_ppo_traj.o $(BEAR)/bear_ppo_loss.o $(BEAR)/bear_ppo_trainer.o $(BEAR)/bear_opt.o $(BEAR)/bear_cudnn.o $(BEAR)/bear_cudnn_cublas.o $(BEAR)/bear_cudnn_cuda.o $(BEAR)/bear_train.o $(BEAR)/bear_vulkan_soft.o \
		-lm -o $(BEAR)/bear_train
	@echo "✅ Bear RL training binary built (./src/bear/bear_train)"

# ── Bare-metal boot / QEMU validation harness ──────────────────────────────
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

# ── GRUB multiboot bootable image (the deployable interboot artifact) ──────
# kernel.elf carries a real multiboot header (0x1BADB002) in crt0.S, so GRUB
# boots it directly -- same path QEMU's `-kernel` uses, and machine-ready on
# bare metal, QEMU/KVM, and any OS that runs GRUB.  This is the image to ship
# for the interboot deployment strat (Linux / Windows / macOS all boot it).
# ── HolyD-built bootable image (the full-circle capper) ─────────────────
# Boots an image that HolyD itself compiled (no gcc kernel in the loop) through
# the SAME GRUB/multiboot + QEMU path as `make iso`.  The emitter lives in the
# compiler repo (src/compiler symlink -> wubunos); we build it there, emit a
# bootable kernel from a one-line source, pack it, boot it, and assert the boot
# trace (entry 'Z', long-mode 'Y', 'H', and the result byte 0x2a='*').
#   make holyd-verify
holyd-verify:
	$(MAKE) -C $(COMP) holyd_emit_elf
	printf 'int main() { return 42; }' > $(CACHE)/holyd_boot.hc
	$(COMP)/tools/holyd_emit_elf --boot $(CACHE)/holyd_boot.hc $(CACHE)/holyd_boot.elf
	rm -rf $(CACHE)/holyd-iso-stage && mkdir -p $(CACHE)/holyd-iso-stage/boot/grub
	cp $(CACHE)/holyd_boot.elf $(CACHE)/holyd-iso-stage/boot/boot.wubu
	printf 'set timeout=0\nset default=0\nmenuentry "WuBuOS (HolyD)" { multiboot /boot/boot.wubu\nboot\n}\n' > $(CACHE)/holyd-iso-stage/boot/grub/grub.cfg
	grub-mkrescue -o $(CACHE)/holyd.iso $(CACHE)/holyd-iso-stage >/dev/null 2>&1
	rm -f /tmp/holyd_serial.bin
	timeout 20 qemu-system-x86_64 -cdrom $(CACHE)/holyd.iso -serial file:/tmp/holyd_serial.bin \
		-display none -m 128 -no-reboot >/dev/null 2>&1 || true
	python3 -c 'import sys; s=open("/tmp/holyd_serial.bin","rb").read(); sys.exit(0 if b"Z" in s and b"Y" in s and b"0000002A" in s else 1)' 2>/dev/null && \
	  echo "✅ HOLYD-VERIFY: compiler-built image boots via make-style QEMU (Z,Y,hex 0000002A=42)" || \
	  { echo "❌ HOLYD-VERIFY: missing boot trace ($$(wc -c < /tmp/holyd_serial.bin 2>/dev/null) bytes)"; exit 1; }
	rm -f $(CACHE)/holyd_boot.hc $(CACHE)/holyd_boot.elf $(CACHE)/holyd.iso /tmp/holyd_serial.bin
	rm -rf $(CACHE)/holyd-iso-stage

iso: $(KERNEL)/kernel.elf
	rm -rf $(CACHE)/iso-stage && mkdir -p $(CACHE)/iso-stage/boot/grub
	cp $(KERNEL)/kernel.elf $(CACHE)/iso-stage/boot/wubuos.elf
	printf 'set timeout=2\nset default=0\nmenuentry "WuBuOS" {\n    multiboot /boot/wubuos.elf\n    boot\n}\n' > $(CACHE)/iso-stage/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL)/wubuos.iso $(CACHE)/iso-stage
	@echo "✅ Machine-ready bootable image: ./src/kernel/wubuos.iso"

qemu: iso
	@echo "▶ Booting WuBuOS kernel in QEMU from the GRUB multiboot image..."
	qemu-system-x86_64 -cdrom $(KERNEL)/wubuos.iso -serial stdio \
		-display none -m 128 -no-reboot \
		-device isa-debug-exit,iobase=0xf4 || true
