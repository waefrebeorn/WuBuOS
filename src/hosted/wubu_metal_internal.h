/*
 * wubu_metal_internal.h  --  WuBuOS Metal Layer Internal Header
 * Shared types, global state, and declarations for metal submodules.
 * C11 opaque struct pattern: public types in wubu_metal.h, private here.
 */

#ifndef WUBU_METAL_INTERNAL_H
#define WUBU_METAL_INTERNAL_H

#include "wubu_metal.h"
#include "../kernel/wubu_math.h"
#include "../audio/wubu_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ====================================================================
 * GLOBAL STATE (extern - defined in wubu_metal.c facade)
 * ==================================================================== */
extern WubuBootEnv      g_env;
extern WubuDisplay      g_display;
extern WubuInput        g_input;
extern WubuAudio        g_audio;
extern bool             g_initialized;

/* ====================================================================
 * EVDEV INPUT BACKEND
 * ==================================================================== */
int wubu_evdev_find_device(const char *type, int *out_fd);
void wubu_evdev_init_all(void);
void wubu_evdev_shutdown(void);
int wubu_evdev_poll(void);
int wubu_evdev_key_down(uint32_t key);
void wubu_evdev_mouse_pos(int *x, int *y);

/* ====================================================================
 * ALSA AUDIO BACKEND
 * ==================================================================== */
int wubu_alsa_init(int sample_rate, int channels, int buffer_frames);
void wubu_alsa_shutdown(void);
void wubu_alsa_submit(const float *buf, int frames);
double wubu_alsa_cpu_load(void);

/* ====================================================================
 * PULSEAUDIO BACKEND
 * ==================================================================== */
int wubu_pulse_init(int sample_rate, int channels, int buffer_frames);
void wubu_pulse_shutdown(void);
void wubu_pulse_submit(const float *buf, int frames);
double wubu_pulse_cpu_load(void);

/* ====================================================================
 * PIPEWIRE BACKEND
 * ==================================================================== */
int wubu_pipewire_init(int sample_rate, int channels, int buffer_frames);
void wubu_pipewire_shutdown(void);
void wubu_pipewire_submit(const float *buf, int frames);
double wubu_pipewire_cpu_load(void);

/* ====================================================================
 * WSL2 SPECIFIC
 * ==================================================================== */
int wubu_wsl2_disp_init(void);
int wubu_wsl2_audio_init(void);
const char *wubu_wsl2_wayland_path(void);
const char *wubu_wsl2_pulse_path(void);

/* ====================================================================
 * X11 DISPLAY BACKEND
 * ==================================================================== */
int wubu_x11_init(int width, int height);
void wubu_x11_shutdown(void);
void wubu_x11_flip(void);
int wubu_x11_set_mode(int width, int height, int refresh_hz);

/* ====================================================================
 * VBE (LEGACY/BIOS) DISPLAY BACKEND
 * ==================================================================== */
void vbe_init_fb(int width, int height);
void vbe_shutdown_fb(void);

/* ====================================================================
 * UNIFIED DISPLAY/INPUT/AUDIO API
 * ==================================================================== */
int wubu_disp_init(int width, int height);
void wubu_disp_shutdown(void);
WubuDisplay *wubu_disp_state(void);
int wubu_disp_set_mode(int width, int height, int refresh_hz);
void wubu_disp_flip(void);
void wubu_disp_poll_events(void);
WubuDispBackend wubu_disp_current(void);
int wubu_disp_force(WubuDispBackend backend);
int wubu_disp_get_modes(int *widths, int *heights, int max);
void wubu_disp_gaad_nearest(int w, int h, int *out_w, int *out_h);

int wubu_input_init(void);
void wubu_input_shutdown(void);
WubuInput *wubu_input_state(void);
int wubu_input_poll(void);
int wubu_input_key_down(uint32_t key);
void wubu_input_mouse_pos(int *x, int *y);
int wubu_input_gamepads(char names[][64]);

int wubu_audio_init(int sample_rate, int channels, int buffer_frames);
void wubu_audio_shutdown(void);
WubuAudio *wubu_audio_state(void);
void wubu_audio_submit(const float *buf, int frames);
double wubu_audio_cpu_load(void);

/* ====================================================================
 * BARE-METAL BOOT ENTRY POINTS
 * ==================================================================== */
int wubu_metal_init(int width, int height);
void wubu_metal_run(void);
void wubu_metal_shutdown(void);

/* ====================================================================
 * VULKAN SURFACE CREATION
 * ==================================================================== */
#ifdef WUBU_USE_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>

/* Forward declare X11/Wayland types */
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef struct wl_display wl_display_t;
typedef struct wl_surface wl_surface_t;

VkResult wubu_vk_create_xlib_surface(VkInstance instance, Display *dpy, Window window, VkSurfaceKHR *surface);
VkResult wubu_vk_create_wayland_surface(VkInstance instance, wl_display_t *display, wl_surface_t *surface, VkSurfaceKHR *vk_surface);
VkResult wubu_vk_create_display_plane_surface(VkInstance instance, VkDisplayModeKHR display_mode, VkDisplayPlaneAlphaFlagKHR alpha, VkExtent2D *image_extent, VkSurfaceKHR *surface);
VkResult wubu_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface);
#else
typedef int VkResult;
typedef void *VkInstance;
typedef void *VkSurfaceKHR;
typedef void *VkDisplayModeKHR;
typedef int VkDisplayPlaneAlphaFlagKHR;
typedef struct { uint32_t width, height; } VkExtent2D;
typedef struct _XDisplay Display;
typedef unsigned long Window;
#define VK_ERROR_EXTENSION_NOT_PRESENT -86
#define VK_ERROR_INITIALIZATION_FAILED -3

VkResult wubu_vk_create_xlib_surface(VkInstance instance, Display *dpy, Window window, VkSurfaceKHR *surface);
VkResult wubu_vk_create_wayland_surface(VkInstance instance, void *display, void *surface, VkSurfaceKHR *vk_surface);
VkResult wubu_vk_create_display_plane_surface(VkInstance instance, VkDisplayModeKHR display_mode, VkDisplayPlaneAlphaFlagKHR alpha, VkExtent2D *image_extent, VkSurfaceKHR *surface);
VkResult wubu_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface);
#endif

#endif /* WUBU_METAL_INTERNAL_H */