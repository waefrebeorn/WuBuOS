/*
 * wubuglott_network.h -- branched airway acoustics.
 *
 * Staggered-grid FDTD transmission-line network.
 *
 *   Nodes carry pressure p (dyn/cm^2) and a volume V = A*dx; branches carry
 *   volume velocity U (cm^3/s) between two nodes. Leapfrog in time: branches
 *   update U(n+1/2) from p(n), then nodes update p(n+1) from U(n+1/2).
 *
 *   Branch momentum: the state is Phi = L*U (proportional to particle
 *   velocity), so as a branch closes (L -> inf) its volume flow goes to zero
 *   without remembering stale momentum. The update solves the exact quadratic
 *       (L/T)*U' + R*U' + k*|U'|*U' = dp + Phi/T
 *   with Borda-Carnot expansion loss k = rho/2*(1/A_up - 1/A_down)^2 chosen
 *   by flow direction.
 *
 *   Node continuity (moving walls included):
 *     (V/rho*c^2)*dp/dt = Sum(U_in) - Sum(U_out) - U_wall - dV/dt
 *   Yielding walls: per-node mass-spring-damper per unit area, times wall
 *   area (perimeter*dx). Radiation: Flanagan parallel R-L load at open ends;
 *   output is d/dt of radiated flow.
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/airway/network.rs.
 * C11. Build allocates; step() and the control-rate setters do not.
 */
#ifndef WUBUGLOTT_NETWORK_H
#define WUBUGLOTT_NETWORK_H

#include <stddef.h>

#include "wubuglott_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WG_KIND_SERIES 0   /* inertance from adjacent half-sections */
#define WG_KIND_PORT   1   /* own duct (area, length) in series with halves */

typedef struct {
    double area;              /* duct area (cm^2) */
    double length;            /* duct length (cm) */
    double k_entrance;        /* kinetic loss, flow entering the duct */
    double k_exit;            /* kinetic loss, flow leaving the duct (jet) */
    double slit_width;        /* >0: viscous R = 12*mu*len*w^2/A^3 */
    int    include_half;      /* include adjacent half-section inertances */
    double extra_resistance;  /* constant series resistance (CGS ohms) */
} wg_port_spec;

#define WG_PORT_SPEC_DEFAULT { \
    .area = 0.0, .length = 0.0, .k_entrance = 0.4, .k_exit = 1.0, \
    .slit_width = 0.0, .include_half = 1, .extra_resistance = 0.0 \
}

typedef struct {
    const char *name;
    int         n;         /* sections (pressure nodes) */
    double      length;    /* cm; may change at control rate */
    int         uniform;   /* 1: uniform area, 0: profile */
    double      area;      /* uniform area (cm^2) */
    const double *profile; /* n areas when uniform == 0 */
    wg_wall_params wall;      /* yielding wall; stiffness <= 0 means rigid */
    double      perimeter; /* cross-section perimeter vs a circle of equal area (>=1) */
    double      viscous;   /* multiplier on viscous resistance */
    double      radiation; /* fraction of wall volume velocity radiated */
} wg_tube_spec;

typedef struct {
    int    index;
    int    n;
    int    first;        /* nodes are first..first+n */
    int    branch_base;  /* internal branch between node 0 and 1 */
} wg_tube;

static inline int wg_tube_node(wg_tube t, int i)      { return t.first + i; }
static inline int wg_tube_first(wg_tube t)            { return t.first; }
static inline int wg_tube_last(wg_tube t)             { return t.first + t.n - 1; }
static inline int wg_tube_branch(wg_tube t, int i)    { return t.branch_base + i; }

typedef struct {
    double gain;
    int    src, fwd, bwd;
    double band_hz;
    double q;
} wg_noise_spec;

typedef struct wg_network wg_network;

wg_network *wg_network_new(double fs);
void        wg_network_free(wg_network *net);

/* Build (before finalize). */
wg_tube wg_network_add_tube(wg_network *net, const wg_tube_spec *spec);
int     wg_network_connect(wg_network *net, int a, int b);
int     wg_network_add_port(wg_network *net, int a, int b, wg_port_spec spec);
int     wg_network_add_cavity(wg_network *net, const char *name, double volume, int has_wall, wg_wall_params wall);
int     wg_network_add_pressure_node(wg_network *net, const char *name);
int     wg_network_add_radiation(wg_network *net, int node);
void    wg_network_finalize(wg_network *net);

/* Control-rate setters (no allocation once warm). */
void    wg_network_set_tube_targets(wg_network *net, wg_tube t, const double *areas,
                                    double length, const double *min_areas);
void    wg_network_set_port_target(wg_network *net, int port, double area);
void    wg_network_set_noise_sources(wg_network *net, const wg_noise_spec *src, int n);
void    wg_network_set_noise_scale(wg_network *net, double scale);

/* Jump the geometry to its targets immediately (initialisation; no dV/dt
 * transient). */
void    wg_network_snap_to_targets(wg_network *net);

/* Prepare per-sample linear ramps toward the targets over `samples`
 * internal samples. Call once per control block. */
void    wg_network_begin_block(wg_network *net, int samples);

/* Inject volume velocity (cm^3/s) at a node for the next step. */
void    wg_network_inject(wg_network *net, int node, double u);

/* Advance one sample. Returns the radiated pressure output. */
double  wg_network_step(wg_network *net);

/* Immediate geometry/pressure setters (per-sample sources call these). */
void    wg_network_set_pressure(wg_network *net, int node, double p);
void    wg_network_set_port_area(wg_network *net, int port, double area);

/* Telemetry. */
int     wg_network_n_nodes(const wg_network *net);
double  wg_network_pressure(const wg_network *net, int node);
double  wg_network_node_pressure(const wg_network *net, int node);
double  wg_network_radiated_flow(const wg_network *net, int radiator);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_NETWORK_H */
