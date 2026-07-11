/*
 * regedit.h  --  Windows Registry Editor Clone
 * Tree view with HKCR, HKCU, HKLM, HKU, HKCC, HKDD
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_REGEDIT_H
#define WUBU_REGEDIT_H

#include <stdint.h>
#include <stdbool.h>

/* Constants for registry limits */
#define REG_MAX_NAME 128
#define REG_MAX_DATA 1024
#define REG_MAX_DEPTH 16

typedef struct DosGuiWindow DosGuiWindow;

/* Registry value types */
typedef enum {
    REG_NONE = 0,
    REG_SZ,
    REG_DWORD,
    REG_QWORD,
    REG_BINARY,
    REG_MULTI_SZ,
    REG_EXPAND_SZ
} RegType;

/* Registry node (fields exposed for tests / inspection) */
struct RegKey {
    char name[REG_MAX_NAME];
    struct RegKey *parent;
    struct RegKey *children;
    struct RegKey *next_sibling;
    int child_count;
};

typedef struct RegKey RegKey;

struct RegValue {
    char name[REG_MAX_NAME];
    RegType type;
    uint8_t data[REG_MAX_DATA];
    int data_len;
};

typedef struct RegValue RegValue;

/* Regedit state (fields exposed for tests / inspection) */
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

typedef struct RegeditState RegeditState;

/* API */
RegeditState* regedit_create(void);
void regedit_destroy(RegeditState *reg);

void regedit_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, RegeditState *reg);
DosGuiWindow* regedit_launch(void);

/* Key management */
void regedit_init_roots(RegeditState *reg);
void* regedit_create_key(RegeditState *reg, void *parent, const char *name);

/* Value management */
int regedit_set_string(RegeditState *reg, const char *name, const char *data);
int regedit_set_dword(RegeditState *reg, const char *name, uint32_t data);

/* Navigation */
void regedit_expand_key(RegeditState *reg, void *key);
void regedit_collapse_key(RegeditState *reg, void *key);

#endif