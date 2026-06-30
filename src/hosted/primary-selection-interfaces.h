/*
 * primary-selection-interfaces.h -- Primary Selection Protocol Interface Definitions
 * Extracted from primary-selection-private.code for testing
 */

#ifndef PRIMARY_SELECTION_INTERFACES_H
#define PRIMARY_SELECTION_INTERFACES_H

#include <wayland-client.h>

extern const struct wl_interface zwp_primary_selection_device_manager_v1_interface;
extern const struct wl_interface zwp_primary_selection_device_v1_interface;
extern const struct wl_interface zwp_primary_selection_offer_v1_interface;
extern const struct wl_interface zwp_primary_selection_source_v1_interface;

#endif