// ============================================================================
// VSL GPU Driver Implementation - Linux DRM/KMS Backend (Simplified)
// 
// Maps VSL GPU API to Linux DRM ioctls for WSL/hosted development.
// On bare metal ZealOS, this would be replaced with VSL syscalls.

#define _GNU_SOURCE
#include "wubu_vsl_gpu.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_fourcc.h>
// ============================================================================
// Internal Structures (minimal - mostly in header)
// ============================================================================

struct WubuVslGpuDevice {
    int fd;
    WubuGpuDeviceInfo info;
    bool is_virtio;
    bool has_dmabuf;
    bool has_syncobj;
    uint32_t max_contexts;
};

struct WubuVslGpuContext {
    WubuVslGpuDevice *device;
    uint32_t ctx_id;
    uint32_t engine_flags;
};

struct WubuVslGpuFence {
    WubuVslGpuDevice *device;
    uint32_t syncobj_handle;
    bool is_timeline;
};

// ============================================================================
// Helper Functions
// ============================================================================

static int drm_ioctl(int fd, unsigned long request, void *arg) {
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

static WubuGpuType detect_gpu_type(int fd) {
    drm_version_t version = {0};
    char name_buf[64];
    version.name_len = sizeof(name_buf);
    version.name = name_buf;
    if (drm_ioctl(fd, DRM_IOCTL_VERSION, &version) == 0) {
        if (strstr(name_buf, "virtio")) return WUBU_GPU_TYPE_VIRTIO;
        if (strstr(name_buf, "nouveau")) return WUBU_GPU_TYPE_NVIDIA;
        if (strstr(name_buf, "amdgpu")) return WUBU_GPU_TYPE_AMD;
        if (strstr(name_buf, "i915")) return WUBU_GPU_TYPE_INTEL;
    }
    return WUBU_GPU_TYPE_UNKNOWN;
}

static const char *gpu_type_name(WubuGpuType type) {
    switch (type) {
        case WUBU_GPU_TYPE_NVIDIA: return "NVIDIA (Nouveau)";
        case WUBU_GPU_TYPE_AMD: return "AMD (amdgpu)";
        case WUBU_GPU_TYPE_INTEL: return "Intel (i915)";
        case WUBU_GPU_TYPE_VIRTIO: return "VirtIO-GPU";
        case WUBU_GPU_TYPE_SOFTWARE: return "Software";
        default: return "Unknown";
    }
}

// ============================================================================
// Device Management
// ============================================================================

WubuVslGpuDevice *wubu_vsl_gpu_open(const char *device_path) {
    const char *paths[] = {
        device_path,
        "/dev/dri/renderD128",
        "/dev/dri/renderD129",
        "/dev/dri/card0",
        "/dev/dri/card1",
        "/dev/virtgpu",
        NULL
    };
    
    int fd = -1;
    for (int i = 0; paths[i]; i++) {
        fd = open(paths[i], O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            fprintf(stderr, "[VSL GPU] Opened %s\n", paths[i]);
            break;
        }
    }
    
    if (fd < 0) {
        fprintf(stderr, "[VSL GPU] Failed to open any DRM device: %s\n", strerror(errno));
        return NULL;
    }
    
    // Check capabilities
    uint64_t has_dumb = 0, has_prime = 0, has_syncobj = 0;
    drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
    drmGetCap(fd, DRM_CAP_PRIME, &has_prime);
    drmGetCap(fd, DRM_CAP_SYNCOBJ, &has_syncobj);
    
    WubuVslGpuDevice *dev = calloc(1, sizeof(WubuVslGpuDevice));
    dev->fd = fd;
    dev->is_virtio = (detect_gpu_type(fd) == WUBU_GPU_TYPE_VIRTIO);
    dev->has_dmabuf = has_prime;
    dev->has_syncobj = has_syncobj;
    dev->max_contexts = 64;
    
    // Get device info
    drm_version_t version = {0};
    char name_buf[64], date_buf[64], desc_buf[128];
    version.name_len = sizeof(name_buf);
    version.name = name_buf;
    version.date_len = sizeof(date_buf);
    version.date = date_buf;
    version.desc_len = sizeof(desc_buf);
    version.desc = desc_buf;
    drm_ioctl(fd, DRM_IOCTL_VERSION, &version);
    
    dev->info.type = detect_gpu_type(fd);
    strncpy(dev->info.name, name_buf, sizeof(dev->info.name)-1);
    snprintf(dev->info.driver_version, sizeof(dev->info.driver_version), 
             "%d.%d.%d", version.version_major, version.version_minor, version.version_patchlevel);
    dev->info.vendor_id = 0;
    dev->info.device_id = 0;
    dev->info.vram_size = 0;
    dev->info.gtt_size = 0;
    dev->info.has_render = has_dumb;
    dev->info.has_compute = has_dumb;
    dev->info.has_video_decode = false;
    dev->info.has_video_encode = false;
    dev->info.supports_dmabuf = has_prime;
    dev->info.supports_explicit_sync = has_syncobj;
    dev->info.supports_vm_bind = false;
    dev->info.max_contexts = dev->max_contexts;
    
    return dev;
}

void wubu_vsl_gpu_close(WubuVslGpuDevice *device) {
    if (!device) return;
    close(device->fd);
    free(device);
}

VkResult wubu_vsl_gpu_get_info(WubuVslGpuDevice *device, WubuGpuDeviceInfo *info) {
    if (!device || !info) return VK_ERROR_INITIALIZATION_FAILED;
    *info = device->info;
    return VK_SUCCESS;
}

// ============================================================================
// Buffer Management (using DRM dumb buffers)
// ============================================================================

WubuVslGpuBuffer *wubu_vsl_gpu_buffer_create(WubuVslGpuDevice *device, const WubuGpuBufferCreateInfo *info) {
    if (!device || !info || info->size == 0) return NULL;
    
    uint64_t size = (info->size + info->alignment - 1) & ~(info->alignment - 1);
    
    struct drm_mode_create_dumb create = {0};
    create.width = size;
    create.height = 1;
    create.bpp = 8;
    create.flags = 0;
    
    if (drm_ioctl(device->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
        fprintf(stderr, "[VSL GPU] Failed to create dumb buffer: %s\n", strerror(errno));
        return NULL;
    }
    
    WubuVslGpuBuffer *buf = calloc(1, sizeof(WubuVslGpuBuffer));
    buf->device = device;
    buf->handle = create.handle;
    buf->size = size;
    buf->offset = 0;
    buf->dmabuf_fd = -1;
    buf->cpu_mapping = NULL;
    buf->heap = info->preferred_heap;
    buf->flags = info->flags;
    
    if ((info->flags & WUBU_GPU_BUFFER_FLAG_SHARED) && device->has_dmabuf) {
        struct drm_prime_handle prime = {0};
        prime.handle = create.handle;
        prime.flags = DRM_RDWR | DRM_CLOEXEC;
        if (drm_ioctl(device->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) == 0) {
            buf->dmabuf_fd = prime.fd;
        }
    }
    
    return buf;
}

void wubu_vsl_gpu_buffer_destroy(WubuVslGpuBuffer *buffer) {
    if (!buffer) return;
    
    if (buffer->cpu_mapping) {
        munmap(buffer->cpu_mapping, buffer->size);
    }
    
    if (buffer->dmabuf_fd >= 0) {
        close(buffer->dmabuf_fd);
    }
    
    struct drm_mode_destroy_dumb destroy = {0};
    destroy.handle = buffer->handle;
    drm_ioctl(buffer->device->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    
    free(buffer);
}

VkResult wubu_vsl_gpu_buffer_get_info(WubuVslGpuBuffer *buffer, WubuGpuBufferInfo *info) {
    if (!buffer || !info) return VK_ERROR_INITIALIZATION_FAILED;
    
    info->size = buffer->size;
    info->offset = buffer->offset;
    info->handle = buffer->handle;
    info->dmabuf_fd = buffer->dmabuf_fd;
    info->cpu_mapping = buffer->cpu_mapping;
    info->heap = buffer->heap;
    
    return VK_SUCCESS;
}

VkResult wubu_vsl_gpu_buffer_map(WubuVslGpuBuffer *buffer, void **ptr) {
    if (!buffer || buffer->cpu_mapping) return VK_ERROR_MEMORY_MAP_FAILED;
    
    struct drm_mode_map_dumb map = {0};
    map.handle = buffer->handle;
    if (drm_ioctl(buffer->device->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    
    void *mapping = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, 
                         buffer->device->fd, map.offset);
    if (mapping == MAP_FAILED) {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    
    buffer->cpu_mapping = mapping;
    *ptr = mapping;
    return VK_SUCCESS;
}

VkResult wubu_vsl_gpu_buffer_unmap(WubuVslGpuBuffer *buffer) {
    if (!buffer || !buffer->cpu_mapping) return VK_SUCCESS;
    
    munmap(buffer->cpu_mapping, buffer->size);
    buffer->cpu_mapping = NULL;
    return VK_SUCCESS;
}

int wubu_vsl_gpu_buffer_export_dmabuf(WubuVslGpuBuffer *buffer) {
    if (!buffer || !buffer->device->has_dmabuf) return -1;
    
    if (buffer->dmabuf_fd >= 0) {
        return dup(buffer->dmabuf_fd);
    }
    
    struct drm_prime_handle prime = {0};
    prime.handle = buffer->handle;
    prime.flags = DRM_RDWR | DRM_CLOEXEC;
    if (drm_ioctl(buffer->device->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) == 0) {
        buffer->dmabuf_fd = prime.fd;
        return dup(prime.fd);
    }
    return -1;
}

WubuVslGpuBuffer *wubu_vsl_gpu_buffer_import_dmabuf(WubuVslGpuDevice *device, int fd, uint64_t size) {
    if (!device || !device->has_dmabuf || fd < 0) return NULL;
    
    struct drm_prime_handle prime = {0};
    prime.fd = fd;
    prime.flags = DRM_RDWR;
    if (drm_ioctl(device->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime) != 0) {
        return NULL;
    }
    
    WubuVslGpuBuffer *buf = calloc(1, sizeof(WubuVslGpuBuffer));
    buf->device = device;
    buf->handle = prime.handle;
    buf->size = size;
    buf->dmabuf_fd = dup(fd);
    buf->heap = WUBU_GPU_MEMORY_HEAP_VRAM;
    
    return buf;
}

// ============================================================================
// Context Management
// ============================================================================

WubuVslGpuContext *wubu_vsl_gpu_context_create(WubuVslGpuDevice *device, uint32_t engine_flags) {
    if (!device) return NULL;
    
    WubuVslGpuContext *ctx = calloc(1, sizeof(WubuVslGpuContext));
    ctx->device = device;
    ctx->ctx_id = 1;
    ctx->engine_flags = engine_flags;
    return ctx;
}

void wubu_vsl_gpu_context_destroy(WubuVslGpuContext *context) {
    if (context) free(context);
}

VkResult wubu_vsl_gpu_context_submit(WubuVslGpuContext *context, const WubuGpuSubmitInfo *submit) {
    if (!context || !submit) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    
    // Simplified: just wait on wait fences and signal signal fences
    for (uint32_t i = 0; i < submit->wait_sync_count; i++) {
        wubu_vsl_gpu_fence_wait((WubuVslGpuFence*)submit->wait_fences[i], UINT64_MAX);
    }
    for (uint32_t i = 0; i < submit->signal_sync_count; i++) {
        wubu_vsl_gpu_fence_signal((WubuVslGpuFence*)submit->signal_fences[i], 
            submit->signal_values ? submit->signal_values[i] : 1);
    }
    
    return VK_SUCCESS;
}

// ============================================================================
// Synchronization (using DRM syncobj - simplified)
// ============================================================================
// Synchronization (using DRM syncobj - simplified for older kernels)
// ============================================================================

WubuVslGpuFence *wubu_vsl_gpu_fence_create(WubuVslGpuDevice *device, const WubuGpuSyncCreateInfo *info) {
    if (!device || !device->has_syncobj) return NULL;
    
    struct drm_syncobj_create create = {0};
    create.flags = 0; // Binary fence only for compatibility
    
    if (drm_ioctl(device->fd, DRM_IOCTL_SYNCOBJ_CREATE, &create) != 0) {
        return NULL;
    }
    
    WubuVslGpuFence *fence = calloc(1, sizeof(WubuVslGpuFence));
    fence->device = device;
    fence->syncobj_handle = create.handle;
    fence->is_timeline = false; // Only binary fences for now
    
    return fence;
}

void wubu_vsl_gpu_fence_destroy(WubuVslGpuFence *fence) {
    if (!fence) return;
    
    struct drm_syncobj_destroy destroy = {0};
    destroy.handle = fence->syncobj_handle;
    drm_ioctl(fence->device->fd, DRM_IOCTL_SYNCOBJ_DESTROY, &destroy);
    
    free(fence);
}

VkResult wubu_vsl_gpu_fence_wait(WubuVslGpuFence *fence, uint64_t timeout_ns) {
    if (!fence) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    
    struct drm_syncobj_wait wait = {0};
    wait.handles = (uint64_t)&fence->syncobj_handle;
    wait.count_handles = 1;
    wait.timeout_nsec = timeout_ns;
    wait.flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
    
    int ret = drm_ioctl(fence->device->fd, DRM_IOCTL_SYNCOBJ_WAIT, &wait);
    if (ret == -1) {
        if (errno == ETIMEDOUT) return VK_TIMEOUT;
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult wubu_vsl_gpu_fence_reset(WubuVslGpuFence *fence) {
    if (!fence) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    
    struct drm_syncobj_array array = {0};
    array.handles = (uint64_t)&fence->syncobj_handle;
    array.count_handles = 1;
    drm_ioctl(fence->device->fd, DRM_IOCTL_SYNCOBJ_RESET, &array);
    return VK_SUCCESS;
}

uint64_t wubu_vsl_gpu_fence_get_value(WubuVslGpuFence *fence) {
    // Binary fence - always 0
    (void)fence;
    return 0;
}

VkResult wubu_vsl_gpu_fence_signal(WubuVslGpuFence *fence, uint64_t value) {
    if (!fence) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    (void)value;
    
    struct drm_syncobj_array array = {0};
    array.handles = (uint64_t)&fence->syncobj_handle;
    array.count_handles = 1;
    drm_ioctl(fence->device->fd, DRM_IOCTL_SYNCOBJ_SIGNAL, &array);
    return VK_SUCCESS;
}

int wubu_vsl_gpu_fence_export_sync_file(WubuVslGpuFence *fence) {
    if (!fence || !fence->device->has_syncobj) return -1;
    
    // Note: syncobj export requires newer kernel
    return -1;
}

WubuVslGpuFence *wubu_vsl_gpu_fence_import_sync_file(WubuVslGpuDevice *device, int fd) {
    // Not implemented
    return NULL;
}

// ============================================================================
// Display/KMS (Placeholders)
// ============================================================================

uint32_t wubu_vsl_gpu_get_connectors(WubuVslGpuDevice *device, WubuGpuConnector **connectors) {
    (void)device;
    *connectors = NULL;
    return 0;
}

uint32_t wubu_vsl_gpu_get_modes(WubuGpuConnector *connector, WubuGpuMode **modes) {
    (void)connector;
    *modes = NULL;
    return 0;
}

VkResult wubu_vsl_gpu_atomic_commit(WubuVslGpuDevice *device, const WubuGpuAtomicCommit *commit, bool test_only) {
    (void)device; (void)commit; (void)test_only;
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

WubuVslGpuFence *wubu_vsl_gpu_page_flip(WubuVslGpuDevice *device, WubuGpuCrtc *crtc, WubuVslGpuBuffer *buffer) {
    (void)device; (void)crtc; (void)buffer;
    return NULL;
}

// ============================================================================
// Debug
// ============================================================================

void wubu_vsl_gpu_dump_state(WubuVslGpuDevice *device) {
    if (!device) return;
    
    fprintf(stderr, "\n=== VSL GPU Device ===\n");
    fprintf(stderr, "Type: %s\n", gpu_type_name(device->info.type));
    fprintf(stderr, "Name: %s\n", device->info.name);
    fprintf(stderr, "Driver: %s\n", device->info.driver_version);
    fprintf(stderr, "VRAM: %lu MB\n", device->info.vram_size / (1024*1024));
    fprintf(stderr, "GTT: %lu MB\n", device->info.gtt_size / (1024*1024));
    fprintf(stderr, "DMABUF: %s\n", device->info.supports_dmabuf ? "Yes" : "No");
    fprintf(stderr, "Syncobj: %s\n", device->info.supports_explicit_sync ? "Yes" : "No");
    fprintf(stderr, "VM Bind: %s\n", device->info.supports_vm_bind ? "Yes" : "No");
    fprintf(stderr, "Max Contexts: %u\n", device->info.max_contexts);
    fprintf(stderr, "========================\n\n");
}

const char *wubu_vsl_gpu_type_name(WubuGpuType type) {
    return gpu_type_name(type);
}