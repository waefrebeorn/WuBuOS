/*
 * regedit.c  --  Windows Registry Editor Clone - minimal stub
 */

#include "regedit.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>
#include <string.h>

/* RegKey is defined in regedit.h (public leaf struct). */

struct RegValue {
    char name[REG_MAX_NAME];
    RegType type;
    uint8_t data[REG_MAX_DATA];
    int data_len;
};

struct RegeditState {
    struct RegKey root_keys[6];
    struct RegKey *current_key;
    struct RegValue values[1024];
    int value_count;
    int expanded_keys[REG_MAX_DEPTH];
    int expand_depth;
    char search_text[REG_MAX_NAME];
    bool search_dialog_open;
};

RegeditState* regedit_create(void) {
    return calloc(1, sizeof(RegeditState));
}

void regedit_destroy(RegeditState *reg) {
    free(reg);
}

void regedit_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, RegeditState *reg) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)reg;
}

DosGuiWindow* regedit_launch(void) {
    return dosgui_wm_create(80, 60, 800, 600, "Registry Editor");
}

void regedit_init_roots(RegeditState *reg) {
    const char *names[6] = {"HKEY_CLASSES_ROOT","HKEY_CURRENT_USER","HKEY_LOCAL_MACHINE","HKEY_USERS","HKEY_CURRENT_CONFIG","HKEY_DYN_DATA"};
    for (int i = 0; i < 6; i++) {
        strncpy(reg->root_keys[i].name, names[i], REG_MAX_NAME - 1);
        reg->root_keys[i].parent = NULL;
        reg->root_keys[i].children = NULL;
        reg->root_keys[i].next_sibling = (i < 5) ? &reg->root_keys[i + 1] : NULL;
        reg->root_keys[i].child_count = 0;
    }
    reg->current_key = &reg->root_keys[1];
}

void* regedit_create_key(RegeditState *reg, void *parent, const char *name) {
    struct RegKey *p = (struct RegKey*)parent;
    if (p->child_count >= 100) return NULL;
    struct RegKey *child = malloc(sizeof(struct RegKey));
    if (!child) return NULL;
    strncpy(child->name, name, REG_MAX_NAME - 1);
    child->parent = p;
    child->children = NULL;
    child->next_sibling = p->children;
    child->child_count = 0;
    p->children = child;
    p->child_count++;
    return child;
}

int regedit_set_string(RegeditState *reg, const char *name, const char *data) {
    if (reg->value_count >= 1024) return -1;
    struct RegValue *v = &reg->values[reg->value_count++];
    strncpy(v->name, name, REG_MAX_NAME - 1);
    v->type = REG_SZ;
    memcpy(v->data, data, strlen(data) + 1);
    v->data_len = strlen(data) + 1;
    return 0;
}

int regedit_set_dword(RegeditState *reg, const char *name, uint32_t data) {
    if (reg->value_count >= 1024) return -1;
    struct RegValue *v = &reg->values[reg->value_count++];
    strncpy(v->name, name, REG_MAX_NAME - 1);
    v->type = REG_DWORD;
    memcpy(v->data, &data, 4);
    v->data_len = 4;
    return 0;
}

void regedit_expand_key(RegeditState *reg, void *key) {
    if (reg->expand_depth < REG_MAX_DEPTH) {
        reg->expanded_keys[reg->expand_depth++] = (intptr_t)key;
    }
}

int regedit_get_expand_depth(const RegeditState *reg) {
    return reg ? reg->expand_depth : 0;
}
bool regedit_search_open(const RegeditState *reg) {
    return reg ? reg->search_dialog_open : false;
}
const char *regedit_get_search_text(const RegeditState *reg) {
    return reg ? reg->search_text : "";
}
const RegKey *regedit_get_current_key(const RegeditState *reg) {
    return reg ? reg->current_key : NULL;
}
const RegKey *regedit_get_root_key(const RegeditState *reg, int i) {
    return (reg && i >= 0 && i < 6) ? &reg->root_keys[i] : NULL;
}
int regedit_value_count(const RegeditState *reg) {
    return reg ? reg->value_count : 0;
}
void regedit_set_search_text(RegeditState *reg, const char *text) {
    if (!reg || !text) return;
    strncpy(reg->search_text, text, sizeof(reg->search_text) - 1);
    reg->search_text[sizeof(reg->search_text) - 1] = '\0';
}
void regedit_set_search_open(RegeditState *reg, bool open) {
    if (reg) reg->search_dialog_open = open;
}

void regedit_collapse_key(RegeditState *reg, void *key) {
    for (int i = 0; i < reg->expand_depth; i++) {
        if (reg->expanded_keys[i] == (intptr_t)key) {
            for (int j = i; j < reg->expand_depth - 1; j++) reg->expanded_keys[j] = reg->expanded_keys[j + 1];
            reg->expand_depth--;
            break;
        }
    }
}