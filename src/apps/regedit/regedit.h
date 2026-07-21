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

/* Opaque state */
typedef struct RegeditState RegeditState;

/* API */
RegeditState* regedit_create(void);
void regedit_destroy(RegeditState *reg);

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

/* RegKey is a public leaf struct (navigable by tests/UI); RegeditState stays
 * opaque. Mirrors the WubuLayer convention in wubu_canvas.h. */
typedef struct RegKey RegKey;
struct RegKey {
    char name[REG_MAX_NAME];
    RegKey *parent;
    RegKey *children;
    RegKey *next_sibling;
    int child_count;
};

/* Read-only inspection accessors (opaque-struct safe; for tests/debug). */
const RegKey *regedit_get_current_key(const RegeditState *reg);
const RegKey *regedit_get_root_key(const RegeditState *reg, int i);
int           regedit_value_count(const RegeditState *reg);
int  regedit_get_expand_depth(const RegeditState *reg);
bool regedit_search_open(const RegeditState *reg);
const char *regedit_get_search_text(const RegeditState *reg);
/* UI-state setters (for tests / programmatic control). */
void regedit_set_search_text(RegeditState *reg, const char *text);
void regedit_set_search_open(RegeditState *reg, bool open);
int regedit_set_string(RegeditState *reg, const char *name, const char *data);
int regedit_set_dword(RegeditState *reg, const char *name, uint32_t data);

/* Navigation */
void regedit_expand_key(RegeditState *reg, void *key);
void regedit_collapse_key(RegeditState *reg, void *key);

#endif