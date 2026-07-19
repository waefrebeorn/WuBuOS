/*
 * VSL GPU Test - Validates DRM/KMS backend on WSL
 */

#include "wubu_vsl_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("=== VSL GPU Test ===\n\n");
    
    // Open GPU device (auto-detects renderD128, card0, virtgpu)
    WubuVslGpuDevice *device = wubu_vsl_gpu_open(NULL);
    if (!device) {
        fprintf(stderr, "Failed to open GPU device\n");
        return 1;
    }
    
    // Get device info
    WubuGpuDeviceInfo info;
    wubu_vsl_gpu_get_info(device, &info);
    
    printf("GPU Type: %s\n", wubu_vsl_gpu_type_name(info.type));
    printf("Name: %s\n", info.name);
    printf("Driver: %s\n", info.driver_version);
    printf("VRAM: %lu MB\n", info.vram_size / (1024*1024));
    printf("GTT: %lu MB\n", info.gtt_size / (1024*1024));
    printf("DMABUF: %s\n", info.supports_dmabuf ? "Yes" : "No");
    printf("Syncobj: %s\n", info.supports_explicit_sync ? "Yes" : "No");
    printf("VM Bind: %s\n", info.supports_vm_bind ? "Yes" : "No");
    
    // Test buffer creation
    printf("\n--- Buffer Test ---\n");
    WubuGpuBufferCreateInfo buf_info = {0};
    buf_info.size = 4096;
    buf_info.alignment = 4096;
    buf_info.flags = WUBU_GPU_BUFFER_FLAG_SHARED | WUBU_GPU_BUFFER_FLAG_CPU_VISIBLE;
    buf_info.preferred_heap = WUBU_GPU_MEMORY_HEAP_GTT;
    
    WubuVslGpuBuffer *buffer = wubu_vsl_gpu_buffer_create(device, &buf_info);
    if (!buffer) {
        fprintf(stderr, "Failed to create buffer\n");
        wubu_vsl_gpu_close(device);
        return 1;
    }
    printf("Buffer created: handle=%u, size=%lu\n", buffer->handle, buffer->size);
    
    // Test dmabuf export
    if (info.supports_dmabuf) {
        int dmabuf_fd = wubu_vsl_gpu_buffer_export_dmabuf(buffer);
        if (dmabuf_fd >= 0) {
            printf("DMABUF exported: fd=%d\n", dmabuf_fd);
            close(dmabuf_fd);
        } else {
            printf("DMABUF export failed\n");
        }
    }
    
    // Test CPU mapping
    void *ptr;
    if (wubu_vsl_gpu_buffer_map(buffer, &ptr) == VK_SUCCESS) {
        printf("Buffer mapped at %p\n", ptr);
        // Write test pattern
        uint32_t *data = ptr;
        for (int i = 0; i < 1024; i++) {
            data[i] = 0xDEADBEEF + i;
        }
        wubu_vsl_gpu_buffer_unmap(buffer);
        printf("Buffer unmapped\n");
    }
    
    // Test fence (syncobj)
    if (info.supports_explicit_sync) {
        printf("\n--- Fence Test ---\n");
        WubuGpuSyncCreateInfo fence_info = {0};
        fence_info.type = WUBU_GPU_SYNC_TIMELINE;
        fence_info.initial_value = 0;
        fence_info.exportable = true;
        
        WubuVslGpuFence *fence = wubu_vsl_gpu_fence_create(device, &fence_info);
        if (fence) {
            printf("Fence created\n");
            
            // Signal
            wubu_vsl_gpu_fence_signal(fence, 1);
            printf("Fence signaled to 1\n");
            
            // Wait
            VkResult res = wubu_vsl_gpu_fence_wait(fence, 1000000000); // 1 second
            printf("Fence wait result: %d\n", res);
            
            // Export as sync_file
            int sync_fd = wubu_vsl_gpu_fence_export_sync_file(fence);
            if (sync_fd >= 0) {
                printf("Fence exported as sync_file: fd=%d\n", sync_fd);
                close(sync_fd);
            }
            
            wubu_vsl_gpu_fence_destroy(fence);
        }
    }
    
    // Test context
    printf("\n--- Context Test ---\n");
    WubuVslGpuContext *ctx = wubu_vsl_gpu_context_create(device, (1<<WUBU_GPU_ENGINE_RENDER) | (1<<WUBU_GPU_ENGINE_COMPUTE));
    if (ctx) {
        printf("Context created\n");
        wubu_vsl_gpu_context_destroy(ctx);
    }
    
    // Cleanup
    wubu_vsl_gpu_buffer_destroy(buffer);
    wubu_vsl_gpu_close(device);
    
    // Note: device already closed, so dump_state won't show info
    printf("\n=== Test Complete ===\n");
    return 0;
}