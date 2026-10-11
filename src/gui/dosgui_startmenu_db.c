/*
 * dosgui_startmenu_db.c  --  WuBuOS Start Menu model constructor
 *
 * Extracted from dosgui_startmenu.c (Cell 402).  Owns construction of the
 * static main-menu item list and the category submenus (Programs ->
 * Accessories / WuBuOS / System, plus MIME-derived groups) from the MIME
 * .desktop registry and the built-in app catalog.  The parent module keeps
 * the render + input facade; this module owns state population only.
 *
 * Self-contained: includes only the public + internal startmenu headers,
 * wubu_theme, wubu_mime, and libc.  No GUI render/input code lives here.
 */

#include "dosgui_startmenu_internal.h"
#include "dosgui_startmenu.h"
#include "dosgui_desktop.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_mime.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* -- Main menu construction -------------------------------------- */

void dosgui_startmenu_build_main_menu(void) {
    g_main_count = 0;

    /* Each row carries an XP-style 16x16 icon glyph color. */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Programs", .type = 1, .submenu_id = 0, .icon_color = 0xE8C84A};

    /* Documents -> File Manager */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Documents", .type = 0, .submenu_id = -1,
        .app_name = "File Manager", .icon_color = 0xE8C84A};

    /* Settings */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Settings", .type = 0, .submenu_id = -1,
        .app_name = "Settings", .icon_color = 0x4A6EA5};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "", .type = 2, .submenu_id = -1, .icon_color = 0};

    /* Find */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Find", .type = 0, .submenu_id = -1,
        .app_name = "Find", .icon_color = 0x4A8AC8};

    /* Help */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Help", .type = 0, .submenu_id = -1,
        .app_name = "Help", .icon_color = 0x3AA02C};

    /* Run */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Run...", .type = 0, .submenu_id = -1,
        .app_name = "Run", .icon_color = 0x808080};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "", .type = 2, .submenu_id = -1, .icon_color = 0};

    /* Shutdown */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "Shut Down", .type = 3, .submenu_id = -1,
        .app_name = "Shutdown", .icon_color = 0xC04030};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "", .type = 2, .submenu_id = -1, .icon_color = 0};

    /* WuBu Buddy on/off toggle. Type 4 = toggle. */
    g_main_items[g_main_count++] = (MainMenuItem){
        .label = "WuBu Buddy", .type = 4, .submenu_id = -1,
        .app_name = "WuBu Buddy", .icon_color = 0x3AA02C};
}

/* -- Submenu construction from MIME desktop entries ------------- */

void dosgui_startmenu_build_submenus(void) {
    MimeSystem *mime = wubu_mime_state();

    /* Reset submenu counts */
    g_programs.item_count = 0;
    g_accessories.item_count = 0;
    g_wubuos.item_count = 0;
    g_system.item_count = 0;

    /* Category mapping */
    struct { const char *cat; SmSubmenu *sub; } cats[] = {
        {"Accessories", &g_accessories},
        {"WuBuOS", &g_wubuos},
        {"System", &g_system},
        {"Development", &g_programs},
        {"Graphics", &g_programs},
        {"Internet", &g_programs},
        {"Office", &g_programs},
        {"AudioVideo", &g_programs},
        {"Game", &g_programs},
        {"Apps", &g_programs},
    };

    for (int i = 0; i < mime->desktop_entry_count; i++) {
        DesktopEntry *de = &mime->desktop_entries[i];
        if (de->hidden || de->no_display) continue;
        if (strcmp(de->type, "Application") != 0) continue;
        if (de->exec[0] == '\0') continue;

        /* Find matching category */
        SmSubmenu *target = &g_programs; /* default */
        for (size_t c = 0; c < sizeof(cats)/sizeof(cats[0]); c++) {
            if (strstr(de->categories, cats[c].cat)) {
                target = cats[c].sub;
                break;
            }
        }

        if (target->item_count < 12) {
            target->items[target->item_count] = (SmMenuItem){
                .type = SM_ITEM_APP, .submenu_id = 0, .icon_color = 0};
            strncpy(target->items[target->item_count].label, de->name, 47);
            strncpy(target->items[target->item_count].app_name, de->name, 47);
            target->item_count++;
        }
    }

    /* WuBuOS - ensure core apps present */
    for (int i = 0; i < 4; i++) {
        SmSubmenu *sub = &g_wubuos;
        const char *label = "", *app_name = "";
        uint32_t icon = 0;
        switch (i) {
            case 0: label = "HolyD Terminal"; app_name = "HolyD Terminal"; icon = 0x2C2C2C; break;
            case 1: label = "Temple REPL"; app_name = "Temple REPL"; icon = 0x3A6EA5; break;
            case 2: label = "Package Manager"; app_name = "Package Manager"; icon = 0xC08A2C; break;
            case 3: label = "Container Manager"; app_name = "Container Manager"; icon = 0x2C8A6E; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_wubuos;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count] = (SmMenuItem){
                .type = SM_ITEM_APP, .submenu_id = 0, .icon_color = icon};
            strncpy(sub->items[sub->item_count].label, label, 47);
            strncpy(sub->items[sub->item_count].app_name, app_name, 47);
            sub->item_count++;
        }
        next_wubuos:;
    }

    /* System */
    for (int i = 0; i < 2; i++) {
        SmSubmenu *sub = &g_system;
        const char *label = "", *app_name = "";
        uint32_t icon = 0;
        switch (i) {
            case 0: label = "File Manager"; app_name = "File Manager"; icon = 0xE8C84A; break;
            case 1: label = "Settings"; app_name = "Settings"; icon = 0x4A6EA5; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_system;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count] = (SmMenuItem){
                .type = SM_ITEM_APP, .submenu_id = 0, .icon_color = icon};
            strncpy(sub->items[sub->item_count].label, label, 47);
            strncpy(sub->items[sub->item_count].app_name, app_name, 47);
            sub->item_count++;
        }
        next_system:;
    }

    /* Accessories. icon_color gives each row the small colored glyph
     * XP draws beside the label (yellow folder, blue control, etc.). */
    for (int i = 0; i < 6; i++) {
        SmSubmenu *sub = &g_accessories;
        const char *label = "", *app_name = "";
        uint32_t icon = 0;
        switch (i) {
            case 0: label = "Notepad"; app_name = "Notepad"; icon = 0xFFFFFF; break;
            case 1: label = "Paint"; app_name = "Paint"; icon = 0xE0533A; break;
            case 2: label = "Editor"; app_name = "Editor"; icon = 0xFFFFFF; break;
            case 3: label = "WuBu Canvas"; app_name = "WuBu Canvas"; icon = 0x3AA02C; break;
            case 4: label = "FreeDoom"; app_name = "FreeDoom"; icon = 0x8B3A2A; break;
            case 5: label = "Calculator"; app_name = "Calculator"; icon = 0xD8C040; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_accessories;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count] = (SmMenuItem){
                .type = SM_ITEM_APP, .submenu_id = 0, .icon_color = icon};
            strncpy(sub->items[sub->item_count].label, label, 47);
            strncpy(sub->items[sub->item_count].app_name, app_name, 47);
            sub->item_count++;
        }
        next_accessories:;
    }
}
