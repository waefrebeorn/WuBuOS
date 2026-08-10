# ── Pattern Rules ────────────────────────────────────────────────
# Compilation rules for each source directory.

# Header dependency tracking: every .c compile now emits a .d file (via -MMD
# -MP in the pattern rules). Including them makes `make` rebuild an object
# whenever any header it includes changes -- previously a header edit left
# stale .o files linked into test binaries, causing phantom test failures.
-include $(shell find src -name '*.d' 2>/dev/null)

# Kernel objects: freestanding, bare-metal, real asm context switch
KERNEL_CFLAGS = -DMYSEED_METAL -DWUBU_BAREMETAL=1 -DWUBU_NO_LIBM

$(KERNEL)/%.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

# Assembly files
$(KERNEL)/%.o: $(KERNEL)/%.S
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

# Special: libc.c needs kernel flags
$(KERNEL)/libc.o: $(KERNEL)/libc.c
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel -I$(KERNEL) -MMD -MP -c $< -o $@

# Runtime objects
$(RT)/%.o: $(RT)/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/vsl -I$(RT)/oci -I$(BRIDGE) -I$(KERNEL) -MMD -MP -c $< -o $@

# JIT objects
$(JIT)/%.o: $(JIT)/%.c
	$(CC) $(CFLAGS) -I$(JIT) -MMD -MP -c $< -o $@

# GUI objects
$(GUI)/%.o: $(GUI)/%.c
	$(CC) $(CFLAGS) -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) `pkg-config --cflags wlroots vulkan gbm libdrm 2>/dev/null` -MMD -MP -c $< -o $@

# Special rule for wubu_ui_hosted.o
$(GUI)/wubu_ui_hosted.o: $(GUI)/wubu_ui.c
	$(CC) $(CFLAGS) -DWUBU_EDR_AGENT -I$(GUI) -I$(KERNEL) -I$(HOSTED) -I$(RT) `pkg-config --cflags wlroots vulkan gbm libdrm 2>/dev/null` -MMD -MP -c $< -o $@

# Bridge objects
$(BRIDGE)/%.o: $(BRIDGE)/%.c
	$(CC) $(CFLAGS) -I$(BRIDGE) -I$(KERNEL) -I$(WS) -MMD -MP -c $< -o $@

# App objects
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

# WorldSim objects
$(WS)/%.o: $(WS)/%.c
	$(CC) $(CFLAGS) -I$(WS) -I$(KERNEL) -MMD -MP -c $< -o $@

# Compiler objects
$(COMP)/%.o: $(COMP)/%.c
	$(CC) $(CFLAGS) -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# VSL submodule objects
$(RT)/vsl/%.o: $(RT)/vsl/%.c
	$(CC) $(CFLAGS) -DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# OCI submodule objects
$(RT)/oci/%.o: $(RT)/oci/%.c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/oci -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# Container submodule objects
$(RT)/container/%.o: $(RT)/container/%..c
	$(CC) $(CFLAGS) -I$(RT) -I$(RT)/container -I$(COMP) -I$(JIT) -MMD -MP -c $< -o $@

# Tools objects
$(TOOLS)/%.o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) -I$(TOOLS) -MMD -MP -c $< -o $@

# Hosted objects
$(HOSTED)/%.o: $(HOSTED)/%.c
	$(CC) $(CFLAGS) -I$(HOSTED) -I$(KERNEL) -I$(RT) -I$(GUI) -I$(BRIDGE) -MMD -MP -c $< -o $@

# DRM/KMS backend
$(HOSTED)/wubu_metal_drm.o: $(HOSTED)/wubu_metal_drm.c
	$(CC) $(CFLAGS) -DWUBU_USE_DRM -I$(HOSTED) -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@

# Audio objects
$(AUDIO)/%.o: $(AUDIO)/%.c
	$(CC) $(CFLAGS) -I$(AUDIO) -I$(KERNEL) -I$(RT) -MMD -MP -c $< -o $@

# Bear objects
$(BEAR)/%.o: $(BEAR)/%.c
	$(CC) $(CFLAGS) -I$(BEAR) -I$(RT) -I$(KERNEL) -MMD -MP -c $< -o $@

# CUDA files
$(BEAR)/%.o: $(BEAR)/%.cu
	nvcc -std=c++17 -I$(BEAR) -I$(RT) -I$(KERNEL) -c $< -o $@

# Shell objects
$(SHELL_DIR)/%.o: $(SHELL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(SHELL_DIR) -I$(KERNEL) -I$(GUI) -I$(RT) -I$(BRIDGE) -I$(HOSTED) -MMD -MP -c $< -o $@

# Special hosted objects
$(HOSTED)/xdg-shell-private.o: $(HOSTED)/xdg-shell-private.c
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -MMD -MP -c $< -o $@

$(HOSTED)/primary-selection-private.o: $(HOSTED)/primary-selection-private.c
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -MMD -MP -c $< -o $@
