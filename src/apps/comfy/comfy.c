/*
 * comfy.c -- Comfy: node-graph visual scripting for the AGI operator.
 *
 * Real node-graph engine: nodes have typed ports, edges connect out->in, and
 * comfy_run_graph() topologically evaluates the graph, calling genuine tools:
 *   - TEXT   : emits a literal string (source)
 *   - CALC   : evaluates a + b (real arithmetic)
 *   - APP    : launches an app via wubufx_app_launch(name)  (REAL plumbing)
 *   - SAY    : hands its input string to Bonzi (REAL plumbing)
 *   - PRINT  : echoes the value into the graph's output log
 *
 * Self-contained C11. No graph lib. The window supports drag-to-move nodes,
 * click-to-wire (port click then target port), and Run via Enter. The headless
 * harness drives the graph programmatically (comfy_add_node/edge + run).
 */
#include "comfy.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"   /* centralized chrome (ADR-001) */
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../framework/wubufx.h"
#include "../apps/bonzi/bonzi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* -- Graph model --------------------------------------------------- */
#define COMFY_MAX_NODES 32
#define COMFY_MAX_EDGES 64
#define COMFY_MAX_PORTS 4
#define COMFY_NTYPES 5

typedef enum {
    COMFY_TEXT = 0,
    COMFY_CALC,
    COMFY_APP,
    COMFY_SAY,
    COMFY_PRINT
} ComfyNodeType;

static const char *g_type_name[COMFY_NTYPES] = {
    "TEXT", "CALC", "APP", "SAY", "PRINT"
};

typedef struct {
    int  type;
    char label[32];
    int  x, y;
    int  w, h;
    /* values (string + numeric) carried by the node after eval */
    char sval[128];
    double fval;
    int   has_num;
    int   drag_off_x;   /* mouse-grab offset */
    int   drag_off_y;
} ComfyNode;

typedef struct {
    int from_node, from_port;
    int to_node, to_port;
} ComfyEdge;

typedef struct {
    ComfyNode  nodes[COMFY_MAX_NODES];
    int        n_nodes;
    ComfyEdge  edges[COMFY_MAX_EDGES];
    int        n_edges;
    char       outlog[8][128];
    int        out_n;
    /* interaction */
    int        drag_node;
    int        wire_from;     /* node id while wiring, -1 otherwise */
    int        wire_port;
    int        ran_ok;        /* last run result (nodes executed) */
} ComfyState;

static ComfyState *g_comfy = NULL;

/* -- Output log ---------------------------------------------------- */
static void comfy_out(ComfyState *c, const char *msg) {
    if (!c) return;
    snprintf(c->outlog[c->out_n % 8], sizeof(c->outlog[0]), "%s", msg);
    c->out_n++;
}

/* -- Port geometry (for hit-testing + drawing) -------------------- */
/* Node layout: title bar + 1 input port (left) + 1 output port (right).
   CALC has 2 inputs (a,b). TEXT/PRINT have 1 out/in respectively. */
static int comfy_n_inputs(ComfyNodeType t) {
    switch (t) {
        case COMFY_TEXT:  return 0;
        case COMFY_CALC:  return 2;
        case COMFY_APP:   return 1;
        case COMFY_SAY:   return 1;
        case COMFY_PRINT: return 1;
    }
    return 0;
}
static int comfy_n_outputs(ComfyNodeType t) {
    return (t == COMFY_PRINT) ? 0 : 1;
}

static void comfy_port_pos(ComfyNode *n, int port, int is_out, int *px, int *py) {
    int n_in = comfy_n_inputs((ComfyNodeType)n->type);
    (void)n_in;
    if (is_out) {
        *px = n->x + n->w;
        *py = n->y + 24;
    } else {
        *px = n->x;
        /* stack inputs vertically */
        *py = n->y + 24 + port * 18;
    }
}

/* -- Edge lookup: upstream value feeding (node, port) ------------- */
static ComfyNode *comfy_upstream(ComfyState *c, int node, int port, int *out_port) {
    for (int e = 0; e < c->n_edges; e++) {
        if (c->edges[e].to_node == node && c->edges[e].to_port == port) {
            if (out_port) *out_port = c->edges[e].from_port;
            return &c->nodes[c->edges[e].from_node];
        }
    }
    return NULL;
}

/* -- Node execution (the real work) -------------------------------- */
static void comfy_exec_node(ComfyState *c, ComfyNode *n) {
    switch ((ComfyNodeType)n->type) {
        case COMFY_TEXT:
            /* literal already in sval (set at creation) */
            n->has_num = 0;
            break;
        case COMFY_CALC: {
            ComfyNode *a = comfy_upstream(c, n - c->nodes, 0, NULL);
            ComfyNode *b = comfy_upstream(c, n - c->nodes, 1, NULL);
            double va = a ? a->fval : 0.0;
            double vb = b ? b->fval : 0.0;
            n->fval = va + vb;
            n->has_num = 1;
            snprintf(n->sval, sizeof(n->sval), "%.4g", n->fval);
            break;
        }
        case COMFY_APP: {
            /* launch the named app via WuBuFX (real plumbing) */
            DosGuiWindow *w = wubufx_app_launch(n->label);
            snprintf(n->sval, sizeof(n->sval), w ? "launched %s" : "no app %s", n->label);
            n->has_num = 0;
            c->ran_ok++;
            break;
        }
        case COMFY_SAY: {
            /* feed the upstream string to Bonzi (real plumbing) */
            ComfyNode *src = comfy_upstream(c, n - c->nodes, 0, NULL);
            const char *msg = src ? src->sval : n->label;
            bonzi_handle_line(msg);
            snprintf(n->sval, sizeof(n->sval), "said: %s", msg);
            n->has_num = 0;
            c->ran_ok++;
            break;
        }
        case COMFY_PRINT: {
            ComfyNode *src = comfy_upstream(c, n - c->nodes, 0, NULL);
            const char *v = src ? (src->has_num ? src->sval : src->sval) : "(none)";
            comfy_out(c, v);
            n->has_num = 0;
            break;
        }
    }
}

/* Topological-ish eval: repeat passes until stable (handles DAGs). */
int comfy_run_graph(void) {
    ComfyState *c = g_comfy;
    if (!c) return 0;
    c->ran_ok = 0;
    int n = c->n_nodes;
    char done[COMFY_MAX_NODES] = {0};
    int total = 0;
    for (int pass = 0; pass < n + 1; pass++) {
        int progressed = 0;
        for (int i = 0; i < n; i++) {
            if (done[i]) continue;
            ComfyNode *nd = &c->nodes[i];
            int ready = 1;
            int nin = comfy_n_inputs((ComfyNodeType)nd->type);
            for (int p = 0; p < nin; p++) {
                if (!comfy_upstream(c, i, p, NULL)) { ready = 0; break; }
            }
            /* TEXT has no inputs but is always ready */
            if (nd->type == COMFY_TEXT) ready = 1;
            if (ready) {
                comfy_exec_node(c, nd);
                done[i] = 1;
                progressed = 1;
                total++;
            }
        }
        if (!progressed) break;
    }
    comfy_out(c, "--- graph run complete ---");
    return c->ran_ok;
}

/* -- Programmatic graph build (for tests) ------------------------- */
int comfy_add_node(const char *type, const char *label, int x, int y) {
    ComfyState *c = g_comfy;
    if (!c || c->n_nodes >= COMFY_MAX_NODES) return -1;
    int t = -1;
    for (int i = 0; i < COMFY_NTYPES; i++)
        if (strcmp(type, g_type_name[i]) == 0) { t = i; break; }
    if (t < 0) return -1;
    ComfyNode *n = &c->nodes[c->n_nodes];
    memset(n, 0, sizeof(*n));
    n->type = t;
    snprintf(n->label, sizeof(n->label), "%s", label ? label : g_type_name[t]);
    n->x = x; n->y = y; n->w = 120; n->h = 48;
    /* TEXT carries its literal in sval */
    if (t == COMFY_TEXT && label) {
        snprintf(n->sval, sizeof(n->sval), "%s", label);
    }
    if (t == COMFY_CALC && label) {
        /* allow "a+b" parse fallback not needed; CALC pulls upstream */
    }
    return c->n_nodes++;
}

int comfy_add_edge(int from, int from_port, int to, int to_port) {
    ComfyState *c = g_comfy;
    if (!c || c->n_edges >= COMFY_MAX_EDGES) return -1;
    if (from < 0 || from >= c->n_nodes || to < 0 || to >= c->n_nodes) return -1;
    c->edges[c->n_edges].from_node = from;
    c->edges[c->n_edges].from_port = from_port;
    c->edges[c->n_edges].to_node = to;
    c->edges[c->n_edges].to_port = to_port;
    return c->n_edges++;
}

int comfy_node_count(void) { return g_comfy ? g_comfy->n_nodes : 0; }
int comfy_edge_count(void) { return g_comfy ? g_comfy->n_edges : 0; }

/* -- Drawing ------------------------------------------------------- */
static void comfy_draw_node(ComfyState *c, ComfyNode *n, int focused) {
    uint32_t body = focused ? 0x00D0E8FF : 0x00C0C0C0;
    uint32_t title = 0x008030A0;
    vbe_fill_rect_rounded(n->x, n->y, n->w, n->h, 6, body);
    vbe_rect_rounded(n->x, n->y, n->w, n->h, 6, 0x00808080);
    vbe_fill_rect(n->x, n->y, n->w, 16, title);
    char tbuf[48];
    snprintf(tbuf, sizeof(tbuf), "%s", g_type_name[n->type]);
    vbe_draw_text(n->x + 4, n->y + 4, tbuf, 0x00FFFFFF, 1);
    /* body text */
    const char *bt = (n->type == COMFY_TEXT) ? n->sval :
                     (n->type == COMFY_APP || n->type == COMFY_SAY) ? n->label :
                     (n->has_num) ? n->sval : "";
    vbe_draw_text(n->x + 4, n->y + 22, bt, 0x00000000, 1);

    /* ports */
    int px, py;
    if (comfy_n_outputs((ComfyNodeType)n->type)) {
        comfy_port_pos(n, 0, 1, &px, &py);
        vbe_fill_circle(px, py, 5, 0x0000A000);
    }
    int nin = comfy_n_inputs((ComfyNodeType)n->type);
    for (int p = 0; p < nin; p++) {
        comfy_port_pos(n, p, 0, &px, &py);
        vbe_fill_circle(px, py, 5, 0x00A00000);
    }
}

static void comfy_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    ComfyState *c = win ? (ComfyState*)win->user_data : NULL;
    if (!c) return;

    /* Centralized window chrome: draw title bar + border, get content rect.
     * ADR-001: apps draw ONLY within the chrome-provided content rect. */
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    int cx = content.x, cy = content.y, cw = content.w, ch = content.h;
    int body_y = cy + 2;
    int body_h = ch - 2;  /* ch already excludes chrome (ADR-001) */

    /* canvas */
    vbe_fill_rect(cx + 2, body_y + 2, cw - 4, body_h - 70, 0x00F5F5F5);
    vbe_rect(cx + 2, body_y + 2, cw - 4, body_h - 70, 0x00808080);

    /* edges */
    for (int e = 0; e < c->n_edges; e++) {
        int ax, ay, bx, by;
        ComfyNode *fn = &c->nodes[c->edges[e].from_node];
        ComfyNode *tn = &c->nodes[c->edges[e].to_node];
        comfy_port_pos(fn, c->edges[e].from_port, 1, &ax, &ay);
        comfy_port_pos(tn, c->edges[e].to_port, 0, &bx, &by);
        vbe_line(ax, ay, bx, by, 0x00000080);
    }

    /* nodes */
    for (int i = 0; i < c->n_nodes; i++)
        comfy_draw_node(c, &c->nodes[i], (i == c->drag_node));

    /* output log strip */
    int oy = body_y + body_h - 66;
    vbe_fill_rect(cx + 2, oy, cw - 4, 64, 0x00202020);
    for (int i = 0; i < 4 && i < c->out_n; i++) {
        int idx = (c->out_n - 1 - i);
        vbe_draw_text(cx + 6, oy + 4 + i * 14, c->outlog[idx % 8], 0x0000FF00, 1);
    }
    /* hint */
    vbe_draw_text(cx + 6, body_y + 4, "Comfy: drag nodes, click ports to wire, Enter=Run", 0x00800000, 1);
}

/* -- Input: drag nodes, wire ports, Enter=Run -------------------- */
static void comfy_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    ComfyState *c = win ? (ComfyState*)win->user_data : NULL;
    if (!c) return;
    if (key == '\r' || key == '\n') {
        comfy_run_graph();
    }
}

static void comfy_mouse(DosGuiWindow *win, int mx, int my, int btn, int kind) {
    ComfyState *c = win ? (ComfyState*)win->user_data : NULL;
    if (!c) return;
    /* convert to window-local */
    int lx = mx - win->x, ly = my - win->y;

    if (kind == 0 && btn == 1) {  /* press */
        /* port hit-test first */
        for (int i = 0; i < c->n_nodes; i++) {
            ComfyNode *n = &c->nodes[i];
            int px, py;
            if (comfy_n_outputs((ComfyNodeType)n->type)) {
                comfy_port_pos(n, 0, 1, &px, &py);
                if (abs(mx - px) < 8 && abs(my - py) < 8) {
                    c->wire_from = i; c->wire_port = 0; return;
                }
            }
            int nin = comfy_n_inputs((ComfyNodeType)n->type);
            for (int p = 0; p < nin; p++) {
                comfy_port_pos(n, p, 0, &px, &py);
                if (abs(mx - px) < 8 && abs(my - py) < 8) {
                    if (c->wire_from >= 0) {
                        comfy_add_edge(c->wire_from, c->wire_port, i, p);
                        c->wire_from = -1;
                    }
                    return;
                }
            }
        }
        /* else start dragging a node (hit-test topmost last) */
        for (int i = c->n_nodes - 1; i >= 0; i--) {
            ComfyNode *n = &c->nodes[i];
            if (mx >= n->x && mx <= n->x + n->w && my >= n->y && my <= n->y + n->h) {
                c->drag_node = i;
                n->drag_off_x = mx - n->x;
                n->drag_off_y = my - n->y;
                return;
            }
        }
    } else if (kind == 1 && btn == 1) {  /* release */
        c->drag_node = -1;
        if (c->wire_from >= 0) c->wire_from = -1;  /* cancel wire if released on empty */
    } else if (kind == 2 && btn == 1) {  /* drag */
        if (c->drag_node >= 0) {
            ComfyNode *n = &c->nodes[c->drag_node];
            n->x = mx - n->drag_off_x;
            n->y = my - n->drag_off_y;
        }
    }
}

/* -- Launch -------------------------------------------------------- */
DosGuiWindow *comfy_launch(void) {
    DosGuiWindow *win = dosgui_wm_create(300, 60, 420, 420, "Comfy - Node Graph");
    if (win) {
        ComfyState *c = calloc(1, sizeof(ComfyState));
        g_comfy = c;
        win->user_data = c;
        win->on_draw = comfy_draw;
        win->on_key  = comfy_key;
        win->on_mouse = comfy_mouse;
        if (c) {
            comfy_out(c, "Comfy ready. Build a graph and press Enter to Run.");
        }
    }
    return win;
}
