/*
 * wubu_atspi.h -- AT-SPI2: the accessibility object model.
 *
 * AT-SPI2 exposes every UI element as an object with a ROLE, a NAME and a
 * position in a tree.  A screen reader announces the role, so a wrong role name
 * is not a cosmetic bug -- it tells the user the wrong thing about the control
 * under the cursor.
 *
 * The rules:
 *
 *   * Role names are the CANONICAL AT-SPI names ("push button", "check box",
 *     "menu item"), not enum names.  "check box" is two words and "push
 *     button" is not "button" -- a mapping written from memory gets these
 *     wrong, and the failure is a screen reader calling a checkbox a button.
 *   * A node with NO name is valid: containers routinely have none.  An empty
 *     name is data, not an error.
 *   * child_count == -1 means "not cached", which is DIFFERENT from zero.
 *     Treating -1 as zero makes a reader believe a container is empty.
 *   * An object path must be a valid D-Bus object path: it starts with '/', has
 *     no trailing '/', and no empty component.  A path that violates this is
 *     rejected by the bus, so validating here turns a confusing bus error into
 *     an actionable one.
 *   * A bus name is either a UNIQUE name (":1.42") or a WELL-KNOWN name
 *     ("org.a11y.Bus"), and the two forms are not interchangeable.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_ATSPI_H
#define WUBU_ATSPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_dbus.h"

typedef enum {
    WB_ATSPI_ROLE_INVALID = 0,
    WB_ATSPI_ROLE_ACCELERATOR_LABEL,
    WB_ATSPI_ROLE_ALERT,
    WB_ATSPI_ROLE_ANIMATION,
    WB_ATSPI_ROLE_APPLICATION,
    WB_ATSPI_ROLE_BUTTON,
    WB_ATSPI_ROLE_CHECK_BOX,
    WB_ATSPI_ROLE_COMBO_BOX,
    WB_ATSPI_ROLE_DIALOG,
    WB_ATSPI_ROLE_FRAME,
    WB_ATSPI_ROLE_IMAGE,
    WB_ATSPI_ROLE_LABEL,
    WB_ATSPI_ROLE_LINK,
    WB_ATSPI_ROLE_LIST,
    WB_ATSPI_ROLE_LIST_ITEM,
    WB_ATSPI_ROLE_MENU,
    WB_ATSPI_ROLE_MENU_BAR,
    WB_ATSPI_ROLE_MENU_ITEM,
    WB_ATSPI_ROLE_PAGE,
    WB_ATSPI_ROLE_PARAGRAPH,
    WB_ATSPI_ROLE_PASSWORD_TEXT,
    WB_ATSPI_ROLE_POPUP_MENU,
    WB_ATSPI_ROLE_PROGRESS_BAR,
    WB_ATSPI_ROLE_PUSH_BUTTON,
    WB_ATSPI_ROLE_RADIO_BUTTON,
    WB_ATSPI_ROLE_SCROLL_BAR,
    WB_ATSPI_ROLE_SECTION,
    WB_ATSPI_ROLE_SLIDER,
    WB_ATSPI_ROLE_SPIN_BUTTON,
    WB_ATSPI_ROLE_STATUS_BAR,
    WB_ATSPI_ROLE_TABLE,
    WB_ATSPI_ROLE_TABLE_CELL,
    WB_ATSPI_ROLE_TAB,
    WB_ATSPI_ROLE_TAB_LIST,
    WB_ATSPI_ROLE_TEXT,
    WB_ATSPI_ROLE_TERMINAL,
    WB_ATSPI_ROLE_TOOL_BAR,
    WB_ATSPI_ROLE_TREE,
    WB_ATSPI_ROLE_TREE_ITEM,
    WB_ATSPI_ROLE_WINDOW,
    WB_ATSPI_ROLE_COUNT
} wb_atspi_role_t;

/* The CANONICAL AT-SPI role name, or NULL for an unknown role. */
const char *wb_atspi_role_name(wb_atspi_role_t r);

/* The role for a canonical name, or WB_ATSPI_ROLE_INVALID. */
wb_atspi_role_t wb_atspi_role_from_name(const char *name);

/* -- references and paths ----------------------------------------- */

typedef struct {
    char name[128];      /* a unique (":1.42") or well-known ("org.a11y.Bus") name */
    char path[256];      /* a D-Bus object path */
} wb_atspi_ref_t;

/* Is this a valid D-Bus object path? */
bool wb_atspi_valid_path(const char *path);

/* Is this a valid unique (":1.42") or well-known ("org.a11y.Bus") name? */
bool wb_atspi_valid_bus_name(const char *name);

/* -- an accessible node ------------------------------------------- */

typedef struct {
    wb_atspi_ref_t  ref;
    wb_atspi_role_t role;
    char            name[256];
    int32_t         child_count;      /* -1 means "not cached" */
    int32_t         index_in_parent;
    bool            has_name;
} wb_atspi_node_t;

void wb_atspi_node_init(wb_atspi_node_t *n);

/* A child count of -1 means "not cached"; zero means "empty". */
bool wb_atspi_has_children(const wb_atspi_node_t *n);

/* Is this node worth announcing?  An invalid role with no name is a container
 * a screen reader cannot describe. */
bool wb_atspi_is_describable(const wb_atspi_node_t *n);

#endif /* WUBU_ATSPI_H */
