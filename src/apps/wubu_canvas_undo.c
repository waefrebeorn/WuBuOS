/*
 * wubu_canvas_undo.c -- WuBuOS canvas: undo/redo history + snapshot hook.
 *
 * Self-contained: owns the undo/redo snapshot stacks as static globals and
 * exposes wubu_cv__undo_push() -- the single internal seam the drawing and
 * transform modules call before mutating layer pixels (declared in
 * wubu_canvas_internal.h). Depends only on wubu_canvas.h for the WubuCanvas
 * / WubuLayer types. Minimal includes.
 */

#include "wubu_canvas.h"
#include "wubu_canvas_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint32_t *pixels;
    int w, h;
} UndoSnapshot;

#define UNDO_MAX 50

static UndoSnapshot g_undo_stack[UNDO_MAX];
static int g_undo_sp = 0;
static UndoSnapshot g_redo_stack[UNDO_MAX];
static int g_redo_sp = 0;

void wubu_cv_undo(WubuCanvas *cv) {
    if (!cv || g_undo_sp <= 0 || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;

    /* Save current to redo */
    if (g_redo_sp >= UNDO_MAX) {
        if (g_redo_stack[0].pixels) free(g_redo_stack[0].pixels);
        memmove(&g_redo_stack[0], &g_redo_stack[1], (UNDO_MAX - 1) * sizeof(UndoSnapshot));
        g_redo_sp = UNDO_MAX - 1;
    }
    g_redo_stack[g_redo_sp].w = l->w;
    g_redo_stack[g_redo_sp].h = l->h;
    g_redo_stack[g_redo_sp].pixels = (uint32_t*)malloc((size_t)l->w * l->h * sizeof(uint32_t));
    if (g_redo_stack[g_redo_sp].pixels) {
        memcpy(g_redo_stack[g_redo_sp].pixels, l->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
        g_redo_sp++;
    }

    /* Restore from undo */
    g_undo_sp--;
    UndoSnapshot *snap = &g_undo_stack[g_undo_sp];
    if (snap->pixels && snap->w == l->w && snap->h == l->h) {
        memcpy(l->pixels, snap->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
    }
    free(snap->pixels);
    snap->pixels = NULL;
}

void wubu_cv_redo(WubuCanvas *cv) {
    if (!cv || g_redo_sp <= 0 || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;

    /* Save current to undo */
    if (g_undo_sp >= UNDO_MAX) {
        if (g_undo_stack[0].pixels) free(g_undo_stack[0].pixels);
        memmove(&g_undo_stack[0], &g_undo_stack[1], (UNDO_MAX - 1) * sizeof(UndoSnapshot));
        g_undo_sp = UNDO_MAX - 1;
    }
    g_undo_stack[g_undo_sp].w = l->w;
    g_undo_stack[g_undo_sp].h = l->h;
    g_undo_stack[g_undo_sp].pixels = (uint32_t*)malloc((size_t)l->w * l->h * sizeof(uint32_t));
    if (g_undo_stack[g_undo_sp].pixels) {
        memcpy(g_undo_stack[g_undo_sp].pixels, l->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
        g_undo_sp++;
    }

    /* Restore from redo */
    g_redo_sp--;
    UndoSnapshot *snap = &g_redo_stack[g_redo_sp];
    if (snap->pixels && snap->w == l->w && snap->h == l->h) {
        memcpy(l->pixels, snap->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
    }
    free(snap->pixels);
    snap->pixels = NULL;
}

/* Internal seam: record a pre-mutation snapshot of the active layer and clear
 * the redo stack. Called by wubu_canvas_draw.c / wubu_canvas_transform.c. */
void wubu_cv__undo_push(WubuCanvas *cv) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;

    if (g_undo_sp >= UNDO_MAX) {
        /* Shift stack down */
        if (g_undo_stack[0].pixels) free(g_undo_stack[0].pixels);
        memmove(&g_undo_stack[0], &g_undo_stack[1], (UNDO_MAX - 1) * sizeof(UndoSnapshot));
        g_undo_sp = UNDO_MAX - 1;
    }

    g_undo_stack[g_undo_sp].w = l->w;
    g_undo_stack[g_undo_sp].h = l->h;
    g_undo_stack[g_undo_sp].pixels = (uint32_t*)malloc((size_t)l->w * l->h * sizeof(uint32_t));
    if (g_undo_stack[g_undo_sp].pixels) {
        memcpy(g_undo_stack[g_undo_sp].pixels, l->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
        g_undo_sp++;
    }

    /* Clear redo stack on new action */
    while (g_redo_sp > 0) {
        g_redo_sp--;
        if (g_redo_stack[g_redo_sp].pixels) free(g_redo_stack[g_redo_sp].pixels);
    }
}
