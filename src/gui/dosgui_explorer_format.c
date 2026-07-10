/* dosgui_explorer_format.c -- Shared type/format helpers for the explorer.
 *
 * These are PUBLIC API (declared in dosgui_explorer.h) and defined exactly
 * ONCE here, so every submodule (fs, fsops, zip, render, tree, drives) links
 * the same implementation — no duplicated copies, no double-coding.
 * Minimal includes.
 */

#include "dosgui_explorer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

const char *dosgui_explorer_type_str(ExEntryType type) {
    static const char *names[] = {
        "Unknown", "File", "Folder", "Link", "Drive", "Archive", "Mount", "Special"
    };
    if (type >= 0 && type < 8) return names[type];
    return "Unknown";
}

const char *dosgui_explorer_view_mode_name(ExViewMode mode) {
    static const char *names[] = {"Details", "Icons", "List", "Tiles"};
    if (mode >= 0 && mode < 4) return names[mode];
    return "Unknown";
}

uint32_t dosgui_explorer_type_color(ExEntryType type) {
    static const uint32_t colors[] = {
        0x808080,  /* Unknown - gray */
        0xFFFFFF,  /* File - white */
        0xFFD700,  /* Folder - gold */
        0x00FFFF,  /* Link - cyan */
        0x00FF00,  /* Drive - green */
        0xFF8000,  /* Zip - orange */
        0x8000FF,  /* Mount - purple */
        0xFF00FF   /* Special - magenta */
    };
    if (type >= 0 && type < 8) return colors[type];
    return 0x808080;
}

void dosgui_explorer_format_size(uint64_t bytes, char *buf, int buf_size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = (double)bytes;
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    if (unit == 0) {
        snprintf(buf, buf_size, "%.0f %s", size, units[unit]);
    } else {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit]);
    }
}

void dosgui_explorer_format_time(time_t t, char *buf, int buf_size) {
    struct tm *tm = localtime(&t);
    if (tm) {
        strftime(buf, buf_size, "%Y-%m-%d %H:%M", tm);
    } else {
        snprintf(buf, buf_size, "Unknown");
    }
}
