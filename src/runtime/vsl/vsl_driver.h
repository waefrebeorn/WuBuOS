/*
 * vsl_driver.h  --  VSL Driver Management API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_DRIVER_H
#define WUBUOS_VSL_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declare opaque type */
struct VSL_DRV;
typedef struct VSL_DRV VSL_DRV;

/* VSL driver types */
typedef enum {
    VSL_DRV_NONE = 0,
    VSL_DRV_GPU_VULKAN,     /* Vulkan GPU passthrough */
    VSL_DRV_GPU_CUDA,       /* CUDA GPU passthrough */
    VSL_DRV_NET,            /* Network interface */
    VSL_DRV_BLOCK,          /* Block device */
    VSL_DRV_INPUT,          /* Keyboard/mouse */
    VSL_DRV_DISPLAY,        /* Display output */
    VSL_DRV_AUDIO,          /* Audio device */
    VSL_DRV_USB,            /* USB controller */
    VSL_DRV_PCI,            /* PCI bus */
} VSL_DRV_TYPE;

/* Register a VSL driver.
 * Returns driver ID (>=0), or -1 on error. */
int vsl_register_driver(VSL_DRV_TYPE type, uint64_t io_base,
                        uint64_t mem_base, size_t mem_size, uint32_t irq);

/* Activate a VSL driver.
 * Returns 0 on success, -1 on error. */
int vsl_activate_driver(int drv_id);

/* Deactivate a VSL driver.
 * Returns 0 on success, -1 on error. */
int vsl_deactivate_driver(int drv_id);

/* Check if a driver type is active. */
bool vsl_driver_active(VSL_DRV_TYPE type);

/* Get driver by type. Returns NULL if not found. */
VSL_DRV *vsl_get_driver(VSL_DRV_TYPE type);

/* Driver accessors */
VSL_DRV_TYPE vsl_drv_get_type(const VSL_DRV *drv);
bool vsl_drv_is_active(const VSL_DRV *drv);
uint64_t vsl_drv_get_io_base(const VSL_DRV *drv);
uint64_t vsl_drv_get_mem_base(const VSL_DRV *drv);
size_t vsl_drv_get_mem_size(const VSL_DRV *drv);
uint32_t vsl_drv_get_irq(const VSL_DRV *drv);
void *vsl_drv_get_priv(const VSL_DRV *drv);

#endif /* WUBUOS_VSL_DRIVER_H */