/*
 * vsl_driver.c  --  VSL Driver Management Implementation
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "vsl/vsl_internal.h"
#include "vsl/vsl_driver.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
typedef VkResult (*vkCreateInstance_t)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
typedef void (*vkDestroyInstance_t)(VkInstance, const VkAllocationCallbacks*);
typedef VkResult (*vkEnumeratePhysicalDevices_t)(VkInstance, uint32_t*, VkPhysicalDevice*);
static void *g_vulkan_lib = NULL;
static vkCreateInstance_t g_vkCreateInstance = NULL;
static vkDestroyInstance_t g_vkDestroyInstance = NULL;
static vkEnumeratePhysicalDevices_t g_vkEnumeratePhysicalDevices = NULL;
static VkInstance g_vulkan_instance = VK_NULL_HANDLE;
#endif

#ifdef HAVE_CUDA
#include <cuda.h>
typedef CUresult (*cuInit_t)(unsigned int);
typedef CUresult (*cuDeviceGet_t)(CUdevice*, int);
typedef CUresult (*cuCtxCreate_v2_t)(CUcontext*, unsigned int, CUdevice);
typedef CUresult (*cuCtxDestroy_v2_t)(CUcontext);
static void *g_cuda_lib = NULL;
static cuInit_t g_cuInit = NULL;
static cuDeviceGet_t g_cuDeviceGet = NULL;
static cuCtxCreate_v2_t g_cuCtxCreate = NULL;
static cuCtxDestroy_v2_t g_cuCtxDestroy = NULL;
static CUcontext g_cuda_context = NULL;
#endif

/* Global state */
extern VSL_STATE g_vsl;

/* -- Driver Management -------------------------------------------- */

int vsl_register_driver(VSL_DRV_TYPE type, uint64_t io_base,
                        uint64_t mem_base, size_t mem_size, uint32_t irq) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_drivers >= 16) return -1;

    int id = (int)g_vsl.n_drivers;
    VSL_DRV *drv = &g_vsl.drivers[id];
    drv->type = type;
    drv->active = false;
    drv->io_base = io_base;
    drv->mem_base = mem_base;
    drv->mem_size = mem_size;
    drv->irq = irq;
    drv->priv = NULL;

    g_vsl.n_drivers++;
    return id;
}

int vsl_activate_driver(int drv_id) {
    if (drv_id < 0 || drv_id >= (int)g_vsl.n_drivers) return -1;
    VSL_DRV *drv = &g_vsl.drivers[drv_id];
    if (drv->active) return 0;

    int rc = 0;
    switch (drv->type) {
#ifdef HAVE_VULKAN
        case VSL_DRV_GPU_VULKAN:
            if (!g_vulkan_lib) {
                g_vulkan_lib = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
                if (!g_vulkan_lib) {
                    fprintf(stderr, "[vsl] Failed to load libvulkan.so.1: %s\n", dlerror());
                    rc = -1;
                    break;
                }
                g_vkCreateInstance = (vkCreateInstance_t)dlsym(g_vulkan_lib, "vkCreateInstance");
                g_vkDestroyInstance = (vkDestroyInstance_t)dlsym(g_vulkan_lib, "vkDestroyInstance");
                g_vkEnumeratePhysicalDevices = (vkEnumeratePhysicalDevices_t)dlsym(g_vulkan_lib, "vkEnumeratePhysicalDevices");
                if (!g_vkCreateInstance || !g_vkDestroyInstance || !g_vkEnumeratePhysicalDevices) {
                    fprintf(stderr, "[vsl] Failed to resolve Vulkan symbols\n");
                    rc = -1;
                    break;
                }
            }
            if (g_vulkan_instance == VK_NULL_HANDLE) {
                VkApplicationInfo app_info = {0};
                app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                app_info.pApplicationName = "WuBuOS VSL";
                app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
                app_info.pEngineName = "WuBuOS";
                app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
                app_info.apiVersion = VK_API_VERSION_1_3;

                VkInstanceCreateInfo create_info = {0};
                create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                create_info.pApplicationInfo = &app_info;
                create_info.enabledLayerCount = 0;
                create_info.enabledExtensionCount = 0;

                VkResult vkr = g_vkCreateInstance(&create_info, NULL, &g_vulkan_instance);
                if (vkr != VK_SUCCESS) {
                    fprintf(stderr, "[vsl] vkCreateInstance failed: %d\n", vkr);
                    rc = -1;
                    break;
                }
                fprintf(stderr, "[vsl] Vulkan instance created successfully\n");
            }
            drv->priv = g_vulkan_instance;
            break;
#endif
#ifdef HAVE_CUDA
        case VSL_DRV_GPU_CUDA:
            if (!g_cuda_lib) {
                g_cuda_lib = dlopen("libcuda.so.1", RTLD_LAZY | RTLD_LOCAL);
                if (!g_cuda_lib) {
                    fprintf(stderr, "[vsl] Failed to load libcuda.so.1: %s\n", dlerror());
                    rc = -1;
                    break;
                }
                g_cuInit = (cuInit_t)dlsym(g_cuda_lib, "cuInit");
                g_cuDeviceGet = (cuDeviceGet_t)dlsym(g_cuda_lib, "cuDeviceGet");
                g_cuCtxCreate = (cuCtxCreate_v2_t)dlsym(g_cuda_lib, "cuCtxCreate_v2");
                g_cuCtxDestroy = (cuCtxDestroy_v2_t)dlsym(g_cuda_lib, "cuCtxDestroy_v2");
                if (!g_cuInit || !g_cuDeviceGet || !g_cuCtxCreate || !g_cuCtxDestroy) {
                    fprintf(stderr, "[vsl] Failed to resolve CUDA symbols\n");
                    rc = -1;
                    break;
                }
            }
            if (g_cuda_context == NULL) {
                CUresult cur = g_cuInit(0);
                if (cur != CUDA_SUCCESS) {
                    fprintf(stderr, "[vsl] cuInit failed: %d\n", cur);
                    rc = -1;
                    break;
                }
                CUdevice device;
                cur = g_cuDeviceGet(&device, 0);
                if (cur != CUDA_SUCCESS) {
                    fprintf(stderr, "[vsl] cuDeviceGet failed: %d\n", cur);
                    rc = -1;
                    break;
                }
                cur = g_cuCtxCreate(&g_cuda_context, CU_CTX_MAP_HOST | CU_CTX_SCHED_AUTO, device);
                if (cur != CUDA_SUCCESS) {
                    fprintf(stderr, "[vsl] cuCtxCreate failed: %d\n", cur);
                    rc = -1;
                    break;
                }
                fprintf(stderr, "[vsl] CUDA context created successfully\n");
            }
            drv->priv = g_cuda_context;
            break;
#endif
        case VSL_DRV_NET:
            /* NET driver: create TUN/TAP interface for VSL networking */
            {
                int tun_fd = open("/dev/net/tun", O_RDWR);
                if (tun_fd < 0) {
                    fprintf(stderr, "[vsl] NET driver: failed to open /dev/net/tun: %s\n", strerror(errno));
                    rc = -1;
                    break;
                }
                struct ifreq ifr = {0};
                ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
                snprintf(ifr.ifr_name, IFNAMSIZ, "vsl%d", drv_id);
                if (ioctl(tun_fd, TUNSETIFF, &ifr) < 0) {
                    fprintf(stderr, "[vsl] NET driver: TUNSETIFF failed: %s\n", strerror(errno));
                    close(tun_fd);
                    rc = -1;
                    break;
                }
                fprintf(stderr, "[vsl] NET driver: created interface %s\n", ifr.ifr_name);
                drv->priv = (void *)(intptr_t)tun_fd;
            }
            break;
        default:
            break;
    }

    if (rc == 0) {
        drv->active = true;
    }
    return rc;
}

int vsl_deactivate_driver(int drv_id) {
    if (drv_id < 0 || drv_id >= (int)g_vsl.n_drivers) return -1;
    VSL_DRV *drv = &g_vsl.drivers[drv_id];
    if (!drv->active) return 0;

    switch (drv->type) {
#ifdef HAVE_VULKAN
        case VSL_DRV_GPU_VULKAN:
            if (g_vulkan_instance != VK_NULL_HANDLE && g_vkDestroyInstance) {
                g_vkDestroyInstance(g_vulkan_instance, NULL);
                g_vulkan_instance = VK_NULL_HANDLE;
            }
            break;
#endif
#ifdef HAVE_CUDA
        case VSL_DRV_GPU_CUDA:
            if (g_cuda_context != NULL && g_cuCtxDestroy) {
                g_cuCtxDestroy(g_cuda_context);
                g_cuda_context = NULL;
            }
            break;
#endif
        case VSL_DRV_NET:
            if (drv->priv) {
                close((int)(intptr_t)drv->priv);
                drv->priv = NULL;
            }
            break;
        default:
            break;
    }

    drv->active = false;
    drv->priv = NULL;
    return 0;
}

bool vsl_driver_active(VSL_DRV_TYPE type) {
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        if (g_vsl.drivers[i].type == type && g_vsl.drivers[i].active)
            return true;
    }
    return false;
}

VSL_DRV *vsl_get_driver(VSL_DRV_TYPE type) {
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        if (g_vsl.drivers[i].type == type) return &g_vsl.drivers[i];
    }
    return NULL;
}

/* -- Driver Accessors --------------------------------------------- */

VSL_DRV_TYPE vsl_drv_get_type(const VSL_DRV *drv) {
    return drv ? drv->type : VSL_DRV_NONE;
}

bool vsl_drv_is_active(const VSL_DRV *drv) {
    return drv ? drv->active : false;
}

uint64_t vsl_drv_get_io_base(const VSL_DRV *drv) {
    return drv ? drv->io_base : 0;
}

uint64_t vsl_drv_get_mem_base(const VSL_DRV *drv) {
    return drv ? drv->mem_base : 0;
}

size_t vsl_drv_get_mem_size(const VSL_DRV *drv) {
    return drv ? drv->mem_size : 0;
}

uint32_t vsl_drv_get_irq(const VSL_DRV *drv) {
    return drv ? drv->irq : 0;
}

void *vsl_drv_get_priv(const VSL_DRV *drv) {
    return drv ? drv->priv : NULL;
}