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

    /* Programs submenu (index 0) */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Programs", 1, 0, ""};

    /* Documents -> File Manager */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Documents", 0, -1, "File Manager"};

    /* Settings */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Settings", 0, -1, "Settings"};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        "", 2, -1, ""};

    /* Find */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Find", 0, -1, "Find"};

    /* Help */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Help", 0, -1, "Help"};

    /* Run */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Run...", 0, -1, "Run"};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        "", 2, -1, ""};

    /* Shutdown */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Shut Down", 3, -1, "Shutdown"};
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
            target->items[target->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(target->items[target->item_count - 1].label, de->name, 47);
            strncpy(target->items[target->item_count - 1].app_name, de->name, 47);
        }
    }

    /* WuBuOS - ensure core apps present */
    for (int i = 0; i < 4; i++) {
        SmSubmenu *sub = &g_wubuos;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "HolyC Terminal"; app_name = "HolyC Terminal"; break;
            case 1: label = "Temple REPL"; app_name = "Temple REPL"; break;
            case 2: label = "Package Manager"; app_name = "Package Manager"; break;
            case 3: label = "Container Manager"; app_name = "Container Manager"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_wubuos;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_wubuos:;
    }

    /* System */
    for (int i = 0; i < 2; i++) {
        SmSubmenu *sub = &g_system;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "File Manager"; app_name = "File Manager"; break;
            case 1: label = "Settings"; app_name = "Settings"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_system;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_system:;
    }

    /* Accessories */
    for (int i = 0; i < 6; i++) {
        SmSubmenu *sub = &g_accessories;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "Notepad"; app_name = "Notepad"; break;
            case 1: label = "Paint"; app_name = "Paint"; break;
            case 2: label = "Editor"; app_name = "Editor"; break;
            case 3: label = "WuBu Canvas"; app_name = "WuBu Canvas"; break;
            case 4: label = "FreeDoom"; app_name = "FreeDoom"; break;
            case 5: label = "Calculator"; app_name = "Calculator"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_accessories;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_accessories:;
    }
}
