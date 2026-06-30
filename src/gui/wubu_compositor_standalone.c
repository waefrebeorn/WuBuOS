/*
 * wubu_compositor_standalone.c  --  WuBuOS Minimal Wayland Compositor (no wlroots)
 *
 * Phase 1: Standalone Compositor Core
 * - Wayland server (libwayland-server)
 * - DRM/KMS + GBM (libdrm + libgbm)
 * - Vulkan renderer (via VSL GPU drivers)
 * - xdg-shell + xdg-decoration + fractional_scale protocols
 * - Damage tracking + triple-buffer + vsync
 * - zwp_linux_dmabuf_v1 + explicit_sync
 * - 9P IPC (no D-Bus)
 *
 * This is our own compositor - no wlroots dependency
 * We implement only what we need for XP+polish metaphor
 */

#define _GNU_SOURCE
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <gbm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libudev.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include <pixman.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "wubu_compositor.h"
#include "wubu_vsl.h"

/* ================================================================
 * WuBuObject - Better than GObject
 * ================================================================ */

typedef void (*WuBuObjectFini)(void *obj);

typedef struct {
    const char *type_name;
    WuBuObjectFini fini;
} WuBuObjectHeader;

#define WUBU_OBJECT_HEADER \
    WuBuObjectHeader _header;

#define WUBU_OBJECT_NEW(type, ...) \
    ({ \
        struct { WuBuObjectHeader _header; type inst; } *obj = calloc(1, sizeof(*obj)); \
        obj->_header.type_name = #type; \
        obj->_header.fini = (WuBuObjectFini)type##_fini; \
        type##_init(&obj->inst, ##__VA_ARGS__); \
        &obj->inst; \
    })

#define WUBU_OBJECT_FINI(obj) \
    do { \
        if (obj) { \
            WuBuObjectHeader *hdr = (WuBuObjectHeader *)((char *)(obj) - offsetof(WuBuObjectHeader, _header)); \
            if (hdr->fini) hdr->fini(obj); \
            free(hdr); \
        } \
    } while (0)

/* ================================================================
 * Wayland Protocol Implementations (minimal subset)
 * ================================================================ */

/* Forward declarations for protocol handlers */
static void wl_compositor_create_surface(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t id);
static void wl_compositor_create_region(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t id);

/* wl_compositor interface */
static const struct wl_compositor_interface wl_compositor_impl = {
    .create_surface = wl_compositor_create_surface,
    .create_region = wl_compositor_create_region,
};

static void wl_compositor_bind(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface,
                                                       version, id);
    wl_resource_set_implementation(resource, &wl_compositor_impl, data, NULL);
}

/* ================================================================
 * Surface / Buffer Management
 * ================================================================ */

typedef struct WuBuBuffer WuBuBuffer;

struct WuBuBuffer {
    WUBU_OBJECT_HEADER

    struct wl_resource *resource;
    struct wl_shm_buffer *shm_buffer;
    struct gbm_bo *gbm_bo;           /* For dmabuf */
    VkImage vk_image;                /* For Vulkan */
    VkDeviceMemory vk_memory;
    VkImageView vk_view;
    VkFormat vk_format;
    uint32_t width, height;
    uint32_t stride;
    uint32_t format;                 /* DRM format */
    bool is_dmabuf;
    int dmabuf_fd;
    struct wl_listener destroy_listener;
};

static void buffer_fini(WuBuBuffer *buf) {
    if (buf->gbm_bo) gbm_bo_destroy(buf->gbm_bo);
    if (buf->vk_image) vkDestroyImage(NULL, buf->vk_image, NULL);
    if (buf->vk_memory) vkFreeMemory(NULL, buf->vk_memory, NULL);
    if (buf->vk_view) vkDestroyImageView(NULL, buf->vk_view, NULL);
    if (buf->dmabuf_fd >= 0) close(buf->dmabuf_fd);
    wl_list_remove(&buf->destroy_listener.link);
}

/* ================================================================
 * Window / Toplevel
 * ================================================================ */

typedef struct WuBuWindow WuBuWindow;

struct WuBuWindow {
    WUBU_OBJECT_HEADER

    struct wl_resource *xdg_toplevel_resource;
    struct wl_resource *surface_resource;
    struct wl_surface *surface;

    WuBuBuffer *current_buffer;
    WuBuBuffer *pending_buffer;

    struct {
        double x, y;
        double width, height;
        double scale;
        float opacity;
        float transform[9];
    } geometry;

    struct {
        bool maximized;
        bool fullscreen;
        bool minimized;
        bool activated;
        bool resizing;
        uint32_t resize_edges;
    } state;

    struct wl_list link;             /* Z-order in compositor */
    struct wl_listener surface_commit;
    struct wl_listener surface_destroy;
    struct wl_listener toplevel_map;
    struct wl_listener toplevel_unmap;
    struct wl_listener toplevel_destroy;
};

static void window_init(WuBuWindow *win, struct wl_resource *xdg_toplevel_resource) {
    win->xdg_toplevel_resource = xdg_toplevel_resource;
    win->current_buffer = NULL;
    win->pending_buffer = NULL;
    win->geometry.x = 100;
    win->geometry.y = 100;
    win->geometry.width = 800;
    win->geometry.height = 600;
    win->geometry.scale = 1.0;
    win->geometry.opacity = 1.0;
    memset(win->geometry.transform, 0, sizeof(win->geometry.transform));
    win->geometry.transform[0] = win->geometry.transform[4] = win->geometry.transform[8] = 1.0;
    memset(&win->state, 0, sizeof(win->state));

    wl_signal_add(&win->surface->events.commit, &win->surface_commit);
    win->surface_commit.notify = window_handle_commit;

    wl_signal_add(&win->surface->events.destroy, &win->surface_destroy);
    win->surface_destroy.notify = window_handle_destroy;
}

static void window_fini(WuBuWindow *win) {
    wl_list_remove(&win->surface_commit.link);
    wl_list_remove(&win->surface_destroy.link);
    wl_list_remove(&win->link);
    if (win->current_buffer) WUBU_OBJECT_FINI(win->current_buffer);
    if (win->pending_buffer) WUBU_OBJECT_FINI(win->pending_buffer);
}

static void window_handle_commit(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, surface_commit);

    /* Get pending buffer from surface */
    struct wl_buffer *buffer = wl_surface_get_buffer(win->surface);
    if (buffer) {
        /* Convert to WuBuBuffer */
        WuBuBuffer *buf = WUBU_OBJECT_NEW(WuBuBuffer, buf);
        buf->resource = buffer->resource;
        buf->width = win->geometry.width;
        buf->height = win->geometry.height;
        buf->format = DRM_FORMAT_ARGB8888;
        buf->is_dmabuf = false;

        win->pending_buffer = buf;

        /* Damage whole window */
        WuBuCompositor *comp = wubu_window_get_compositor(win);
        if (comp) {
            pixman_region32_t damage;
            pixman_region32_init_rect(&damage, win->geometry.x, win->geometry.y,
                                       win->geometry.width, win->geometry.height);
            pixman_region32_union(&comp->damage, &comp->damage, &damage);
            pixman_region32_fini(&damage);
        }
    }
}

static void window_handle_destroy(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, surface_destroy);
    WUBU_OBJECT_FINI(win);
}

/* ================================================================
 * Output / Monitor
 * ================================================================ */

typedef struct WuBuOutput WuBuOutput;

struct WuBuOutput {
    WUBU_OBJECT_HEADER

    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeConnector *connector;
    drmModeCrtc *crtc;
    drmModeModeInfo mode;

    struct gbm_device *gbm_device;
    struct gbm_surface *gbm_surface;

    VkSurfaceKHR vk_surface;
    VkSwapchainKHR vk_swapchain;
    uint32_t swapchain_image_count;
    VkImage *vk_swapchain_images;
    VkImageView *vk_swapchain_image_views;
    VkFramebuffer *vk_framebuffers;
    VkRenderPass vk_render_pass;
    VkPipelineLayout vk_pipeline_layout;
    VkPipeline vk_pipeline;
    VkCommandPool vk_cmd_pool;
    VkCommandBuffer *vk_cmd_buffers;
    VkSemaphore *vk_image_available;
    VkSemaphore *vk_render_finished;
    VkFence *vk_in_flight_fences;
    uint32_t current_frame;

    struct wl_list link;
    struct wl_listener destroy;
};

static void output_init(WuBuOutput *out, int drm_fd, uint32_t connector_id, uint32_t crtc_id) {
    out->drm_fd = drm_fd;
    out->connector_id = connector_id;
    out->crtc_id = crtc_id;

    out->connector = drmModeGetConnector(drm_fd, connector_id);
    out->crtc = drmModeGetCrtc(drm_fd, crtc_id);
    if (out->connector && out->connector->count_modes > 0) {
        out->mode = out->connector->modes[0];
    }

    /* Create GBM device */
    out->gbm_device = gbm_create_device(drm_fd);

    /* Create GBM surface */
    out->gbm_surface = gbm_surface_create(out->gbm_device,
                                           out->mode.hdisplay, out->mode.vdisplay,
                                           GBM_FORMAT_ARGB8888,
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    /* Initialize Vulkan swapchain */
    output_init_vulkan(out);
}

static void output_init_vulkan(WuBuOutput *out) {
    WuBuCompositor *comp = wubu_output_get_compositor(out);
    VkDevice device = comp->vk_device;

    /* Create VkSurfaceKHR for Wayland */
    VkWaylandSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = wl_display_get_wl_display(comp->display),
        .surface = NULL, /* Will be set per-window */
    };
    vkCreateWaylandSurfaceKHR(comp->vk_instance, &surface_info, NULL, &out->vk_surface);

    /* Create swapchain */
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = out->vk_surface,
        .minImageCount = 3,  /* Triple buffering */
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { out->mode.hdisplay, out->mode.vdisplay },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,  /* VSync */
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    vkCreateSwapchainKHR(device, &swapchain_info, NULL, &out->vk_swapchain);

    /* Get swapchain images */
    vkGetSwapchainImagesKHR(device, out->vk_swapchain, &out->swapchain_image_count, NULL);
    out->vk_swapchain_images = calloc(out->swapchain_image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(device, out->vk_swapchain, &out->swapchain_image_count, out->vk_swapchain_images);

    /* Create image views */
    out->vk_swapchain_image_views = calloc(out->swapchain_image_count, sizeof(VkImageView));
    for (uint32_t i = 0; i < out->swapchain_image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out->vk_swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 },
        };
        vkCreateImageView(device, &view_info, NULL, &out->vk_swapchain_image_views[i]);
    }

    /* Create render pass */
    VkAttachmentDescription color_attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref };
    VkSubpassDependency dep = { .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0, .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
    VkRenderPassCreateInfo rp_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &color_attachment, .subpassCount = 1, .pSubpasses = &subpass, .dependencyCount = 1, .pDependencies = &dep };
    vkCreateRenderPass(device, &rp_info, NULL, &out->vk_render_pass);

    /* Create framebuffers */
    out->vk_framebuffers = calloc(out->swapchain_image_count, sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < out->swapchain_image_count; i++) {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = out->vk_render_pass,
            .attachmentCount = 1,
            .pAttachments = &out->vk_swapchain_image_views[i],
            .width = out->mode.hdisplay,
            .height = out->mode.vdisplay,
            .layers = 1,
        };
        vkCreateFramebuffer(device, &fb_info, NULL, &out->vk_framebuffers[i]);
    }

    /* Command pool & buffers */
    VkCommandPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = 0, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    vkCreateCommandPool(device, &pool_info, NULL, &out->vk_cmd_pool);

    out->vk_cmd_buffers = calloc(out->swapchain_image_count, sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = out->vk_cmd_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = out->swapchain_image_count };
    vkAllocateCommandBuffers(device, &alloc_info, out->vk_cmd_buffers);

    /* Sync objects */
    out->vk_image_available = calloc(out->swapchain_image_count, sizeof(VkSemaphore));
    out->vk_render_finished = calloc(out->swapchain_image_count, sizeof(VkSemaphore));
    out->vk_in_flight_fences = calloc(out->swapchain_image_count, sizeof(VkFence));
    for (uint32_t i = 0; i < out->swapchain_image_count; i++) {
        VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(device, &sem_info, NULL, &out->vk_image_available[i]);
        vkCreateSemaphore(device, &sem_info, NULL, &out->vk_render_finished[i]);
        VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateFence(device, &fence_info, NULL, &out->vk_in_flight_fences[i]);
    }

    out->current_frame = 0;
}

static void output_fini(WuBuOutput *out) {
    WuBuCompositor *comp = wubu_output_get_compositor(out);
    VkDevice device = comp->vk_device;
    vkDeviceWaitIdle(device);

    for (uint32_t i = 0; i < out->swapchain_image_count; i++) {
        vkDestroySemaphore(device, out->vk_image_available[i], NULL);
        vkDestroySemaphore(device, out->vk_render_finished[i], NULL);
        vkDestroyFence(device, out->vk_in_flight_fences[i], NULL);
    }
    vkFreeCommandBuffers(device, out->vk_cmd_pool, out->swapchain_image_count, out->vk_cmd_buffers);
    vkDestroyCommandPool(device, out->vk_cmd_pool, NULL);
    for (uint32_t i = 0; i < out->swapchain_image_count; i++) {
        vkDestroyFramebuffer(device, out->vk_framebuffers[i], NULL);
        vkDestroyImageView(device, out->vk_swapchain_image_views[i], NULL);
    }
    vkDestroyRenderPass(device, out->vk_render_pass, NULL);
    vkDestroySwapchainKHR(device, out->vk_swapchain, NULL);
    vkDestroySurfaceKHR(comp->vk_instance, out->vk_surface, NULL);
    gbm_surface_destroy(out->gbm_surface);
    gbm_device_destroy(out->gbm_device);
    if (out->connector) drmModeFreeConnector(out->connector);
    if (out->crtc) drmModeFreeCrtc(out->crtc);
    wl_list_remove(&out->link);
    free(out->vk_swapchain_images);
    free(out->vk_swapchain_image_views);
    free(out->vk_framebuffers);
    free(out->vk_cmd_buffers);
    free(out->vk_image_available);
    free(out->vk_render_finished);
    free(out->vk_in_flight_fences);
}

static void output_render(WuBuOutput *out) {
    WuBuCompositor *comp = wubu_output_get_compositor(out);
    VkDevice device = comp->vk_device;

    /* Wait for fence */
    vkWaitForFences(device, 1, &out->vk_in_flight_fences[out->current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &out->vk_in_flight_fences[out->current_frame]);

    /* Acquire next image */
    uint32_t image_index;
    vkAcquireNextImageKHR(device, out->vk_swapchain, UINT64_MAX,
                           out->vk_image_available[out->current_frame], VK_NULL_HANDLE, &image_index);

    /* Record command buffer */
    VkCommandBuffer cmd = out->vk_cmd_buffers[out->current_frame];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &begin_info);

    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = out->vk_render_pass,
        .framebuffer = out->vk_framebuffers[image_index],
        .renderArea = { {0, 0}, {out->mode.hdisplay, out->mode.vdisplay} },
        .clearValueCount = 1,
        .pClearValues = &(VkClearValue){ .color = { {0.1f, 0.1f, 0.1f, 1.0f} } },
    };
    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    /* Bind pipeline & render windows */
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, out->vk_pipeline);

    WuBuWindow *win;
    wl_list_for_each(win, &comp->windows, link) {
        if (!win->current_buffer) continue;
        /* TODO: Bind window texture, push constants for transform, draw quad */
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    /* Submit */
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &out->vk_image_available[out->current_frame],
        .pWaitDstStageMask = (VkPipelineStageFlags[]){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &out->vk_render_finished[out->current_frame],
    };
    vkQueueSubmit(comp->vk_queue, 1, &submit, out->vk_in_flight_fences[out->current_frame]);

    /* Present */
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &out->vk_render_finished[out->current_frame],
        .swapchainCount = 1,
        .pSwapchains = &out->vk_swapchain,
        .pImageIndices = &image_index,
    };
    vkQueuePresentKHR(comp->vk_queue, &present);

    out->current_frame = (out->current_frame + 1) % out->swapchain_image_count;
}

/* ================================================================
 * Compositor
 * ================================================================ */

struct WuBuCompositor {
    WUBU_OBJECT_HEADER

    struct wl_display *display;
    int drm_fd;
    struct udev *udev;
    struct gbm_device *gbm_device;

    VkInstance vk_instance;
    VkPhysicalDevice vk_physical_device;
    VkDevice vk_device;
    VkQueue vk_queue;
    uint32_t vk_queue_family;

    struct wl_list outputs;           /* WuBuOutput */
    struct wl_list windows;           /* WuBuWindow - Z-order */

    pixman_region32_t damage;

    uint64_t last_frame_ns;
    uint64_t frame_interval_ns;       /* 16666667 for 60Hz */

    /* Wayland globals */
    struct wl_global *wl_compositor_global;
    struct wl_global *wl_shm_global;
    struct wl_global *xdg_wm_base_global;
    struct wl_global *xdg_decoration_global;
    struct wl_global *fractional_scale_global;
    struct wl_global *linux_dmabuf_global;

    /* 9P server */
    int p9_fd;
};

static void compositor_init(WuBuCompositor *comp) {
    wl_list_init(&comp->outputs);
    wl_list_init(&comp->windows);
    pixman_region32_init(&comp->damage);
    comp->frame_interval_ns = 16666667;
    comp->last_frame_ns = 0;

    /* Wayland display */
    comp->display = wl_display_create();

    /* Open DRM device */
    comp->drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (comp->drm_fd < 0) {
        fprintf(stderr, "Failed to open DRM device: %s\n", strerror(errno));
        abort();
    }

    /* GBM device */
    comp->gbm_device = gbm_create_device(comp->drm_fd);

    /* Udev for hotplug */
    comp->udev = udev_new();

    /* Initialize Vulkan */
    compositor_init_vulkan(comp);

    /* Register Wayland globals */
    wl_global_create(comp->display, &wl_compositor_interface, 5, comp, wl_compositor_bind);
    /* TODO: wl_shm, xdg_wm_base, xdg_decoration, fractional_scale, linux_dmabuf */

    /* Scan for outputs */
    compositor_scan_outputs(comp);
}

static void compositor_init_vulkan(WuBuCompositor *comp) {
    /* Get VSL GPU driver */
    VSL_DRV *drv = vsl_get_driver(VSL_DRV_GPU_VULKAN);
    if (drv && drv->active) {
        comp->vk_instance = drv->priv;
    } else {
        /* Create our own instance */
        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "WuBuOS Compositor",
            .apiVersion = VK_API_VERSION_1_3,
        };
        const char *extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        };
        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .enabledExtensionCount = 3,
            .ppEnabledExtensionNames = extensions,
        };
        vkCreateInstance(&create_info, NULL, &comp->vk_instance);
    }

    /* Select physical device */
    uint32_t count;
    vkEnumeratePhysicalDevices(comp->vk_instance, &count, NULL);
    VkPhysicalDevice *devices = calloc(count, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(comp->vk_instance, &count, devices);
    comp->vk_physical_device = devices[0];
    free(devices);

    /* Find queue family */
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(comp->vk_physical_device, &qcount, NULL);
    VkQueueFamilyProperties *qprops = calloc(qcount, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(comp->vk_physical_device, &qcount, qprops);
    for (uint32_t i = 0; i < qcount; i++) {
        if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 present_support;
            vkGetPhysicalDeviceWaylandPresentationSupportKHR(comp->vk_physical_device, i, NULL);
            comp->vk_queue_family = i;
            break;
        }
    }
    free(qprops);

    /* Create device */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = comp->vk_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };
    vkCreateDevice(comp->vk_physical_device, &device_info, NULL, &comp->vk_device);
    vkGetDeviceQueue(comp->vk_device, comp->vk_queue_family, 0, &comp->vk_queue);
}

static void compositor_scan_outputs(WuBuCompositor *comp) {
    drmModeRes *res = drmModeGetResources(comp->drm_fd);
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(comp->drm_fd, res->connectors[i]);
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            for (int j = 0; j < conn->count_encoders; j++) {
                drmModeEncoder *enc = drmModeGetEncoder(comp->drm_fd, conn->encoders[j]);
                if (enc) {
                    WuBuOutput *out = WUBU_OBJECT_NEW(WuBuOutput, out, comp->drm_fd, conn->connector_id, enc->crtc_id);
                    wl_list_insert(&comp->outputs, &out->link);
                    drmModeFreeEncoder(enc);
                    break;
                }
            }
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);
}

static void compositor_fini(WuBuCompositor *comp) {
    pixman_region32_fini(&comp->damage);

    WuBuOutput *out, *tmp;
    wl_list_for_each_safe(out, tmp, &comp->outputs, link) {
        WUBU_OBJECT_FINI(out);
    }

    WuBuWindow *win, *wtmp;
    wl_list_for_each_safe(win, wtmp, &comp->windows, link) {
        WUBU_OBJECT_FINI(win);
    }

    vkDestroyDevice(comp->vk_device, NULL);
    vkDestroyInstance(comp->vk_instance, NULL);
    gbm_device_destroy(comp->gbm_device);
    close(comp->drm_fd);
    udev_unref(comp->udev);
    wl_display_destroy(comp->display);
}

/* ================================================================
 * Main Loop
 * ================================================================ */

static void compositor_run_frame(WuBuCompositor *comp) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_ns = now.tv_sec * 1000000000ULL + now.tv_nsec;

    if (comp->last_frame_ns > 0) {
        uint64_t elapsed = now_ns - comp->last_frame_ns;
        if (elapsed < comp->frame_interval_ns) {
            /* Sleep until next frame */
            uint64_t sleep_ns = comp->frame_interval_ns - elapsed;
            struct timespec ts = { .tv_sec = 0, .tv_nsec = sleep_ns };
            nanosleep(&ts, NULL);
            return;
        }
    }
    comp->last_frame_ns = now_ns;

    /* Process Wayland events */
    wl_display_flush_clients(comp->display);

    /* Render each output */
    WuBuOutput *out;
    wl_list_for_each(out, &comp->outputs, link) {
        output_render(out);
    }

    /* Clear damage */
    pixman_region32_clear(&comp->damage);
}

/* ================================================================
 * Public API
 * ================================================================ */

WuBuCompositor *wubu_compositor_create(void) {
    WuBuCompositor *comp = WUBU_OBJECT_NEW(WuBuCompositor, comp);
    compositor_init(comp);
    return comp;
}

void wubu_compositor_destroy(WuBuCompositor *comp) {
    WUBU_OBJECT_FINI(comp);
}

int wubu_compositor_run(WuBuCompositor *comp) {
    const char *socket = wl_display_add_socket_auto(comp->display);
    if (!socket) return -1;
    setenv("WAYLAND_DISPLAY", socket, true);
    fprintf(stderr, "WuBuOS Compositor running on wayland-%s\n", socket);

    while (true) {
        compositor_run_frame(comp);
    }
    return 0;
}

struct wl_display *wubu_compositor_get_display(WuBuCompositor *comp) {
    return comp->display;
}

WuBuOutput *wubu_output_get_compositor(WuBuOutput *out) {
    return wl_container_of(out, comp, outputs);
}

WuBuWindow *wubu_window_get_compositor(WuBuWindow *win) {
    return wl_container_of(win, comp, windows);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char *argv[]) {
    /* Initialize VSL */
    if (vsl_init() != 0) {
        fprintf(stderr, "VSL init failed\n");
        return 1;
    }
    int vk_drv = vsl_register_driver(VSL_DRV_GPU_VULKAN, 0, 0, 0, 0);
    if (vk_drv >= 0) vsl_activate_driver(vk_drv);

    WuBuCompositor *comp = wubu_compositor_create();
    int ret = wubu_compositor_run(comp);
    wubu_compositor_destroy(comp);

    vsl_shutdown();
    return ret;
}