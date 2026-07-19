/*
 * VSL GPU Driver Abstraction - WuBuOS GPU Driver Interface
 * 
 * This is the bare-metal kernel/userspace interface for GPU operations.
 * On Linux (WSL/hosted), this maps to DRM/KMS ioctls.
 * On ZealOS bare metal, this maps to custom GPU driver syscalls.
 * 
 * Provides:
 *   - Buffer allocation (dumb buffers, dmabuf export/import)
 *   - Command submission (render, compute, copy)
 *   - Synchronization (fences, semaphores, timeline syncobjs)
 *   - Display/KMS (modeset, page flip, atomic)
 *   - VRAM/GT memory management
 */

#ifndef WUBU_VSL_GPU_H
#define WUBU_VSL_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Vulkan result type (avoid vulkan.h dependency)
typedef int32_t VkResult;
#define VK_SUCCESS 0
#define VK_TIMEOUT 1
#define VK_ERROR_INITIALIZATION_FAILED -3
#define VK_ERROR_DEVICE_LOST -4
#define VK_ERROR_MEMORY_MAP_FAILED -7
#define VK_ERROR_INVALID_EXTERNAL_HANDLE -10
#define VK_ERROR_FEATURE_NOT_PRESENT -11

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct WubuVslGpuDevice WubuVslGpuDevice;
typedef struct WubuVslGpuBuffer WubuVslGpuBuffer;
typedef struct WubuVslGpuContext WubuVslGpuContext;
typedef struct WubuVslGpuFence WubuVslGpuFence;
typedef struct WubuVslGpuSemaphore WubuVslGpuSemaphore;
typedef struct WubuGpuConnector WubuGpuConnector;
typedef struct WubuGpuEncoder WubuGpuEncoder;
typedef struct WubuGpuCrtc WubuGpuCrtc;
typedef struct WubuGpuPlane WubuGpuPlane;

// ============================================================================
// GPU Device Types
// ============================================================================

typedef enum {
    WUBU_GPU_TYPE_UNKNOWN = 0,
    WUBU_GPU_TYPE_NVIDIA  = 1,  // Nouveau equivalent
    WUBU_GPU_TYPE_AMD     = 2,  // AMDGPU equivalent
    WUBU_GPU_TYPE_INTEL   = 3,  // i915 equivalent
    WUBU_GPU_TYPE_VIRTIO  = 4,  // VirtIO-GPU (VM/WSL)
    WUBU_GPU_TYPE_SOFTWARE = 5, // Software fallback
} WubuGpuType;

// Memory heap types
typedef enum {
    WUBU_GPU_MEMORY_HEAP_VRAM = 0,   // Device local (VRAM)
    WUBU_GPU_MEMORY_HEAP_GTT = 1,    // Host visible (GTT)
    WUBU_GPU_MEMORY_HEAP_SYSTEM = 2, // System memory
} WubuGpuMemoryHeap;

// Buffer flags
typedef enum {
    WUBU_GPU_BUFFER_FLAG_NONE = 0,
    WUBU_GPU_BUFFER_FLAG_SCANOUT = 1 << 0,      // Can be used for display
    WUBU_GPU_BUFFER_FLAG_SHARED = 1 << 1,       // Exportable as dmabuf
    WUBU_GPU_BUFFER_FLAG_CPU_VISIBLE = 1 << 2,  // Mappable to CPU
    WUBU_GPU_BUFFER_FLAG_COHERENT = 1 << 3,     // CPU-GPU coherent
    WUBU_GPU_BUFFER_FLAG_PROTECTED = 1 << 4,    // Protected content
} WubuGpuBufferFlags;

// ============================================================================
// GPU Device Info
// ============================================================================

typedef struct {
    WubuGpuType type;
    char name[64];
    char driver_version[32];
    uint32_t vendor_id;
    uint32_t device_id;
    uint64_t vram_size;
    uint64_t gtt_size;
    bool has_render;
    bool has_compute;
    bool has_video_decode;
    bool has_video_encode;
    bool supports_dmabuf;
    bool supports_explicit_sync;
    bool supports_vm_bind;
    uint32_t max_contexts;
} WubuGpuDeviceInfo;

// ============================================================================
// Buffer Struct Definition (needed by API)
// ============================================================================

struct WubuVslGpuBuffer {
    WubuVslGpuDevice *device;
    uint32_t handle;
    uint64_t size;
    uint64_t offset;
    int dmabuf_fd;
    void *cpu_mapping;
    WubuGpuMemoryHeap heap;
    uint32_t flags;
};

// ============================================================================
// Buffer Management
// ============================================================================

// Buffer create info
typedef struct {
    uint64_t size;
    uint32_t alignment;
    WubuGpuBufferFlags flags;
    WubuGpuMemoryHeap preferred_heap;
    uint32_t format;           // DRM fourcc for dmabuf
    uint64_t modifier;         // DRM format modifier
    const void *external_fd;   // For importing dmabuf
} WubuGpuBufferCreateInfo;

// Buffer info
typedef struct {
    uint64_t size;
    uint64_t offset;           // GPU virtual address offset
    uint32_t handle;           // GEM handle
    int dmabuf_fd;             // Exported dmabuf fd (-1 if not exported)
    void *cpu_mapping;         // CPU mapping if mapped
    WubuGpuMemoryHeap heap;
} WubuGpuBufferInfo;

// ============================================================================
// Command Submission
// ============================================================================

typedef enum {
    WUBU_GPU_ENGINE_RENDER = 0,
    WUBU_GPU_ENGINE_COMPUTE = 1,
    WUBU_GPU_ENGINE_COPY = 2,
    WUBU_GPU_ENGINE_VIDEO_DECODE = 3,
    WUBU_GPU_ENGINE_VIDEO_ENCODE = 4,
} WubuGpuEngine;

// Sync object types
typedef enum {
    WUBU_GPU_SYNC_FENCE = 0,       // Binary fence (signaled once)
    WUBU_GPU_SYNC_TIMELINE = 1,    // Timeline semaphore (monotonic)
    WUBU_GPU_SYNC_BINARY_SEM = 2,  // Binary semaphore
} WubuGpuSyncType;

// Sync object create info
typedef struct {
    WubuGpuSyncType type;
    uint64_t initial_value;    // For timeline semaphores
    bool exportable;           // Can export as sync_file fd
} WubuGpuSyncCreateInfo;

// Command buffer (simplified - real impl would have push constants, descriptors, etc.)
typedef struct {
    void *data;
    size_t size;
    uint32_t engine;           // WubuGpuEngine
} WubuGpuCommandBuffer;

// Submit info
typedef struct {
    uint32_t command_buffer_count;
    const WubuGpuCommandBuffer *command_buffers;
    uint32_t wait_sync_count;
    const struct WubuVslGpuFence **wait_fences;
    const uint64_t *wait_values;  // For timeline semaphores
    uint32_t signal_sync_count;
    const struct WubuVslGpuFence **signal_fences;
    const uint64_t *signal_values; // For timeline semaphores
} WubuGpuSubmitInfo;

// ============================================================================
// Display/KMS (Atomic Modeset)
// ============================================================================

typedef enum {
    WUBU_GPU_CONNECTOR_UNKNOWN = 0,
    WUBU_GPU_CONNECTOR_HDMI = 1,
    WUBU_GPU_CONNECTOR_DP = 2,
    WUBU_GPU_CONNECTOR_DVI = 3,
    WUBU_GPU_CONNECTOR_VGA = 4,
    WUBU_GPU_CONNECTOR_VIRTUAL = 5,
    WUBU_GPU_CONNECTOR_EDP = 6,
} WubuGpuConnectorType;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;  // mHz
    uint32_t flags;         // DRM_MODE_FLAG_*
    bool interlaced;
} WubuGpuMode;

typedef struct {
    WubuGpuConnector *connector;
    WubuGpuCrtc *crtc;
    WubuVslGpuBuffer *buffer;
    WubuGpuMode mode;
    uint32_t x, y;
    bool page_flip;
    WubuVslGpuFence *fence;  // Signaled on flip complete
} WubuGpuAtomicCommit;

// ============================================================================
// VSL GPU API
// ============================================================================

// Device management
WubuVslGpuDevice *wubu_vsl_gpu_open(const char *device_path);
void wubu_vsl_gpu_close(WubuVslGpuDevice *device);
VkResult wubu_vsl_gpu_get_info(WubuVslGpuDevice *device, WubuGpuDeviceInfo *info);

// Buffer management
WubuVslGpuBuffer *wubu_vsl_gpu_buffer_create(WubuVslGpuDevice *device, const WubuGpuBufferCreateInfo *info);
void wubu_vsl_gpu_buffer_destroy(WubuVslGpuBuffer *buffer);
VkResult wubu_vsl_gpu_buffer_get_info(WubuVslGpuBuffer *buffer, WubuGpuBufferInfo *info);
VkResult wubu_vsl_gpu_buffer_map(WubuVslGpuBuffer *buffer, void **ptr);
VkResult wubu_vsl_gpu_buffer_unmap(WubuVslGpuBuffer *buffer);
int wubu_vsl_gpu_buffer_export_dmabuf(WubuVslGpuBuffer *buffer);  // Returns dmabuf fd
WubuVslGpuBuffer *wubu_vsl_gpu_buffer_import_dmabuf(WubuVslGpuDevice *device, int fd, uint64_t size);

// Context management
WubuVslGpuContext *wubu_vsl_gpu_context_create(WubuVslGpuDevice *device, uint32_t engine_flags);
void wubu_vsl_gpu_context_destroy(WubuVslGpuContext *context);
VkResult wubu_vsl_gpu_context_submit(WubuVslGpuContext *context, const WubuGpuSubmitInfo *submit);

// Synchronization
WubuVslGpuFence *wubu_vsl_gpu_fence_create(WubuVslGpuDevice *device, const WubuGpuSyncCreateInfo *info);
void wubu_vsl_gpu_fence_destroy(WubuVslGpuFence *fence);
VkResult wubu_vsl_gpu_fence_wait(WubuVslGpuFence *fence, uint64_t timeout_ns);
VkResult wubu_vsl_gpu_fence_reset(WubuVslGpuFence *fence);
uint64_t wubu_vsl_gpu_fence_get_value(WubuVslGpuFence *fence);  // For timeline
VkResult wubu_vsl_gpu_fence_signal(WubuVslGpuFence *fence, uint64_t value); // For timeline
int wubu_vsl_gpu_fence_export_sync_file(WubuVslGpuFence *fence); // Returns sync_file fd
WubuVslGpuFence *wubu_vsl_gpu_fence_import_sync_file(WubuVslGpuDevice *device, int fd);

// Display/KMS
uint32_t wubu_vsl_gpu_get_connectors(WubuVslGpuDevice *device, WubuGpuConnector **connectors);
uint32_t wubu_vsl_gpu_get_modes(WubuGpuConnector *connector, WubuGpuMode **modes);
VkResult wubu_vsl_gpu_atomic_commit(WubuVslGpuDevice *device, const WubuGpuAtomicCommit *commit, bool test_only);
WubuVslGpuFence *wubu_vsl_gpu_page_flip(WubuVslGpuDevice *device, WubuGpuCrtc *crtc, WubuVslGpuBuffer *buffer);

// Debug/Introspection
void wubu_vsl_gpu_dump_state(WubuVslGpuDevice *device);
const char *wubu_vsl_gpu_type_name(WubuGpuType type);

#ifdef __cplusplus
}
#endif

#endif // WUBU_VSL_GPU_H