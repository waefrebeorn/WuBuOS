/*
 * wubu_mixgraph.c -- kernel-owned audio mixing graph routing.
 *
 * The audio mixing graph is how apps, streams, and effects route to the
 * output. PipeWire fixed 20 years of ALSA/PulseAudio/JACK chaos. "Runs on
 * everything" includes correct audio mixing + routing.
 *
 * Audio graph components:
 *   - PipeWire: the modern graph (sessions, nodes, links, ports), pw-cli
 *   - PulseAudio: legacy graph (sink/source, sink-input, modules)
 *   - JACK: pro-audio graph (ports, connections, real-time)
 *   - ALSA: the raw mixer (dmix software mixing)
 *   - session manager: WirePlumber (PipeWire), pulse (PA)
 *
 * WuBuOS owns this: detect the active audio graph + session manager, route
 * to the right graph driver, and expose the mixing topology.
 *
 * Research (Kevin-Bacon 7-hop on the audio-graph frontier):
 *   - PipeWire + WirePlumber: the modern graph + session manager
 *   - PulseAudio: legacy; JACK: pro-audio real-time graph
 *   - ALSA dmix: software mixing fallback
 *   - graph routing: node/link/port topology (pw-dump, pactl)
 */
#include "wubu_mixgraph.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_pipewire = 0;
static int  g_pulse = 0;
static int  g_jack = 0;
static int  g_alsa = 0;
static int  g_wireplumber = 0;
static char g_graph_drv[24] = "";

/* ---- W1: probe the audio graph topology ---- */
void wubu_mixgraph_probe(void)
{
    g_pipewire = 0; g_pulse = 0; g_jack = 0; g_alsa = 0; g_wireplumber = 0;
    g_graph_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* PipeWire running? */
    if (access("/run/user", R_OK) == 0 && access("/usr/bin/pipewire", R_OK) == 0) {
        g_pipewire = 1;
        strcpy(g_graph_drv, "pipewire");
    }
    /* WirePlumber (PipeWire session manager)? */
    if (access("/usr/bin/wireplumber", R_OK) == 0) {
        g_wireplumber = 1;
    }
    /* PulseAudio? */
    if (access("/usr/bin/pulseaudio", R_OK) == 0 ||
        access("/run/pulse", R_OK) == 0) {
        g_pulse = 1;
        if (!g_graph_drv[0]) strcpy(g_graph_drv, "pulseaudio");
    }
    /* JACK? */
    if (access("/usr/bin/jackd", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/libjack.so.0", R_OK) == 0) {
        g_jack = 1;
        if (!g_graph_drv[0]) strcpy(g_graph_drv, "jack");
    }
    /* ALSA? */
    if (access("/proc/asound", R_OK) == 0 || access("/dev/snd", R_OK) == 0) {
        g_alsa = 1;
        if (!g_graph_drv[0]) strcpy(g_graph_drv, "alsa");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_mixgraph_pipewire(void){ return g_pipewire; }
int  wubu_mixgraph_pulse(void)   { return g_pulse; }
int  wubu_mixgraph_jack(void)    { return g_jack; }
int  wubu_mixgraph_alsa(void)    { return g_alsa; }
int  wubu_mixgraph_wireplumber(void){ return g_wireplumber; }
const char *wubu_mixgraph_driver(void){ return g_graph_drv[0] ? g_graph_drv : NULL; }

/* ---- W3: graph driver routing ---- */
const char *wubu_mixgraph_driver_for(const char *graph)
{
    if (!graph) return NULL;
    if (strstr(graph, "pipewire") || strstr(graph, "pw")) return "pipewire";
    if (strstr(graph, "wireplumber")) return "wireplumber";
    if (strstr(graph, "pulse"))  return "pulseaudio";
    if (strstr(graph, "jack"))   return "jack";
    if (strstr(graph, "alsa"))   return "alsa-dmix";
    return "alsa";
}

/* ---- W4: summary ---- */
int wubu_mixgraph_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "mixgraph[pw=%d pulse=%d jack=%d alsa=%d wplumber=%d drv=%s]",
        g_pipewire, g_pulse, g_jack, g_alsa, g_wireplumber,
        wubu_mixgraph_driver() ? wubu_mixgraph_driver() : "none");
}
