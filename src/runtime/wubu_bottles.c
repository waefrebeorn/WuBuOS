/*
 * wubu_bottles.c  --  WuBuOS Bottles/Lutris Integration Implementation
 *
 * Cell 480: Stub implementation for .wubu container format.
 *
 * This provides the API structure for Bottles/Lutris integration.
 * Full implementation would handle:
 *   - tar.zst / squashfs extraction
 *   - JSON serialization
 *   - Wine prefix operations
 *   - Dependency installation via winetricks
 *   - Flatpak manifest generation
 */

#define _GNU_SOURCE

#include "wubu_bottles.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <ftw.h>
#include <sys/wait.h>

/* -- Recursive directory removal (replaces system("rm -rf")) -------- */

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

static int rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/* ==================================================================
 * Bottle Management
 * ================================================================== */

WubuBottle *wubu_bottle_create(const char *name, WubuBottleType type) {
    if (!name) return NULL;
    WubuBottle *b = (WubuBottle*)calloc(1, sizeof(WubuBottle));
    if (!b) return NULL;

    strncpy(b->name, name, WUBU_BOTTLE_MAX_NAME - 1);
    b->type = type;
    strncpy(b->arch, "win64", sizeof(b->arch) - 1);
    b->created = time(NULL);
    b->last_run = 0;
    b->install_size = 0;
    b->installed = false;
    b->verified = false;

    switch (type) {
        case BOTTLE_TYPE_PROTON:
            b->runner = RUNNER_PROTON_GE;
            strncpy(b->runner_version, "GE-Proton9-20", sizeof(b->runner_version) - 1);
            break;
        case BOTTLE_TYPE_LUTRIS:
            b->runner = RUNNER_LUTRIS_WINE;
            strncpy(b->runner_version, "wine-ge-9-20", sizeof(b->runner_version) - 1);
            break;
        case BOTTLE_TYPE_BOTTLES:
            b->runner = RUNNER_WINE_GE;
            strncpy(b->runner_version, "wine-ge-9-20", sizeof(b->runner_version) - 1);
            break;
        default:
            b->runner = RUNNER_WINE_GE;
            strncpy(b->runner_version, "wine-ge-9-20", sizeof(b->runner_version) - 1);
            break;
    }

    b->dxvk.enabled = true;
    b->dxvk.async = true;
    strncpy(b->dxvk.hud, "off", sizeof(b->dxvk.hud) - 1);
    b->dxvk.frame_rate_limit = 0;
    b->dxvk.nvapi_hack = false;
    b->dxvk.present_mode_mailbox = false;

    b->gamescope.mode = 0;
    b->gamescope.fsr = false;
    b->gamescope.width = 0;
    b->gamescope.height = 0;
    strncpy(b->gamescope.filter, "fsr", sizeof(b->gamescope.filter) - 1);
    b->gamescope.refresh = 0;
    b->gamescope.hdr = false;
    b->gamescope.fullscreen = false;

    b->gpu_passthrough = true;
    b->hid_passthrough = true;
    b->audio_passthrough = true;
    b->network_isolated = false;

    return b;
}

void wubu_bottle_destroy(WubuBottle *bottle) {
    if (!bottle) return;
    free(bottle);
}

/* ==================================================================
 * Dependency Management
 * ================================================================== */

int wubu_bottle_add_dep(WubuBottle *bottle, WubuDependencyType type, const char *version) {
    if (!bottle || bottle->dep_count >= WUBU_BOTTLE_MAX_DEPS) return -1;
    WubuBottleDependency *dep = &bottle->deps[bottle->dep_count++];
    dep->type = type;
    dep->installed = false;

    switch (type) {
        case DEP_DXVK: strncpy(dep->name, "dxvk", sizeof(dep->name) - 1); break;
        case DEP_VKD3D: strncpy(dep->name, "vkd3d", sizeof(dep->name) - 1); break;
        case DEP_VCRUN:
            if (version) snprintf(dep->name, sizeof(dep->name), "vcrun%s", version);
            else strncpy(dep->name, "vcrun2019", sizeof(dep->name) - 1);
            break;
        case DEP_DOTNET:
            if (version) snprintf(dep->name, sizeof(dep->name), "dotnet%s", version);
            else strncpy(dep->name, "dotnet48", sizeof(dep->name) - 1);
            break;
        case DEP_CORE_FONTS: strncpy(dep->name, "corefonts", sizeof(dep->name) - 1); break;
        case DEP_D3DCOMPILER: strncpy(dep->name, "d3dcompiler_47", sizeof(dep->name) - 1); break;
        case DEP_DXVK_NVAPI: strncpy(dep->name, "dxvk-nvapi", sizeof(dep->name) - 1); break;
        case DEP_GAMEMODE: strncpy(dep->name, "gamemode", sizeof(dep->name) - 1); break;
        case DEP_MANGOHUD: strncpy(dep->name, "mangohud", sizeof(dep->name) - 1); break;
        case DEP_PROTONTRICKS: strncpy(dep->name, "protontricks", sizeof(dep->name) - 1); break;
        default: strncpy(dep->name, "custom", sizeof(dep->name) - 1); break;
    }
    if (version) strncpy(dep->version, version, sizeof(dep->version) - 1);
    return 0;
}

int wubu_bottle_remove_dep(WubuBottle *bottle, WubuDependencyType type) {
    if (!bottle) return -1;
    for (int i = 0; i < bottle->dep_count; i++) {
        if (bottle->deps[i].type == type) {
            for (int j = i; j < bottle->dep_count - 1; j++) {
                bottle->deps[j] = bottle->deps[j + 1];
            }
            bottle->dep_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_bottle_install_deps(WubuBottle *bottle, const char *prefix_path) {
    (void)bottle; (void)prefix_path;
    return 0;
}

bool wubu_bottle_dep_available(WubuRunnerType runner, WubuDependencyType type) {
    switch (type) {
        case DEP_DXVK:
        case DEP_VKD3D:
        case DEP_CORE_FONTS:
        case DEP_D3DCOMPILER:
        case DEP_MANGOHUD:
        case DEP_GAMEMODE:
            return (runner == RUNNER_WINE_GE || runner == RUNNER_PROTON_GE ||
                    runner == RUNNER_PROTON_STEAM || runner == RUNNER_LUTRIS_WINE ||
                    runner == RUNNER_WINE_STAGING);
        case DEP_VCRUN:
        case DEP_DOTNET:
        case DEP_DXVK_NVAPI:
        case DEP_PROTONTRICKS:
            return (runner == RUNNER_WINE_GE || runner == RUNNER_PROTON_GE ||
                    runner == RUNNER_PROTON_STEAM || runner == RUNNER_LUTRIS_WINE);
        default:
            return false;
    }
}

/* ==================================================================
 * Mount Management
 * ================================================================== */

int wubu_bottle_add_mount(WubuBottle *bottle, const char *host, const char *guest, bool readonly) {
    if (!bottle || !host || !guest || bottle->mount_count >= WUBU_BOTTLE_MAX_MOUNTS) return -1;
    WubuBottleMount *m = &bottle->mounts[bottle->mount_count++];
    strncpy(m->host_path, host, sizeof(m->host_path) - 1);
    strncpy(m->guest_path, guest, sizeof(m->guest_path) - 1);
    m->readonly = readonly;
    m->required = false;
    return 0;
}

int wubu_bottle_remove_mount(WubuBottle *bottle, const char *guest_path) {
    if (!bottle || !guest_path) return -1;
    for (int i = 0; i < bottle->mount_count; i++) {
        if (strcmp(bottle->mounts[i].guest_path, guest_path) == 0) {
            for (int j = i; j < bottle->mount_count - 1; j++) {
                bottle->mounts[j] = bottle->mounts[j + 1];
            }
            bottle->mount_count--;
            return 0;
        }
    }
    return -1;
}

/* ==================================================================
 * Environment
 * ================================================================== */

int wubu_bottle_set_env(WubuBottle *bottle, const char *key, const char *value) {
    if (!bottle || !key || bottle->env_count >= WUBU_BOTTLE_MAX_ENV) return -1;
    WubuBottleEnv *e = &bottle->env[bottle->env_count++];
    strncpy(e->key, key, sizeof(e->key) - 1);
    if (value) strncpy(e->value, value, sizeof(e->value) - 1);
    return 0;
}

const char *wubu_bottle_get_env(WubuBottle *bottle, const char *key) {
    if (!bottle || !key) return NULL;
    for (int i = 0; i < bottle->env_count; i++) {
        if (strcmp(bottle->env[i].key, key) == 0) {
            return bottle->env[i].value;
        }
    }
    return NULL;
}

/* ==================================================================
 * JSON Helpers
 * ================================================================== */

static const char *json_find_string_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return NULL;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return NULL;
    const char *quote = strchr(colon, '"');
    if (!quote) return NULL;
    const char *end = strchr(quote + 1, '"');
    if (!end) return NULL;
    return quote + 1;
}

static int json_find_int_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return 0;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return 0;
    while (*colon && !isdigit((unsigned char)*colon) && *colon != '-') colon++;
    return atoi(colon);
}

static bool json_find_bool_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return false;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return false;
    while (*colon && isspace((unsigned char)*colon)) colon++;
    if (strncmp(colon, "true", 4) == 0) return true;
    if (strncmp(colon, "false", 5) == 0) return false;
    return false;
}

const char *wubu_bottle_dep_type_name(WubuDependencyType type) {
    switch (type) {
        case DEP_DXVK: return "dxvk";
        case DEP_VKD3D: return "vkd3d";
        case DEP_VCRUN: return "vcrun";
        case DEP_DOTNET: return "dotnet";
        case DEP_CORE_FONTS: return "corefonts";
        case DEP_D3DCOMPILER: return "d3dcompiler_47";
        case DEP_DXVK_NVAPI: return "dxvk-nvapi";
        case DEP_GAMEMODE: return "gamemode";
        case DEP_MANGOHUD: return "mangohud";
        case DEP_PROTONTRICKS: return "protontricks";
        default: return "custom";
    }
}

/* ==================================================================
 * Bottle Save / Load
 * ================================================================== */

int wubu_bottle_save(WubuBottle *bottle, const char *output_path) {
    if (!bottle || !output_path) return -1;
    char path_copy[512];
    strncpy(path_copy, output_path, sizeof(path_copy) - 1);
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(path_copy, 0755);
    }

    char json[65536];
    char *p = json;
    p += snprintf(p, sizeof(json) - (p - json), "{\n");
    p += snprintf(p, sizeof(json) - (p - json), "  \"name\": \"%s\",\n", bottle->name);
    p += snprintf(p, sizeof(json) - (p - json), "  \"version\": \"%s\",\n", bottle->version);
    p += snprintf(p, sizeof(json) - (p - json), "  \"type\": %d,\n", bottle->type);
    p += snprintf(p, sizeof(json) - (p - json), "  \"runner\": %d,\n", bottle->runner);
    p += snprintf(p, sizeof(json) - (p - json), "  \"runner_version\": \"%s\",\n", bottle->runner_version);
    p += snprintf(p, sizeof(json) - (p - json), "  \"arch\": \"%s\",\n", bottle->arch);
    p += snprintf(p, sizeof(json) - (p - json), "  \"installed\": %s,\n", bottle->installed ? "true" : "false");
    p += snprintf(p, sizeof(json) - (p - json), "  \"gpu_passthrough\": %s,\n", bottle->gpu_passthrough ? "true" : "false");
    p += snprintf(p, sizeof(json) - (p - json), "  \"hid_passthrough\": %s,\n", bottle->hid_passthrough ? "true" : "false");
    p += snprintf(p, sizeof(json) - (p - json), "  \"audio_passthrough\": %s,\n", bottle->audio_passthrough ? "true" : "false");
    p += snprintf(p, sizeof(json) - (p - json), "  \"network_isolated\": %s,\n", bottle->network_isolated ? "true" : "false");
    p += snprintf(p, sizeof(json) - (p - json), "  \"entrypoint\": \"%s\",\n", bottle->exe_path[0] ? bottle->exe_path : "wine");
    p += snprintf(p, sizeof(json) - (p - json), "  \"workdir\": \"%s\",\n", bottle->work_dir);
    p += snprintf(p, sizeof(json) - (p - json), "  \"args\": \"%s\",\n", bottle->args);

    p += snprintf(p, sizeof(json) - (p - json), "  \"environment\": {\n");
    for (int i = 0; i < bottle->env_count; i++) {
        p += snprintf(p, sizeof(json) - (p - json), "    \"%s\": \"%s\"%s\n",
            bottle->env[i].key, bottle->env[i].value, i < bottle->env_count - 1 ? "," : "");
    }
    p += snprintf(p, sizeof(json) - (p - json), "  },\n");

    p += snprintf(p, sizeof(json) - (p - json), "  \"dependencies\": [\n");
    for (int i = 0; i < bottle->dep_count; i++) {
        p += snprintf(p, sizeof(json) - (p - json), "    {\"name\": \"%s\", \"version\": \"%s\"}%s\n",
            bottle->deps[i].name, bottle->deps[i].version, i < bottle->dep_count - 1 ? "," : "");
    }
    p += snprintf(p, sizeof(json) - (p - json), "  ],\n");

    p += snprintf(p, sizeof(json) - (p - json), "  \"mounts\": [\n");
    for (int i = 0; i < bottle->mount_count; i++) {
        p += snprintf(p, sizeof(json) - (p - json), "    {\"host\": \"%s\", \"guest\": \"%s\", \"readonly\": %s}%s\n",
            bottle->mounts[i].host_path, bottle->mounts[i].guest_path,
            bottle->mounts[i].readonly ? "true" : "false",
            i < bottle->mount_count - 1 ? "," : "");
    }
    p += snprintf(p, sizeof(json) - (p - json), "  ],\n");

    p += snprintf(p, sizeof(json) - (p - json), "  \"dxvk\": {\"enabled\": %s, \"async\": %s, \"hud\": \"%s\", \"frame_rate_limit\": %d},\n",
        bottle->dxvk.enabled ? "true" : "false", bottle->dxvk.async ? "true" : "false",
        bottle->dxvk.hud, bottle->dxvk.frame_rate_limit);

    p += snprintf(p, sizeof(json) - (p - json), "  \"gamescope\": {\"mode\": %d, \"fsr\": %s, \"filter\": \"%s\"}\n",
        bottle->gamescope.mode, bottle->gamescope.fsr ? "true" : "false", bottle->gamescope.filter);

    p += snprintf(p, sizeof(json) - (p - json), "}\n");

    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return 0;
}

WubuBottle *wubu_bottle_load(const char *package_path) {
    if (!package_path) return NULL;
    FILE *f = fopen(package_path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    WubuBottle *bottle = calloc(1, sizeof(WubuBottle));
    if (!bottle) { free(buf); return NULL; }

    const char *p = json_find_string_literal(buf, "name");
    if (p) strncpy(bottle->name, p, sizeof(bottle->name) - 1);
    bottle->type = (WubuBottleType)json_find_int_literal(buf, "type");
    bottle->runner = (WubuRunnerType)json_find_int_literal(buf, "runner");
    p = json_find_string_literal(buf, "runner_version");
    if (p) strncpy(bottle->runner_version, p, sizeof(bottle->runner_version) - 1);
    p = json_find_string_literal(buf, "arch");
    if (p) strncpy(bottle->arch, p, sizeof(bottle->arch) - 1);
    bottle->installed = json_find_bool_literal(buf, "installed");
    bottle->gpu_passthrough = json_find_bool_literal(buf, "gpu_passthrough");
    bottle->hid_passthrough = json_find_bool_literal(buf, "hid_passthrough");
    bottle->audio_passthrough = json_find_bool_literal(buf, "audio_passthrough");
    bottle->network_isolated = json_find_bool_literal(buf, "network_isolated");
    p = json_find_string_literal(buf, "entrypoint");
    if (p) strncpy(bottle->exe_path, p, sizeof(bottle->exe_path) - 1);
    p = json_find_string_literal(buf, "workdir");
    if (p) strncpy(bottle->work_dir, p, sizeof(bottle->work_dir) - 1);
    p = json_find_string_literal(buf, "args");
    if (p) strncpy(bottle->args, p, sizeof(bottle->args) - 1);

    const char *dxvk = strstr(buf, "\"dxvk\"");
    if (dxvk) {
        bottle->dxvk.enabled = json_find_bool_literal(dxvk, "enabled");
        bottle->dxvk.async = json_find_bool_literal(dxvk, "async");
        p = json_find_string_literal(dxvk, "hud");
        if (p) strncpy(bottle->dxvk.hud, p, sizeof(bottle->dxvk.hud) - 1);
        bottle->dxvk.frame_rate_limit = json_find_int_literal(dxvk, "frame_rate_limit");
    }

    const char *gamescope = strstr(buf, "\"gamescope\"");
    if (gamescope) {
        bottle->gamescope.mode = json_find_int_literal(gamescope, "mode");
        bottle->gamescope.fsr = json_find_bool_literal(gamescope, "fsr");
        p = json_find_string_literal(gamescope, "filter");
        if (p) strncpy(bottle->gamescope.filter, p, sizeof(bottle->gamescope.filter) - 1);
    }

    free(buf);
    return bottle;
}

/* ==================================================================
 * Bottle Install / Uninstall / Run
 * ================================================================== */

int wubu_bottle_install(WubuBottle *bottle, const char *install_dir) {
    if (!bottle || !install_dir) return -1;
    mkdir(install_dir, 0755);
    char prefix_dir[512];
    snprintf(prefix_dir, sizeof(prefix_dir), "%s/prefix", install_dir);
    mkdir(prefix_dir, 0755);
    char drive_c[512];
    snprintf(drive_c, sizeof(drive_c), "%s/drive_c", prefix_dir);
    mkdir(drive_c, 0755);

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/bottle.json", install_dir);
    WubuBottle tmp = *bottle;
    snprintf(tmp.prefix_path, sizeof(tmp.prefix_path), "%s", prefix_dir);
    snprintf(tmp.rootfs_path, sizeof(tmp.rootfs_path), "%s/rootfs", install_dir);
    mkdir(tmp.rootfs_path, 0755);

    int ret = wubu_bottle_save(&tmp, meta_path);
    if (ret < 0) return ret;

    bottle->installed = true;
    strncpy(bottle->prefix_path, prefix_dir, sizeof(bottle->prefix_path) - 1);
    strncpy(bottle->rootfs_path, tmp.rootfs_path, sizeof(bottle->rootfs_path) - 1);
    return 0;
}

int wubu_bottle_uninstall(WubuBottle *bottle) {
    if (!bottle) return -1;
    if (bottle->prefix_path[0]) {
        rm_rf(bottle->prefix_path);
    }
    if (bottle->rootfs_path[0]) {
        rm_rf(bottle->rootfs_path);
    }
    bottle->installed = false;
    bottle->prefix_path[0] = '\0';
    bottle->rootfs_path[0] = '\0';
    return 0;
}

int wubu_bottle_run(WubuBottle *bottle) {
    if (!bottle) return -1;
    char cmd[2048];
    if (bottle->type == BOTTLE_TYPE_PROTON && bottle->prefix_path[0]) {
        snprintf(cmd, sizeof(cmd), "%s/proton launch -- \"%s\" %s 2>/dev/null",
            bottle->prefix_path, bottle->exe_path[0] ? bottle->exe_path : "wine", bottle->args);
    } else {
        snprintf(cmd, sizeof(cmd), "wine \"%s\" %s 2>/dev/null",
            bottle->exe_path[0] ? bottle->exe_path : "explorer.exe", bottle->args);
    }
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(1);
    }
    int ret;
    waitpid(pid, &ret, 0);
    bottle->last_run = time(NULL);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) return 0;
    return -1;
}

/* ==================================================================
 * Bottles Import/Export
 * ================================================================== */

int wubu_bottle_import_bottles(const char *bottles_prefix, WubuBottle *bottle) {
    if (!bottles_prefix || !bottle) return -1;
    char bottles_path[512];
    snprintf(bottles_path, sizeof(bottles_path), "%s/bottles.json", bottles_prefix);
    FILE *f = fopen(bottles_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    const char *name = json_find_string_literal(buf, "name");
    if (name) strncpy(bottle->name, name, sizeof(bottle->name) - 1);
    const char *dxvk = strstr(buf, "\"dxvk\"");
    if (dxvk) {
        bottle->dxvk.enabled = json_find_bool_literal(dxvk, "enabled");
        bottle->dxvk.async = json_find_bool_literal(dxvk, "async");
    }
    free(buf);
    bottle->type = BOTTLE_TYPE_BOTTLES;
    bottle->runner = RUNNER_WINE_GE;
    strncpy(bottle->arch, "win64", sizeof(bottle->arch) - 1);
    return 0;
}

int wubu_bottle_export_bottles(WubuBottle *bottle, const char *output_dir) {
    if (!bottle || !output_dir) return -1;
    mkdir(output_dir, 0755);
    char path[512];
    snprintf(path, sizeof(path), "%s/bottle.json", output_dir);
    return wubu_bottle_save(bottle, path);
}

int wubu_bottle_import_lutris(const char *lutris_prefix, WubuBottle *bottle) {
    if (!lutris_prefix || !bottle) return -1;
    char game_yml[512];
    snprintf(game_yml, sizeof(game_yml), "%s/game.yml", lutris_prefix);
    FILE *f = fopen(game_yml, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    const char *name = json_find_string_literal(buf, "name");
    if (name) strncpy(bottle->name, name, sizeof(bottle->name) - 1);
    const char *runner = json_find_string_literal(buf, "runner");
    if (runner && strcmp(runner, "wine") == 0) {
        bottle->type = BOTTLE_TYPE_LUTRIS;
        bottle->runner = RUNNER_LUTRIS_WINE;
    }
    const char *exe = json_find_string_literal(buf, "exe");
    if (exe) strncpy(bottle->exe_path, exe, sizeof(bottle->exe_path) - 1);
    const char *prefix = json_find_string_literal(buf, "prefix");
    if (prefix) strncpy(bottle->prefix_path, prefix, sizeof(bottle->prefix_path) - 1);

    free(buf);
    strncpy(bottle->arch, "win64", sizeof(bottle->arch) - 1);
    return 0;
}

int wubu_bottle_export_lutris(WubuBottle *bottle, const char *output_path) {
    if (!bottle || !output_path) return -1;
    char yml[4096];
    snprintf(yml, sizeof(yml),
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"runner\": \"%s\",\n"
        "  \"exe\": \"%s\",\n"
        "  \"prefix\": \"%s\",\n"
        "  \"arch\": \"%s\",\n"
        "  \"args\": \"%s\"\n"
        "}\n",
        bottle->name,
        bottle->type == BOTTLE_TYPE_LUTRIS ? "wine" : "proton",
        bottle->exe_path[0] ? bottle->exe_path : "/usr/bin/wine",
        bottle->prefix_path[0] ? bottle->prefix_path : "",
        bottle->arch,
        bottle->args);
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    fwrite(yml, 1, strlen(yml), f);
    fclose(f);
    return 0;
}

int wubu_bottle_flatpak_manifest(WubuBottle *bottle, const char *output_path) {
    if (!bottle || !output_path) return -1;
    char manifest[8192];
    snprintf(manifest, sizeof(manifest),
        "{\n"
        "  \"app-id\": \"com.wubu.%s\",\n"
        "  \"runtime\": \"org.freedesktop.Platform\",\n"
        "  \"runtime-version\": \"22.08\",\n"
        "  \"sdk\": \"org.freedesktop.Sdk\",\n"
        "  \"command\": \"%s\",\n"
        "  \"modules\": [\n"
        "    {\n"
        "      \"name\": \"%s\",\n"
        "      \"build-options\": {\n"
        "        \"env\": {\n"
        "          \"WINEPREFIX\": \"/app/prefix\",\n"
        "          \"WINEARCH\": \"win64\"\n"
        "        }\n"
        "      },\n"
        "      \"sources\": []\n"
        "    }\n"
        "  ],\n"
        "  \"finish-args\": [\n"
        "    \"--share=ipc\",\n"
        "    \"--socket=x11\",\n"
        "    \"--socket=wayland\",\n"
        "    \"--device=dri\"\n"
        "  ]\n"
        "}\n",
        bottle->name,
        bottle->exe_path[0] ? bottle->exe_path : "wine",
        bottle->runner_version);
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    fwrite(manifest, 1, strlen(manifest), f);
    fclose(f);
    return 0;
}

bool wubu_bottle_flatpak_runtime_available(const char *runtime) {
    if (!runtime) return false;
    
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "flatpak --version >/dev/null 2>&1", (char*)NULL);
        _exit(1);
    }
    int ret;
    waitpid(pid, &ret, 0);
    if (!WIFEXITED(ret) || WEXITSTATUS(ret) != 0) return false;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "flatpak info %s >/dev/null 2>&1", runtime);
    pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(1);
    }
    waitpid(pid, &ret, 0);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) return true;
    return false;
}

/* ==================================================================
 * Query
 * ================================================================== */

int wubu_bottle_list(const char *install_dir, WubuBottle ***out_bottles, int *count) {
    if (!install_dir || !out_bottles || !count) return -1;
    *out_bottles = NULL;
    *count = 0;
    DIR *d = opendir(install_dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s/bottle.json", install_dir, ent->d_name);
        WubuBottle *bottle = wubu_bottle_load(meta_path);
        if (!bottle) continue;
        (*count)++;
        *out_bottles = realloc(*out_bottles, sizeof(WubuBottle *) * (*count));
        (*out_bottles)[(*count) - 1] = bottle;
    }
    closedir(d);
    return *count;
}

const WubuBottle *wubu_bottle_find(const char *install_dir, const char *name) {
    if (!install_dir || !name) return NULL;
    WubuBottle **bottles = NULL;
    int count = 0;
    int ret = wubu_bottle_list(install_dir, &bottles, &count);
    if (ret < 0) return NULL;
    for (int i = 0; i < count; i++) {
        if (strcmp(bottles[i]->name, name) == 0) {
            const WubuBottle *result = bottles[i];
            free(bottles);
            return result;
        }
    }
    free(bottles);
    return NULL;
}

bool wubu_bottle_verify(WubuBottle *bottle) {
    if (!bottle) return false;
    if (bottle->prefix_path[0]) {
        struct stat st;
        if (stat(bottle->prefix_path, &st) != 0) return false;
    }
    if (bottle->prefix_path[0]) {
        char meta[512];
        snprintf(meta, sizeof(meta), "%s/bottle.json", bottle->prefix_path);
        struct stat st;
        if (stat(meta, &st) != 0) return false;
    }
    return true;
}
