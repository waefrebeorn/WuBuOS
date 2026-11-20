# ── Pattern Rules ────────────────────────────────────────────────
# Compilation rules for each source directory.

# Header dependency tracking: every .c compile now emits a .d file (via -MMD
# -MP in the pattern rules). Including them makes `make` rebuild an object
# whenever any header it includes changes -- previously a header edit left
# stale .o files linked into test binaries, causing phantom test failures.
-include $(shell find src -name '*.d' 2>/dev/null)

# Kernel objects: freestanding, bare-metal, real asm context switch
# -fno-jump-tables: the bare-metal boot died on an emitted byte-switch jump
#   table whose slots were 0xFFFFFFFF / non-canonical (vsl boot crash at
#   `jmp *CSWTCH.21(,%rax,8)` -> #PF at 0xFFFFFFFF).  Kernel builds must not
#   rely on GCC jumping through rodata tables; compare-chains are safe.
# -march=x86-64: the kernel pins its OWN ISA baseline. Distro compilers drift
#   (Ubuntu 26.04 gcc14 defaults to x86-64-v3): an inherited -march let gcc
#   vectorize memset with vpbroadcastb/ymm — #UD on any CPU without AVX2
#   (qemu64 TCG), triple fault before the IDT exists. Caught by the multi-host
#   boot gate (CI run 44509946540, vmovd at memset+0x21). Freestanding code
#   must never inherit the build host's default architecture.
KERNEL_CFLAGS = -Iinclude -DMYSEED_METAL -DWUBU_BAREMETAL=1 -DWUBU_NO_LIBM -fno-jump-tables -march=x86-64 -mtune=generic

$(KERNEL)/%.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# CRC-32: the runtime owner (src/runtime/wubu_crc32.c, self-contained C11,
# no libc) compiled freestanding for the kernel — txfs calls it since the
# one-owner refactor (452354c) deleted the kernel-local copy.
$(KERNEL)/wubu_crc32.o: $(RT)/wubu_crc32.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@ -Iinclude

# Assembly files
$(KERNEL)/%.o: $(KERNEL)/%.S
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# Special: libc.c needs kernel flags
$(KERNEL)/libc.o: $(KERNEL)/libc.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# Runtime objects
$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/vsl -I$(RT)/oci -I$(BRIDGE) -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# The archd/holyd daemon objects carry shared service code (archd_log,
# wubu_archd_init, holyd lifecycle) AND a standalone main(). Product and
# test links supply their own main, so these two objects are built with
# WUBU_HOSTED_PRODUCT to compile the daemon main() out. Standalone daemon
# builds compile the .c directly without the flag.
$(RT)/wubu_archd_daemon.o: $(RT)/wubu_archd_daemon.c
	$(CC) $(CFLAGS) -DWUBU_HOSTED_PRODUCT -I$(RT) -I$(RT)/vsl -I$(RT)/oci -I$(BRIDGE) -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude
$(RT)/wubu_holyd_lifecycle.o: $(RT)/wubu_holyd_lifecycle.c
	$(CC) $(CFLAGS) -DWUBU_HOSTED_PRODUCT -I$(RT) -I$(RT)/vsl -I$(RT)/oci -I$(BRIDGE) -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# Spell objects.  A spell is self-contained, so it needs no -I beyond its
# own directory: quoted includes search the source's directory first.
$(SPELLS)/%.o: $(SPELLS)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@ -Iinclude

# JIT objects
$(JIT)/%.o: $(JIT)/%.c
	$(CC) $(CFLAGS) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

# GUI objects
$(GUI)/%.o: $(GUI)/%.c
	$(CC) $(CFLAGS) -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) `pkg-config --cflags wlroots vulkan gbm libdrm 2>/dev/null` -MMD -MP -c $< -o $@ -Iinclude

# Hosted signing leaves: compiled with the AUTO-DETECTED backend config.
# Separate object path so a plain test/GUI build cannot leave a stale
# no-backend object that `make hosted` would then link without the native
# core.  The generated config header is a prerequisite for rebuilds.
$(WUBU_EDDSA_BUILD_DIR)/hosted/%.o: $(GUI)/%.c $(WUBU_EDDSA_BUILD_DIR)/native_eddsa_config.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(WUBU_EDDSA_HOSTED_CFLAGS) -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) -MMD -MP -c $< -o $@ -Iinclude

# Special rule for wubu_ui_hosted.o
$(GUI)/wubu_ui_hosted.o: $(GUI)/wubu_ui.c
	$(CC) $(CFLAGS) -DWUBU_EDR_AGENT -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) `pkg-config --cflags wlroots vulkan gbm libdrm 2>/dev/null` -MMD -MP -c $< -o $@ -Iinclude

# Bridge objects
$(BRIDGE)/%.o: $(BRIDGE)/%.c
	$(CC) $(CFLAGS) -I$(BRIDGE) -I$(KERNEL) -I$(WS) -MMD -MP -c $< -o $@ -Iinclude

# App objects
$(APPS)/%.o: $(APPS)/%.c
	$(CC) $(CFLAGS) -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

$(APPS)/calc/%.o: $(APPS)/calc/%.c
	$(CC) $(CFLAGS) -I$(APPS)/calc -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

$(APPS)/notepad/%.o: $(APPS)/notepad/%.c
	$(CC) $(CFLAGS) -I$(APPS)/notepad -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

$(APPS)/cmd/%.o: $(APPS)/cmd/%.c
	$(CC) $(CFLAGS) -I$(APPS)/cmd -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

$(APPS)/taskmgr/%.o: $(APPS)/taskmgr/%.c
	$(CC) $(CFLAGS) -I$(APPS)/taskmgr -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

$(APPS)/regedit/%.o: $(APPS)/regedit/%.c
	$(CC) $(CFLAGS) -I$(APPS)/regedit -I$(APPS) -I$(JIT) -I$(GUI) -I$(KERNEL) -I$(RT) -I$(COMP) -MMD -MP -c $< -o $@ -Iinclude

# WorldSim objects
$(WS)/%.o: $(WS)/%.c
	$(CC) $(CFLAGS) -I$(WS) -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# Compiler objects
$(COMP)/%.o: $(COMP)/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

$(COMP)/jit/%.o: $(COMP)/jit/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

# tgemm AVX-512 kernel needs -mavx512; object is linked but dispatch guards runtime
$(COMP)/wubu_tgemm_avx512.o: $(COMP)/wubu_tgemm_avx512.c
	$(CC) $(CFLAGS) -mavx512f -mavx512dq -c -o $@ $< -Iinclude

# VSL submodule objects
$(RT)/vsl/%.o: $(RT)/vsl/%.c
	$(CC) $(CFLAGS) -DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl -I$(KERNEL) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

# OCI submodule objects
$(RT)/oci/%.o: $(RT)/oci/%.c
	$(CC) $(CFLAGS) $(OCI_TLS_CFLAGS) -I$(RT) -I$(RT)/oci -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

# Container submodule objects
$(RT)/container/%.o: $(RT)/container/%..c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/container -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@ -Iinclude

# Tools objects
$(WUBUGREP_SRC)/%.o: $(WUBUGREP_SRC)/%.c
	$(CC) $(CFLAGS) -I$(WUBUGREP_SRC) -MMD -MP -c $< -o $@ -Iinclude

$(TOOLS)/%.o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) -I$(TOOLS) -MMD -MP -c $< -o $@ -Iinclude

# Hosted objects
$(HOSTED)/%.o: $(HOSTED)/%.c
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -MMD -MP -c $< -o $@ -Iinclude

# DRM/KMS backend
$(HOSTED)/wubu_metal_drm.o: $(HOSTED)/wubu_metal_drm.c
	$(CC) $(CFLAGS) -DWUBU_USE_DRM -I$(HOSTED) -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@ -Iinclude

# Audio objects
$(AUDIO)/bigmac/%.o: $(AUDIO)/bigmac/%.c
	$(CC) $(CFLAGS) -I$(AUDIO)/bigmac -MMD -MP -c $< -o $@ -Iinclude

$(AUDIO)/%.o: $(AUDIO)/%.c
	$(CC) $(CFLAGS) -I$(AUDIO) -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@ -Iinclude

# Bear objects
$(BEAR)/%.o: $(BEAR)/%.c
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) -MMD -MP -c $< -o $@ -Iinclude

# CUDA files
$(BEAR)/%.o: $(BEAR)/%.cu
	nvcc -std=c++17 -I$(BEAR) -I$(RT) -I$(KERNEL) -c $< -o $@

# Shell objects
$(SHELL_DIR)/%.o: $(SHELL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) -MMD -MP -c $< -o $@ -Iinclude

# Special hosted objects
$(HOSTED)/xdg-shell-private.o: $(HOSTED)/xdg-shell-private.c
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -MMD -MP -c $< -o $@ -Iinclude

$(HOSTED)/primary-selection-private.o: $(HOSTED)/primary-selection-private.c
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -MMD -MP -c $< -o $@ -Iinclude
