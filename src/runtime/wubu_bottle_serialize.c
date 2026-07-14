/*
 * wubu_bottle_serialize.c -- WuBuOS Bottles: JSON (de)serialization
 *
 * Self-contained serialization concern split out of wubu_bottle_lifecycle.c.
 * Owns the bottle JSON schema: dependency-type naming, save (emit) and load
 * (parse) of a WubuBottle. Depends only on the shared public types
 * (wubu_bottles.h) and the JSON literal helpers declared in
 * wubu_bottles_internal.h. C11, opaque-safe, no god headers.
 */

#include "wubu_bottles_internal.h"

#include <stdio.h>

/* -- Dependency type name (schema string) ------------------------ */

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

/* -- Bottle Save -------------------------------------------------- */

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

/* -- Bottle Load -------------------------------------------------- */

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
