/*
 * wubuglott_network.c -- branched airway acoustics.
 * See wubuglott_network.h. Slermed from chimera-vocal-synth's
 * airway/network.rs; the operation order is the original's.
 */
#include "wubuglott_network.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct wg_network {
    double fs, t, inv_t;

    /* tubes */
    wg_tube *tubes;
    int      n_tubes, cap_tubes;
    double  *tube_length;
    wg_tube_spec *tube_specs;
    int      finalized;

    /* nodes */
    int     n_nodes, cap_nodes;
    double *p;
    double *area, *area_target, *area_step;
    double *amin, *amin_target, *amin_step;
    int    *is_junction;
    double *inv_lsum, *inv_amin;
    int    *nbr_start, *nbr_list;
    double *spill;
    double *vol_src1, *vol_src2;
    double  vol_lp_coef;
    int     enable_volume_source;
    int     enable_junction_floor;
    int     energy_safe;
    int     symmetric_kinetic;
    double *dx, *dx_target, *dx_step;
    double *vol, *inv_a;
    double *half_l, *half_r, *sqrt_a;
    double *flow_sum;
    double *ext_flow;
    int    *fixed;
    double *wall_m, *wall_r, *wall_k, *wall_perim, *wall_rad;
    int    *has_wall;
    double *wall_y, *wall_v, *wall_u;
    int    *node_tube;
    double *visc_f;
    int    *rad_of;
    double  squeeze_area;
    double  min_node_area;

    /* branches */
    int     n_branches, cap_branches;
    int    *b_a, *b_b, *b_kind;
    wg_port_spec *port_specs;
    double *u, *mom, *l_prev, *psrc;
    double *port_area, *port_area_target, *port_area_step;
    double *port_len, *port_kin, *port_kex, *port_slit, *port_extra_r;
    int    *port_half;
    double *branch_area;
    int    *pending_fixed;
    int     n_pending, cap_pending;

    /* radiation */
    int    *rad_nodes;
    int     n_rad, cap_rad;
    double *rad_ul, *rad_l_prev, *rad_u;
    double  rad_prev_total;

    /* sources */
    wg_noise_spec *noise;
    int     n_noise, cap_noise;
    wg_rng  rng;
    double  closed_area;
    double  viscous_scale;
    double  viscous_omega;
    double  noise_scale;
    double  out;
};

/* ---- allocation helpers ---------------------------------------------- */
#define GROW(type, ptr, need, cap) do { \
    if ((need) > (cap)) { \
        (cap) = (cap) ? (cap) * 2 : 16; \
        while ((cap) < (need)) (cap) *= 2; \
        (ptr) = (type *)realloc((ptr), (size_t)(cap) * sizeof(type)); \
    } \
} while (0)

wg_network *wg_network_new(double fs)
{
    wg_network *net = calloc(1, sizeof(*net));
    if (!net) return NULL;
    net->fs = fs;
    net->t = 1.0 / fs;
    net->inv_t = fs;
    net->vol_lp_coef = 1.0 - exp(-2.0 * WG_PI * 120.0 / fs);
    net->enable_volume_source = 1;
    net->enable_junction_floor = 1;
    net->energy_safe = 1;
    net->symmetric_kinetic = 0;
    net->squeeze_area = 0.01;
    net->min_node_area = 1e-4;
    net->closed_area = 1e-4;
    net->viscous_scale = 1.0;
    net->viscous_omega = 2.0 * WG_PI * 20000.0;
    net->noise_scale = 1.0;
    wg_rng_init(&net->rng, 0x9e3779b9u);
    return net;
}

void wg_network_free(wg_network *net)
{
    if (!net) return;
    free(net->tubes); free(net->tube_length); free(net->tube_specs);
    free(net->p); free(net->area); free(net->area_target); free(net->area_step);
    free(net->amin); free(net->amin_target); free(net->amin_step);
    free(net->is_junction); free(net->inv_lsum); free(net->inv_amin);
    free(net->nbr_start); free(net->nbr_list); free(net->spill);
    free(net->vol_src1); free(net->vol_src2);
    free(net->dx); free(net->dx_target); free(net->dx_step);
    free(net->vol); free(net->inv_a);
    free(net->half_l); free(net->half_r); free(net->sqrt_a);
    free(net->flow_sum); free(net->ext_flow); free(net->fixed);
    free(net->wall_m); free(net->wall_r); free(net->wall_k);
    free(net->wall_perim); free(net->wall_rad); free(net->has_wall);
    free(net->wall_y); free(net->wall_v); free(net->wall_u);
    free(net->node_tube); free(net->visc_f); free(net->rad_of);
    free(net->b_a); free(net->b_b); free(net->b_kind); free(net->port_specs);
    free(net->u); free(net->mom); free(net->l_prev); free(net->psrc);
    free(net->port_area); free(net->port_area_target); free(net->port_area_step);
    free(net->port_len); free(net->port_kin); free(net->port_kex);
    free(net->port_slit); free(net->port_extra_r); free(net->port_half);
    free(net->branch_area); free(net->pending_fixed);
    free(net->rad_nodes); free(net->rad_ul); free(net->rad_l_prev); free(net->rad_u);
    free(net->noise);
    free(net);
}

static int push_branch(wg_network *net, int a, int b, int kind, wg_port_spec spec, int has_spec)
{
    if (net->n_branches + 1 > net->cap_branches) {
        int cap = net->cap_branches ? net->cap_branches * 2 : 16;
        while (cap < net->n_branches + 1) cap *= 2;
        net->b_a = realloc(net->b_a, (size_t)cap * sizeof(int));
        net->b_b = realloc(net->b_b, (size_t)cap * sizeof(int));
        net->b_kind = realloc(net->b_kind, (size_t)cap * sizeof(int));
        net->port_specs = realloc(net->port_specs, (size_t)cap * sizeof(wg_port_spec));
        net->cap_branches = cap;
    }
    int j = net->n_branches++;
    net->b_a[j] = a;
    net->b_b[j] = b;
    net->b_kind[j] = kind;
    if (!has_spec) {
        static const wg_port_spec zero = {0};
        spec = zero;
    }
    net->port_specs[j] = spec;
    return j;
}

wg_tube wg_network_add_tube(wg_network *net, const wg_tube_spec *spec)
{
    int idx = net->n_tubes;
    if (idx + 1 > net->cap_tubes) {
        int cap = net->cap_tubes ? net->cap_tubes * 2 : 16;
        while (cap < idx + 1) cap *= 2;
        net->tubes = realloc(net->tubes, (size_t)cap * sizeof(wg_tube));
        net->tube_length = realloc(net->tube_length, (size_t)cap * sizeof(double));
        net->tube_specs = realloc(net->tube_specs, (size_t)cap * sizeof(wg_tube_spec));
        net->cap_tubes = cap;
    }

    wg_tube t;
    t.index = idx;
    t.n = spec->n;
    t.first = net->n_nodes;
    t.branch_base = net->n_branches;
    net->tubes[idx] = t;
    net->tube_length[idx] = spec->length;
    net->tube_specs[idx] = *spec;
    net->n_tubes++;
    net->n_nodes += spec->n;

    for (int i = 0; i + 1 < spec->n; i++)
        push_branch(net, wg_tube_node(t, i), wg_tube_node(t, i + 1),
                    WG_KIND_SERIES, (wg_port_spec){0}, 0);
    return t;
}

int wg_network_connect(wg_network *net, int a, int b)
{
    return push_branch(net, a, b, WG_KIND_SERIES, (wg_port_spec){0}, 0);
}

int wg_network_add_port(wg_network *net, int a, int b, wg_port_spec spec)
{
    return push_branch(net, a, b, WG_KIND_PORT, spec, 1);
}

int wg_network_add_cavity(wg_network *net, const char *name, double volume,
                          int has_wall, wg_wall_params wall)
{
    double len = cbrt(volume);
    wg_tube_spec s = {0};
    s.name = name;
    s.n = 1;
    s.length = len;
    s.uniform = 1;
    s.area = volume / len;
    s.wall = wall;
    s.perimeter = 1.2;
    s.viscous = 1.0;
    s.radiation = 0.0;
    /* stiffness <= 0 means rigid in this port */
    if (!has_wall) s.wall.stiffness = 0.0;
    wg_tube t = wg_network_add_tube(net, &s);
    return wg_tube_first(t);
}

int wg_network_add_pressure_node(wg_network *net, const char *name)
{
    wg_tube_spec s = {0};
    s.name = name;
    s.n = 1;
    s.length = 1.0;
    s.uniform = 1;
    s.area = 100.0;
    s.wall.stiffness = 0.0;
    s.perimeter = 1.0;
    s.viscous = 1.0;
    wg_tube t = wg_network_add_tube(net, &s);
    GROW(int, net->pending_fixed, net->n_pending + 1, net->cap_pending);
    net->pending_fixed[net->n_pending++] = wg_tube_first(t);
    return wg_tube_first(t);
}

int wg_network_add_radiation(wg_network *net, int node)
{
    GROW(int, net->rad_nodes, net->n_rad + 1, net->cap_rad);
    net->rad_nodes[net->n_rad] = node;
    return net->n_rad++;
}

static double *zf(double *p, int n) { (void)p; return calloc((size_t)n, sizeof(double)); }

void wg_network_finalize(wg_network *net)
{
    if (net->finalized) return;
    net->finalized = 1;

    int n = net->n_nodes;
    int b = net->n_branches;
    net->n_branches = b;

    net->p = zf(NULL, n);
    net->area = zf(NULL, n);
    net->area_target = zf(NULL, n);
    net->area_step = zf(NULL, n);
    net->amin = zf(NULL, n);
    net->amin_target = zf(NULL, n);
    net->amin_step = zf(NULL, n);
    net->is_junction = calloc((size_t)n, sizeof(int));
    net->inv_lsum = zf(NULL, n);
    net->inv_amin = zf(NULL, n);
    net->dx = zf(NULL, n);
    net->dx_target = zf(NULL, n);
    net->dx_step = zf(NULL, n);
    net->vol = zf(NULL, n);
    net->inv_a = zf(NULL, n);
    net->half_l = zf(NULL, n);
    net->half_r = zf(NULL, n);
    net->sqrt_a = zf(NULL, n);
    net->flow_sum = zf(NULL, n);
    net->ext_flow = zf(NULL, n);
    net->fixed = calloc((size_t)n, sizeof(int));
    net->wall_m = zf(NULL, n);
    net->wall_r = zf(NULL, n);
    net->wall_k = zf(NULL, n);
    net->wall_perim = zf(NULL, n);
    net->wall_rad = zf(NULL, n);
    net->has_wall = calloc((size_t)n, sizeof(int));
    net->wall_y = zf(NULL, n);
    net->wall_v = zf(NULL, n);
    net->wall_u = zf(NULL, n);
    net->node_tube = calloc((size_t)n, sizeof(int));
    net->visc_f = zf(NULL, n);
    net->rad_of = malloc((size_t)n * sizeof(int));
    for (int k = 0; k < n; k++)
        net->rad_of[k] = -1;          /* the original: vec![-1; n] */
    net->spill = zf(NULL, n);
    net->vol_src1 = zf(NULL, n);
    net->vol_src2 = zf(NULL, n);

    for (int ti = 0; ti < net->n_tubes; ti++) {
        wg_tube t = net->tubes[ti];
        const wg_tube_spec *spec = &net->tube_specs[ti];
        double dx = spec->length / (double)spec->n;
        for (int i = 0; i < t.n; i++) {
            int k = wg_tube_node(t, i);
            double a = spec->uniform ? spec->area : spec->profile[i];
            net->area[k] = a;
            net->area_target[k] = a;
            net->amin[k] = a;
            net->amin_target[k] = a;
            net->dx[k] = dx;
            net->dx_target[k] = dx;
            net->node_tube[k] = t.index;
            net->visc_f[k] = spec->viscous;
            if (spec->wall.stiffness > 0.0) {
                net->has_wall[k] = 1;
                net->wall_m[k] = spec->wall.mass;
                net->wall_r[k] = spec->wall.resistance;
                net->wall_k[k] = spec->wall.stiffness;
            }
            net->wall_perim[k] = spec->perimeter * 2.0 * sqrt(WG_PI);
            net->wall_rad[k] = spec->radiation;
        }
    }
    for (int i = 0; i < net->n_pending; i++)
        net->fixed[net->pending_fixed[i]] = 1;

    net->u = zf(NULL, b);
    net->mom = zf(NULL, b);
    net->l_prev = zf(NULL, b);
    net->psrc = zf(NULL, b);
    net->port_area = zf(NULL, b);
    net->port_area_target = zf(NULL, b);
    net->port_area_step = zf(NULL, b);
    net->port_len = zf(NULL, b);
    net->port_kin = zf(NULL, b);
    net->port_kex = zf(NULL, b);
    net->port_slit = zf(NULL, b);
    net->port_extra_r = zf(NULL, b);
    net->port_half = calloc((size_t)b, sizeof(int));
    net->branch_area = zf(NULL, b);

    for (int j = 0; j < b; j++) {
        if (net->b_kind[j] == WG_KIND_PORT) {
            wg_port_spec s = net->port_specs[j];
            net->port_area[j] = s.area;
            net->port_area_target[j] = s.area;
            net->port_len[j] = s.length;
            net->port_kin[j] = s.k_entrance;
            net->port_kex[j] = s.k_exit;
            net->port_slit[j] = s.slit_width;
            net->port_half[j] = s.include_half;
            net->port_extra_r[j] = s.extra_resistance;
        }
    }

    /* junction flags and the CSR neighbour list */
    int *degree = calloc((size_t)n, sizeof(int));
    for (int j = 0; j < b; j++) {
        degree[net->b_a[j]]++;
        degree[net->b_b[j]]++;
    }
    for (int k = 0; k < n; k++)
        net->is_junction[k] = degree[k] >= 3;
    net->nbr_start = calloc((size_t)n + 1, sizeof(int));
    for (int k = 0; k < n; k++)
        net->nbr_start[k + 1] = net->nbr_start[k] + degree[k];
    net->nbr_list = calloc((size_t)net->nbr_start[n], sizeof(int));
    int *fill = malloc((size_t)n * sizeof(int));
    memcpy(fill, net->nbr_start, (size_t)n * sizeof(int));
    for (int j = 0; j < b; j++) {
        int a = net->b_a[j], bb = net->b_b[j];
        net->nbr_list[fill[a]++] = bb;
        net->nbr_list[fill[bb]++] = a;
    }
    free(fill);
    free(degree);

    /* radiation */
    net->rad_ul = zf(NULL, net->n_rad);
    net->rad_l_prev = zf(NULL, net->n_rad);
    net->rad_u = zf(NULL, net->n_rad);
    for (int i = 0; i < net->n_rad; i++)
        net->rad_of[net->rad_nodes[i]] = i;
}

/* ---- control-rate setters -------------------------------------------- */

void wg_network_set_tube_targets(wg_network *net, wg_tube t, const double *areas,
                                 double length, const double *min_areas)
{
    for (int i = 0; i < t.n; i++) {
        int k = wg_tube_node(t, i);
        net->area_target[k] = areas[i];
        if (min_areas)
            net->amin_target[k] = min_areas[i];
    }
    if (length > 0.0)
        net->tube_length[t.index] = length;
}

void wg_network_set_port_target(wg_network *net, int port, double area)
{
    net->port_area_target[port] = area;
}

void wg_network_set_noise_sources(wg_network *net, const wg_noise_spec *src, int n)
{
    if (n > net->cap_noise) {
        net->cap_noise = n;
        net->noise = realloc(net->noise, (size_t)n * sizeof(wg_noise_spec));
    }
    memcpy(net->noise, src, (size_t)n * sizeof(wg_noise_spec));
    net->n_noise = n;
}

void wg_network_set_noise_scale(wg_network *net, double scale)
{
    net->noise_scale = scale;
}

void wg_network_snap_to_targets(wg_network *net)
{
    for (int k = 0; k < net->n_nodes; k++) {
        net->area[k] = net->area_target[k];
        net->amin[k] = net->amin_target[k];
        net->dx[k] = net->dx_target[k];
        net->vol[k] = wg_js_max(net->area[k], net->min_node_area) * net->dx[k];
        net->inv_a[k] = 1.0 / wg_js_max(net->area[k], 1e-6);
        net->area_step[k] = 0.0;
        net->amin_step[k] = 0.0;
        net->dx_step[k] = 0.0;
    }
    for (int j = 0; j < net->n_branches; j++) {
        if (net->b_kind[j] == WG_KIND_PORT) {
            net->port_area[j] = net->port_area_target[j];
            net->port_area_step[j] = 0.0;
        }
    }
}

void wg_network_begin_block(wg_network *net, int samples)
{
    double inv = 1.0 / wg_js_max(1.0, (double)samples);
    for (int k = 0; k < net->n_nodes; k++) {
        net->area_step[k] = (net->area_target[k] - net->area[k]) * inv;
        net->amin_step[k] = (net->amin_target[k] - net->amin[k]) * inv;
        net->dx_step[k] = (net->dx_target[k] - net->dx[k]) * inv;
    }
    for (int j = 0; j < net->n_branches; j++) {
        if (net->b_kind[j] == WG_KIND_PORT)
            net->port_area_step[j] =
                (net->port_area_target[j] - net->port_area[j]) * inv;
    }
}

void wg_network_inject(wg_network *net, int node, double u)
{
    net->ext_flow[node] += u;
}

/* ---- noise ----------------------------------------------------------- */

static void compute_noise(wg_network *net)
{
    double scale = net->noise_scale * sqrt(96000.0 / net->fs);
    double sat = 12000.0 * 12000.0 - WG_RE_CRIT * WG_RE_CRIT;
    for (int i = 0; i < net->n_noise; i++) {
        wg_noise_spec *s = &net->noise[i];
        double a = net->branch_area[s->src];
        double u = net->u[s->src];
        double amp = 0.0;
        if (a > net->closed_area) {
            /* Re = v*d_h/nu with d_h = 2*sqrt(A/pi) -> Re = |U|*2/(nu*sqrt(pi*A)) */
            double re = (fabs(u) * 2.0) / (WG_NU * sqrt(WG_PI * a));
            if (re > WG_RE_CRIT) {
                double x = re * re - WG_RE_CRIT * WG_RE_CRIT;
                amp = (s->gain * scale * x) / (1.0 + x / sat);
            }
        }
        net->psrc[s->src] += amp * wg_rng_bipolar(&net->rng);
    }
}

/* ---- the step -------------------------------------------------------- */

double wg_network_step(wg_network *net)
{
    int n = net->n_nodes;
    int nb = net->n_branches;
    double t = net->t;
    double inv_t = net->inv_t;
    double min_a = net->min_node_area;

    /* 1) geometry ramps + moving-wall volume source */
    double sq = net->squeeze_area;
    int vol_on = net->enable_volume_source;
    if (vol_on)
        memset(net->spill, 0, (size_t)n * sizeof(double));

    for (int k = 0; k < n; k++) {
        double a = net->area[k] + net->area_step[k];
        if (a < 0.0) a = 0.0;
        net->area[k] = a;

        double am = net->amin[k] + net->amin_step[k];
        if (am < 0.0) am = 0.0;
        double amk = wg_js_min(am, a);
        net->amin[k] = amk;

        net->dx[k] += net->dx_step[k];
        double d = net->dx[k];

        double ia = 1.0 / (a > 1e-6 ? a : 1e-6);
        net->inv_a[k] = ia;
        net->inv_amin[k] = 1.0 / (amk > 1e-6 ? amk : 1e-6);
        double sa = sqrt(a);
        net->sqrt_a[k] = sa;
        net->half_l[k] = d * ia;
        net->half_r[k] = net->wall_perim[k] * d * sa * ia * ia * net->visc_f[k];

        double v = (a > min_a ? a : min_a) * d;
        if (vol_on) {
            double raw = -(v - net->vol[k]) * inv_t;
            double ae = wg_js_min(net->amin[k], a);
            if (raw != 0.0 && ae < sq) {
                double tt = ae / sq;
                double keep = tt * tt * (3.0 - 2.0 * tt);
                net->spill[k] += raw * keep;
                double sp = raw * (1.0 - keep);
                int n_open = 0;
                for (int e = net->nbr_start[k]; e < net->nbr_start[k + 1]; e++) {
                    int m = net->nbr_list[e];
                    if (wg_js_min(net->area[m], net->amin[m]) >= sq
                        || net->amin_target[m] >= sq)
                        n_open++;
                }
                if (n_open > 0) {
                    double share = sp / (double)n_open;
                    for (int e = net->nbr_start[k]; e < net->nbr_start[k + 1]; e++) {
                        int m = net->nbr_list[e];
                        if (wg_js_min(net->area[m], net->amin[m]) >= sq
                            || net->amin_target[m] >= sq)
                            net->spill[m] += share;
                    }
                }
            } else {
                net->spill[k] += raw;
            }
        }
        net->vol[k] = v;
        net->inv_lsum[k] = 0.0;
        net->flow_sum[k] = net->ext_flow[k];
        net->ext_flow[k] = 0.0;
    }
    if (vol_on) {
        double c = net->vol_lp_coef;
        for (int k = 0; k < n; k++) {
            net->vol_src1[k] += c * (net->spill[k] - net->vol_src1[k]);
            double s1 = net->vol_src1[k];
            net->vol_src2[k] += c * (s1 - net->vol_src2[k]);
            net->flow_sum[k] += net->vol_src2[k];
        }
    }

    /* 2) turbulence noise -> series pressure sources */
    memset(net->psrc, 0, (size_t)nb * sizeof(double));
    if (net->n_noise > 0)
        compute_noise(net);

    /* 3) branches */
    double visc = sqrt((WG_RHO * WG_MU * net->viscous_omega) / 2.0) * net->viscous_scale;
    double closed = net->closed_area;
    double half_rho = 0.5 * WG_RHO;

    for (int j = 0; j < nb; j++) {
        int a = net->b_a[j];
        int b = net->b_b[j];
        double l, r, kf, kb, a_eff;

        if (net->b_kind[j] == WG_KIND_SERIES) {
            double aa = net->area[a], ab = net->area[b];
            double ma = net->amin[a], mb = net->amin[b];
            a_eff = wg_js_min(ma, mb);
            if (wg_js_min(aa, ab) < closed || a_eff < closed) {
                net->u[j] = 0.0;
                net->mom[j] = 0.0;
                net->branch_area[j] = a_eff;
                continue;
            }
            double ia = net->inv_a[a], ib = net->inv_a[b];
            l = half_rho * (net->half_l[a] + net->half_l[b]);
            r = 0.5 * visc * (net->half_r[a] + net->half_r[b]);
            double ima = net->inv_amin[a], imb = net->inv_amin[b];
            double ef = ima - imb;   /* a->b */
            double eb = imb - ia;    /* b->a */
            kf = ef > 0.0 ? half_rho * ef * ef : 0.0;
            kb = eb > 0.0 ? half_rho * eb * eb : 0.0;
            double cf = imb - ia;
            double cb = ima - ib;
            if (cf > 0.0) kf += 0.1 * half_rho * cf * cf;
            if (cb > 0.0) kb += 0.1 * half_rho * cb * cb;
        } else {
            net->port_area[j] += net->port_area_step[j];
            double ap = net->port_area[j];
            if (ap < 0.0) { net->port_area[j] = 0.0; ap = 0.0; }
            a_eff = ap;
            if (ap < closed) {
                net->u[j] = 0.0;
                net->mom[j] = 0.0;
                net->branch_area[j] = ap;
                continue;
            }
            double ip = 1.0 / ap;
            double len = net->port_len[j];
            double ll = WG_RHO * len * ip;
            if (net->port_half[j])
                ll += half_rho * (net->half_l[a] + net->half_l[b]);
            l = ll;
            double w = net->port_slit[j];
            double rr = (w > 0.0)
                ? (12.0 * WG_MU * len * w * w) * ip * ip * ip
                : visc * 2.0 * sqrt(WG_PI) * len * sqrt(ap) * ip * ip;
            rr += net->port_extra_r[j];
            r = rr;
            double ex_f = ip - net->inv_a[b];
            double ex_b = ip - net->inv_a[a];
            double kin = net->port_kin[j] * half_rho * ip * ip;
            kf = kin + net->port_kex[j] * half_rho * (ex_f > 0.0 ? ex_f * ex_f : 0.0);
            kb = kin + net->port_kex[j] * half_rho * (ex_b > 0.0 ? ex_b * ex_b : 0.0);
        }

        net->branch_area[j] = a_eff;

        if (net->symmetric_kinetic) {
            double km = wg_js_min(kf, kb);
            kf = km; kb = km;
        }
        double lp = net->l_prev[j];
        double m0 = (net->energy_safe && l < lp) ? l * net->u[j] : net->mom[j];
        double c = net->p[a] - net->p[b] + net->psrc[j] + m0 * inv_t;
        double bq = l * inv_t + r;
        double u;
        if (c >= 0.0)
            u = (kf > 0.0) ? (2.0 * c) / (bq + sqrt(bq * bq + 4.0 * kf * c)) : c / bq;
        else
            u = (kb > 0.0) ? (2.0 * c) / (bq + sqrt(bq * bq - 4.0 * kb * c)) : c / bq;

        net->u[j] = u;
        net->mom[j] = l * u;
        net->l_prev[j] = l;
        net->flow_sum[a] -= u;
        net->flow_sum[b] += u;
        if (net->is_junction[a] || net->is_junction[b]) {
            double il = 1.0 / l;
            net->inv_lsum[a] += il;
            net->inv_lsum[b] += il;
        }
    }

    /* 4) walls (velocity at half step from p(n), displacement integrates) */
    double wall_rad_sum = 0.0;
    for (int k = 0; k < n; k++) {
        if (!net->has_wall[k]) continue;
        double m = net->wall_m[k];
        double r = net->wall_r[k];
        double v = ((m * inv_t - 0.5 * r) * net->wall_v[k]
                    + net->p[k] - net->wall_k[k] * net->wall_y[k])
                 / (m * inv_t + 0.5 * r);
        net->wall_v[k] = v;
        net->wall_y[k] += t * v;
        double sa = (net->area[k] > min_a) ? net->sqrt_a[k] : sqrt(min_a);
        double uw = net->wall_perim[k] * sa * net->dx[k] * v;
        net->wall_u[k] = uw;
        net->flow_sum[k] -= uw;
        wall_rad_sum += uw * net->wall_rad[k];
    }

    /* 5) nodes (radiation loads handled semi-implicitly) */
    double t2 = t * t;
    double rad_total = wall_rad_sum;
    double cl = net->closed_area;
    for (int k = 0; k < n; k++) {
        if (net->fixed[k]) continue;
        if (net->area[k] < cl || net->amin[k] < cl) {
            int rr = net->rad_of[k];
            if (rr >= 0) {
                net->p[k] *= 0.98;
            } else {
                double sum = 0.0;
                int cnt = 0;
                for (int e = net->nbr_start[k]; e < net->nbr_start[k + 1]; e++) {
                    int m = net->nbr_list[e];
                    if (net->area[m] >= cl && net->amin[m] >= cl) {
                        sum += net->p[m];
                        cnt++;
                    }
                }
                if (cnt > 0)
                    net->p[k] += 0.02 * (sum / (double)cnt - net->p[k]);
            }
            if (rr >= 0) {
                net->rad_ul[rr] *= 0.995;
                net->rad_u[rr] = net->rad_ul[rr];
                rad_total += net->rad_ul[rr];
            }
            continue;
        }
        double c = net->vol[k] / WG_RHO_C2;
        if (net->is_junction[k] && net->enable_junction_floor) {
            double c_min = (t2 * net->inv_lsum[k]) / 2.4;
            if (c < c_min) c = c_min;
        }
        int r = net->rad_of[k];
        if (r < 0) {
            net->p[k] += (t / c) * net->flow_sum[k];
        } else {
            double a = net->area[k] > 1e-4 ? net->area[k] : 1e-4;
            double lr = (8.0 * WG_RHO) / (3.0 * WG_PI * sqrt(WG_PI * a));
            double rr2 = (128.0 * WG_RHO * WG_C_SOUND) / (9.0 * WG_PI * WG_PI * a);
            double p_old = net->p[k];
            double lp = net->rad_l_prev[r];
            double ul_prev = net->rad_ul[r];
            if (lp > 0.0 && lr > lp)
                ul_prev *= lp / lr;
            net->rad_l_prev[r] = lr;
            double h = t / (2.0 * lr);
            double g = 0.5 / rr2;
            double p_new = (p_old * (c * inv_t - 0.5 * h - g) + net->flow_sum[k] - ul_prev)
                         / (c * inv_t + 0.5 * h + g);
            net->p[k] = p_new;
            double ul = ul_prev + h * (p_old + p_new);
            net->rad_ul[r] = ul;
            double ur = 0.5 * (ul_prev + ul) + g * (p_new + p_old);
            net->rad_u[r] = ur;
            rad_total += ur;
        }
    }

    double out = (rad_total - net->rad_prev_total) * inv_t;
    net->rad_prev_total = rad_total;
    net->out = out;
    return out;
}

void wg_network_set_pressure(wg_network *net, int node, double p)
{
    net->p[node] = p;
}

void wg_network_set_port_area(wg_network *net, int port, double area)
{
    if (area < 0.0) area = 0.0;
    net->port_area[port] = area;
    net->port_area_target[port] = area;
    net->port_area_step[port] = 0.0;
}

/* ---- telemetry -------------------------------------------------------- */

int     wg_network_n_nodes(const wg_network *net)          { return net->n_nodes; }
double  wg_network_pressure(const wg_network *net, int node) { return net->p[node]; }
double  wg_network_node_pressure(const wg_network *net, int node) { return net->p[node]; }
double  wg_network_radiated_flow(const wg_network *net, int r)    { return net->rad_u[r]; }
