/*
 * startmenu.h  --  WuBuOS Win98-Style Start Menu
 */

#ifndef WUBU_STARTMENU_H
#define WUBU_STARTMENU_H

#include <stdint.h>

#define STARTMENU_MAX_ENTRIES 32
#define SM_LABEL_MAX 64

typedef enum {
    SM_PROGRAM  = 0,
    SM_SYSTEM   = 1,
    SM_SEPARATOR = 2,
} SmEntryType;

typedef struct {
    char      label[SM_LABEL_MAX];
    SmEntryType type;
    void    (*action)(void);
    int       enabled;
    int       has_submenu;
} StartMenuEntry;

/* -- Init ----------------------------------------------------- */
void startmenu_init(void);

/* -- Entry Management ----------------------------------------- */
int  startmenu_add_entry(const char *label, int type, void (*action)(void));
void startmenu_remove_entry(int index);
int  startmenu_count(void);
StartMenuEntry *startmenu_get_entry(int index);

/* -- Open/Close ----------------------------------------------- */
void startmenu_open(int x, int y);
void startmenu_close(void);
int  startmenu_is_open(void);
void startmenu_toggle(int x, int y);

/* -- Interaction ---------------------------------------------- */
void startmenu_set_hover(int index);
int  startmenu_get_hover(void);
int  startmenu_click(int index);
int  startmenu_handle_mouse(int mx, int my);

/* -- Rendering ------------------------------------------------ */
void startmenu_draw(void);

/* -- Query ---------------------------------------------------- */
int startmenu_get_width(void);
int startmenu_get_height(void);
int startmenu_is_inside(int mx, int my);

#endif
