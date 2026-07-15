/*
 * wubu_canvas_plugin.c -- WuBuOS canvas: plugin registration / run /
 *   unregister.
 *
 * Self-contained: depends only on wubu_canvas.h for the WubuCanvas /
 * WubuPlugin types and the public wubu_cv_plugin_* API. Plugins are stored
 * by value in the canvas's plugin array; running a plugin delegates to its
 * process_image callback. Minimal includes.
 */

#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int wubu_cv_plugin_register(WubuCanvas *cv, const WubuPlugin *plugin) {
    if (!cv || !plugin || cv->n_plugins >= WUBU_CV_MAX_PLUGINS) return -1;
    cv->plugins[cv->n_plugins] = *plugin;
    cv->plugins[cv->n_plugins].active = true;
    cv->n_plugins++;
    return cv->n_plugins - 1;
}

int wubu_cv_plugin_run(WubuCanvas *cv, int plugin_idx) {
    if (!cv || plugin_idx < 0 || plugin_idx >= cv->n_plugins) return -1;
    WubuPlugin *p = &cv->plugins[plugin_idx];
    if (!p->active) return -1;
    if (p->process_image)
        return p->process_image(cv, p->user_data);
    return 0;
}

void wubu_cv_plugin_unregister(WubuCanvas *cv, int plugin_idx) {
    if (!cv || plugin_idx < 0 || plugin_idx >= cv->n_plugins) return;
    WubuPlugin *p = &cv->plugins[plugin_idx];
    if (p->destroy) p->destroy(p->user_data);
    for (int i = plugin_idx; i < cv->n_plugins - 1; i++)
        cv->plugins[i] = cv->plugins[i + 1];
    cv->n_plugins--;
}
