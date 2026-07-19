# VSL Vulkan + GPU Build System
# Builds: libwubu_vsl_vulkan.so, libwubu_vsl_gpu.so, test programs

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC \
         -I/home/wubu/wubuos/vsl/vulkan \
         -I/home/wubu/wubuos/vsl/gpu \
         -I/usr/include/drm \
         -I/usr/include/libdrm \
         -D_GNU_SOURCE
LDFLAGS = -shared -ldl -lvulkan -ldrm

# For bare metal ZealOS cross-compile:
# CFLAGS += -target x86_64-wubuos-zeal -nostdlib -nostartfiles
# LDFLAGS = -shared -nostdlib

BUILD_DIR = /home/wubu/wubuos/build
VULKAN_SRC = /home/wubu/wubuos/vsl/vulkan/wubu_vsl_vulkan.c
GPU_SRC = /home/wubu/wubuos/vsl/gpu/wubu_vsl_gpu.c

VULKAN_OBJ = $(BUILD_DIR)/wubu_vsl_vulkan.o
GPU_OBJ = $(BUILD_DIR)/wubu_vsl_gpu.o

VULKAN_LIB = $(BUILD_DIR)/libwubu_vsl_vulkan.so
GPU_LIB = $(BUILD_DIR)/libwubu_vsl_gpu.so

all: $(VULKAN_LIB) $(GPU_LIB) test_vulkan test_gpu

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VULKAN_OBJ): $(VULKAN_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(GPU_OBJ): $(GPU_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(VULKAN_LIB): $(VULKAN_OBJ)
	$(CC) $(LDFLAGS) $< -o $@

$(GPU_LIB): $(GPU_OBJ)
	$(CC) $(LDFLAGS) $< -o $@

# Test programs
test_vulkan: $(VULKAN_LIB) /home/wubu/wubuos/vsl/tests/test_vulkan.c
	$(CC) $(CFLAGS) /home/wubu/wubuos/vsl/tests/test_vulkan.c -L$(BUILD_DIR) -lwubu_vsl_vulkan -lvulkan -ldl -o $(BUILD_DIR)/test_vulkan

test_gpu: $(GPU_LIB) /home/wubu/wubuos/vsl/tests/test_gpu.c
	$(CC) $(CFLAGS) /home/wubu/wubuos/vsl/tests/test_gpu.c -L$(BUILD_DIR) -lwubu_vsl_gpu -ldrm -o $(BUILD_DIR)/test_gpu

clean:
	rm -rf $(BUILD_DIR)

install: $(VULKAN_LIB) $(GPU_LIB)
	cp $(VULKAN_LIB) /usr/local/lib/
	cp $(GPU_LIB) /usr/local/lib/
	cp /home/wubu/wubuos/vsl/vulkan/wubu_vsl_vulkan.h /usr/local/include/
	cp /home/wubu/wubuos/vsl/gpu/wubu_vsl_gpu.h /usr/local/include/
	ldconfig

.PHONY: all clean install test_vulkan test_gpu