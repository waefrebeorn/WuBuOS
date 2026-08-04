# ── Build Targets ────────────────────────────────────────────────
#
# Each subsystem builds its object list via the pattern rules in
# mk/pattern-rules.mk. The `all` umbrella links them in dependency order:
# kernel first (no deps), then compiler/jit (dep: kernel), runtime (dep: rt),
# tools, gui (dep: kernel + rt), bridge, apps (dep: all), worldsim, metal,
# audio, shell, bear, and the hosted Wayland objects.

.PHONY: all clean test kernel jit compiler runtime tools gui bridge apps worldsim firmware uefi test_uefi test_agi_metal docs shell bear hosted_objs metal audio

all: kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs
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

# ── Metal / Audio / Hosted ────────────────────────────────────────────────
metal: $(METAL_OBJS)
	@echo "✅ Metal layer built"

audio: $(AUDIO_OBJS)
	@echo "✅ Audio engine built"

hosted_objs: $(HOSTED_OBJS_LIST)
	@echo "✅ Hosted objects built"

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

qemu: $(KERNEL)/disk.img
	@echo "▶ Booting WuBuOS kernel in QEMU via bootsector floppy (true bare-metal path)..."
	qemu-system-x86_64 -fda $(KERNEL)/disk.img -serial stdio \
		-display none -m 128 -no-reboot \
		-device isa-debug-exit,iobase=0xf4 || true
