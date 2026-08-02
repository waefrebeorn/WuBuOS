/*
 * comfy.h -- Comfy: node-graph visual scripting for the AGI operator.
 *
 * Comfy is the human's plumbing workbench. The human wires nodes into a
 * dataflow graph; pressing Run executes the graph front-to-back, and each
 * node performs a REAL action (emit text, compute, launch an app via WuBuFX,
 * or hand a result to Bonzi). It is the tangible "make the AGI do things"
 * surface inside the WuBuOS GUI.
 *
 * C11, opaque struct, minimal includes. Nodes + edges live in a flat array;
 * rendering and hit-testing are from VBE primitives. No external graph libs.
 */
#ifndef WUBU_COMFY_H
#define WUBU_COMFY_H

#include <stdint.h>
#include "../gui/dosgui_wm.h"

/* Public API: launch the Comfy node-graph editor window. */
DosGuiWindow *comfy_launch(void);

/* Execute the current graph. Returns number of nodes that ran real work. */
int comfy_run_graph(void);

/* For testing: programmatically add a node / edge (no GUI needed). */
int  comfy_add_node(const char *type, const char *label, int x, int y);
int  comfy_add_edge(int from_node, int from_port, int to_node, int to_port);
int  comfy_node_count(void);
int  comfy_edge_count(void);

#endif /* WUBU_COMFY_H */
