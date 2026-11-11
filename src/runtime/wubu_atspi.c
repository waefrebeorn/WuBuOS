/*
 * wubu_atspi.c -- AT-SPI2: the accessibility object model.
 *
 * See wubu_atspi.h.
 */
#include "wubu_atspi.h"

#include <string.h>

/* The CANONICAL AT-SPI role names.  These are the strings a screen reader
 * receives, so they are data rather than a naming convention -- "push button"
 * and "check box" are two words, and there is both a BUTTON and a PUSH_BUTTON.
 * Writing this table from memory is exactly how a reader ends up calling a
 * checkbox a button. */
static const char *const g_role_names[WB_ATSPI_ROLE_COUNT] = {
    [WB_ATSPI_ROLE_INVALID]          = NULL,
    [WB_ATSPI_ROLE_ACCELERATOR_LABEL]= "accelerator label",
    [WB_ATSPI_ROLE_ALERT]            = "alert",
    [WB_ATSPI_ROLE_ANIMATION]        = "animation",
    [WB_ATSPI_ROLE_APPLICATION]      = "application",
    [WB_ATSPI_ROLE_BUTTON]           = "button",
    [WB_ATSPI_ROLE_CHECK_BOX]        = "check box",
    [WB_ATSPI_ROLE_COMBO_BOX]        = "combo box",
    [WB_ATSPI_ROLE_DIALOG]           = "dialog",
    [WB_ATSPI_ROLE_FRAME]            = "frame",
    [WB_ATSPI_ROLE_IMAGE]            = "image",
    [WB_ATSPI_ROLE_LABEL]            = "label",
    [WB_ATSPI_ROLE_LINK]             = "link",
    [WB_ATSPI_ROLE_LIST]             = "list",
    [WB_ATSPI_ROLE_LIST_ITEM]        = "list item",
    [WB_ATSPI_ROLE_MENU]             = "menu",
    [WB_ATSPI_ROLE_MENU_BAR]         = "menu bar",
    [WB_ATSPI_ROLE_MENU_ITEM]        = "menu item",
    [WB_ATSPI_ROLE_PAGE]             = "page",
    [WB_ATSPI_ROLE_PARAGRAPH]        = "paragraph",
    [WB_ATSPI_ROLE_PASSWORD_TEXT]    = "password text",
    [WB_ATSPI_ROLE_POPUP_MENU]       = "popup menu",
    [WB_ATSPI_ROLE_PROGRESS_BAR]     = "progress bar",
    [WB_ATSPI_ROLE_PUSH_BUTTON]      = "push button",
    [WB_ATSPI_ROLE_RADIO_BUTTON]     = "radio button",
    [WB_ATSPI_ROLE_SCROLL_BAR]       = "scroll bar",
    [WB_ATSPI_ROLE_SECTION]          = "section",
    [WB_ATSPI_ROLE_SLIDER]           = "slider",
    [WB_ATSPI_ROLE_SPIN_BUTTON]      = "spin button",
    [WB_ATSPI_ROLE_STATUS_BAR]       = "status bar",
    [WB_ATSPI_ROLE_TABLE]            = "table",
    [WB_ATSPI_ROLE_TABLE_CELL]       = "table cell",
    [WB_ATSPI_ROLE_TAB]              = "tab",
    [WB_ATSPI_ROLE_TAB_LIST]         = "tab list",
    [WB_ATSPI_ROLE_TEXT]             = "text",
    [WB_ATSPI_ROLE_TERMINAL]         = "terminal",
    [WB_ATSPI_ROLE_TOOL_BAR]         = "tool bar",
    [WB_ATSPI_ROLE_TREE]             = "tree",
    [WB_ATSPI_ROLE_TREE_ITEM]        = "tree item",
    [WB_ATSPI_ROLE_WINDOW]           = "window",
};

const char *wb_atspi_role_name(wb_atspi_role_t r)
{
    if (r < 0 || r >= WB_ATSPI_ROLE_COUNT) return NULL;
    return g_role_names[r];
}

wb_atspi_role_t wb_atspi_role_from_name(const char *name)
{
    if (!name) return WB_ATSPI_ROLE_INVALID;
    for (int i = 1; i < WB_ATSPI_ROLE_COUNT; i++) {
        if (g_role_names[i] && strcmp(g_role_names[i], name) == 0)
            return (wb_atspi_role_t)i;
    }
    return WB_ATSPI_ROLE_INVALID;
}

/* -- references and paths ----------------------------------------- */

bool wb_atspi_valid_path(const char *path)
{
    if (!path || path[0] != '/') return false;      /* must be absolute */
    if (path[1] == '\0') return true;               /* the root is valid */
    if (path[strlen(path) - 1] == '/') return false; /* no trailing slash */

    /* every component must be non-empty and made of [A-Za-z0-9_] */
    const char *p = path + 1;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/') p++;
        if (p == start) return false;               /* an empty component */
        for (const char *q = start; q < p; q++) {
            char c = *q;
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_'))
                return false;
        }
        if (*p == '/') p++;
    }
    return true;
}

bool wb_atspi_valid_bus_name(const char *name)
{
    if (!name || !*name) return false;

    /* a UNIQUE name: ":" then digits, then ".", then digits */
    if (name[0] == ':') {
        const char *p = name + 1;
        if (!(*p >= '0' && *p <= '9')) return false;
        while (*p >= '0' && *p <= '9') p++;
        if (*p != '.') return false;
        p++;
        if (!(*p >= '0' && *p <= '9')) return false;
        while (*p >= '0' && *p <= '9') p++;
        return *p == '\0';
    }

    /* a WELL-KNOWN name: two or more dot-separated elements, the first of
     * which must not start with a digit (a reverse-DNS prefix) */
    const char *p = name;
    int elements = 0;
    while (*p) {
        const char *start = p;
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
            return false;
        while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
               (*p >= '0' && *p <= '9') || *p == '_')
            p++;
        if (p == start) return false;
        elements++;
        if (*p == '.') { p++; if (!*p) return false; }
    }
    return elements >= 2;      /* "org.a11y.Bus", never a bare word */
}

/* -- nodes --------------------------------------------------------- */

void wb_atspi_node_init(wb_atspi_node_t *n)
{
    if (!n) return;
    memset(n, 0, sizeof(*n));
    n->role = WB_ATSPI_ROLE_INVALID;
    n->child_count = -1;       /* not cached */
}

bool wb_atspi_has_children(const wb_atspi_node_t *n)
{
    if (!n) return false;
    /* -1 means NOT CACHED, which is different from zero.  Treating -1 as empty
     * makes a screen reader believe a container has no children. */
    if (n->child_count < 0) return false;
    return n->child_count > 0;
}

bool wb_atspi_is_describable(const wb_atspi_node_t *n)
{
    if (!n) return false;
    /* An invalid role with no name is a container a screen reader cannot
     * describe -- it should be announced as a generic container, not skipped,
     * so the caller needs to know it is unhelpful. */
    if (n->role == WB_ATSPI_ROLE_INVALID && !n->has_name) return false;
    return true;
}
