/*
 * bear_mujoco.c  --  MuJoCo Physics Backend Implementation
 *
 * Generates MJCF XML for N-pole cartpole (independent poles on cart),
 * loads into MuJoCo, steps dynamics via mj_step().
 * Pure C11 calling MuJoCo's C API.
 */
#include "bear_mujoco.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* MuJoCo C API */
#include <mujoco/mujoco.h>

/* Max poles and envs */
#define MJ_MAX_POLES 10
#define MJ_MAX_ENVS 256

/* Per-env MuJoCo data */
typedef struct {
    mjModel* model;           /* single model shared across envs */
    mjData** data;            /* per-env simulation state */
    int num_poles;
    int num_envs;
    int initialized;
    
    /* Joint ID mappings (cached after init) */
    int slider_id;            /* cart slider joint ID */
    int slider_qposadr;       /* qpos index for slider */
    int slider_dofadr;        /* qvel/dof index for slider */
    int hinge_ids[MJ_MAX_POLES];  /* pole hinge joint IDs */
    int hinge_qposadr[MJ_MAX_POLES];
    int hinge_dofadr[MJ_MAX_POLES];
    int motor_id;             /* actuator ID */
} MujocoBackend;

static MujocoBackend g_mjb = {0};

/* Build MJCF XML for N-pole CHAINED cartpole (each pole hinged at tip of previous).
 * Returns malloc'd string, caller must free.
 * Uses cartpole8 parameters: m_i = 0.30 * 0.82^i, l_i = 0.40 * 0.90^i
 */
static char* build_mjcf(int num_poles) {
    /* Estimate buffer size */
    size_t sz = 16384 + num_poles * 512;
    char* xml = (char*)malloc(sz);
    if (!xml) return NULL;
    
    /* Precompute lengths for nesting */
    float lens[MJ_MAX_POLES];
    float masses[MJ_MAX_POLES];
    for (int i = 0; i < num_poles; ++i) {
        masses[i] = 0.30f * powf(0.82f, i);
        lens[i] = 0.40f * powf(0.90f, i);
    }
    
    int pos = 0;
    pos += snprintf(xml + pos, sz - pos,
        "<mujoco model=\"bear_npole_chained_%d\">\n"
        "  <compiler angle=\"radian\" autolimits=\"true\"/>\n"
        "  <option gravity=\"0 -9.81 0\" timestep=\"0.005\"\n"
        "          iterations=\"100\" tolerance=\"1e-8\"/>\n"
        "  <default>\n"
        "    <joint limited=\"false\" damping=\"0\"/>\n"
        "    <geom condim=\"1\" friction=\"0 0 0\"/>\n"
        "  </default>\n"
        "  <worldbody>\n"
        "    <body name=\"cart\" pos=\"0 0 0\">\n"
        "      <inertial pos=\"0 0 0\" mass=\"1.0\" diaginertia=\"0.01 0.01 0.01\"/>\n"
        "      <joint name=\"slider\" type=\"slide\" axis=\"1 0 0\" limited=\"true\" range=\"-2.5 2.5\"/>\n"
        "      <geom name=\"cart_geom\" type=\"box\" size=\"0.3 0.1 0.05\" pos=\"0 0 0\" mass=\"1.0\"/>\n",
        num_poles);
    
    /* Nest poles: pole0 on cart, pole1 on tip of pole0, pole2 on tip of pole1, etc. */
    for (int i = 0; i < num_poles; ++i) {
        float mi = masses[i];
        float li = lens[i];
        float lc = li * 0.5f;
        float ixx = mi * li * li / 12.0f;  /* MOI about COM for uniform rod */
        
        /* Indent: each nested level adds 4 spaces */
        int indent = 4 + 4 * (i + 1);
        
        /* Open body at tip of previous pole (or cart for i=0) */
        pos += snprintf(xml + pos, sz - pos,
            "%*s<body name=\"pole%d\" pos=\"0 %.4f 0\">\n",
            indent, "", i, (i == 0) ? 0.0f : lens[i-1]);
        
        /* Inertial at COM (half-length from hinge in body frame) */
        pos += snprintf(xml + pos, sz - pos,
            "%*s  <inertial pos=\"0 %.4f 0\" mass=\"%.4f\" diaginertia=\"%.6f %.6f %.6f\"/>\n"
            "%*s  <joint name=\"hinge%d\" type=\"hinge\" axis=\"0 0 1\" limited=\"true\" range=\"-3.1416 3.1416\"/>\n"
            "%*s  <geom name=\"pole%d_geom\" type=\"capsule\" fromto=\"0 0 0 0 %.4f 0\" size=\"0.015\" mass=\"%.4f\"/>\n",
            indent, "", lc, mi, ixx, ixx, ixx,
            indent, "", i,
            indent, "", i, li, mi);
    }
    
    /* Close all pole bodies */
    for (int i = 0; i < num_poles; ++i) {
        pos += snprintf(xml + pos, sz - pos, "%*s</body>\n", 4 + 4 * (num_poles - i), "");
    }
    
    pos += snprintf(xml + pos, sz - pos,
        "    </body>\n"  /* close cart */
        "  </worldbody>\n"
        "  <actuator>\n"
        "    <motor name=\"cart_motor\" joint=\"slider\" gear=\"1\" ctrlrange=\"-80 80\" ctrllimited=\"true\"/>\n"
        "  </actuator>\n"
        "</mujoco>\n");
    
    return xml;
}

/* MJCF error callback */
static char mj_error_buf[1024];
static void mj_error_handler(const char* msg) {
    strncpy(mj_error_buf, msg, sizeof(mj_error_buf) - 1);
    mj_error_buf[sizeof(mj_error_buf) - 1] = '\0';
}

/* MJCF warning callback */
static void mj_warning_handler(const char* msg) {
    (void)msg;
    /* Suppress warnings */
}

int bear_mujoco_init(int num_poles, int num_envs) {
    if (g_mjb.initialized) return 0;
    
    if (num_poles < 1 || num_poles > MJ_MAX_POLES) return -1;
    if (num_envs < 1 || num_envs > MJ_MAX_ENVS) return -1;
    
    /* Install error handler */
    /* mju_user_error = mj_error_handler; */
    /* mju_user_warning = mj_warning_handler; */
    
    /* Build MJCF XML */
    char* xml = build_mjcf(num_poles);
    if (!xml) return -1;
    
    /* Write XML to temp file and load */
    char tmpname[256];
    snprintf(tmpname, sizeof(tmpname), "/tmp/bear_mj_%d_%d.xml", num_poles, rand());
    FILE* ftmp = fopen(tmpname, "w");
    if (!ftmp) { free(xml); return -1; }
    fputs(xml, ftmp);
    fclose(ftmp);
    free(xml);
    
    g_mjb.model = mj_loadXML(tmpname, NULL, NULL, 0);
    remove(tmpname);
    
    if (!g_mjb.model) {
        fprintf(stderr, "[MuJoCo] Failed to load model\\n");
        return -1;
    }
    
    /* Create per-env data */
    g_mjb.data = (mjData**)calloc(num_envs, sizeof(mjData*));
    if (!g_mjb.data) {
        mj_deleteModel(g_mjb.model);
        g_mjb.model = NULL;
        return -1;
    }
    
    for (int i = 0; i < num_envs; ++i) {
        printf("[DEBUG] Loop i=%d, num_envs=%d\n", i, num_envs);
        fflush(stdout);
        g_mjb.data[i] = mj_makeData(g_mjb.model);
        printf("[DEBUG] env %d: data=%p, qpos=%p, qvel=%p\n", i, g_mjb.data[i], g_mjb.data[i] ? g_mjb.data[i]->qpos : NULL, g_mjb.data[i] ? g_mjb.data[i]->qvel : NULL);
        fflush(stdout);
        if (!g_mjb.data[i]) {
            for (int j = 0; j < i; ++j) mj_deleteData(g_mjb.data[j]);
            free(g_mjb.data);
            mj_deleteModel(g_mjb.model);
            g_mjb.model = NULL;
            g_mjb.data = NULL;
            return -1;
        }
    }
    
    /* Cache joint ID mappings */
    g_mjb.slider_id = mj_name2id(g_mjb.model, mjOBJ_JOINT, "slider");
    g_mjb.motor_id = mj_name2id(g_mjb.model, mjOBJ_ACTUATOR, "cart_motor");
    
    /* Cache qpos/dof addresses for fast access */
    if (g_mjb.slider_id >= 0) {
        g_mjb.slider_qposadr = g_mjb.model->jnt_qposadr[g_mjb.slider_id];
        g_mjb.slider_dofadr = g_mjb.model->jnt_dofadr[g_mjb.slider_id];
    }
    
    char hname[64];
    for (int i = 0; i < num_poles; ++i) {
        snprintf(hname, sizeof(hname), "hinge%d", i);
        int jid = mj_name2id(g_mjb.model, mjOBJ_JOINT, hname);
        g_mjb.hinge_ids[i] = jid;
        if (jid >= 0) {
            g_mjb.hinge_qposadr[i] = g_mjb.model->jnt_qposadr[jid];
            g_mjb.hinge_dofadr[i] = g_mjb.model->jnt_dofadr[jid];
        }
    }
    
    if (g_mjb.slider_id < 0) {
        fprintf(stderr, "[MuJoCo] Could not find slider joint\n");
        bear_mujoco_shutdown();
        return -1;
    }
    
    g_mjb.num_poles = num_poles;
    g_mjb.num_envs = num_envs;
    g_mjb.initialized = 1;
    
    printf("[MuJoCo] Initialized %d-pole cartpole, %d envs\n", num_poles, num_envs);
    printf("[MuJoCo] nq=%d, nv=%d, nu=%d\n", 
           g_mjb.model->nq, g_mjb.model->nv, g_mjb.model->nu);
    fflush(stdout);
    
    return 0;
}

void bear_mujoco_step(int num_envs, int num_poles,
                       const float* cart_x, const float* cart_vx,
                       const float* theta_in, const float* omega_in,
                       const float* force, float dt,
                       float* cart_x_out, float* cart_vx_out,
                       float* theta_out, float* omega_out) {
    if (!g_mjb.initialized || !g_mjb.model || !g_mjb.data) return;
    
    mjModel* m = g_mjb.model;
    
    for (int e = 0; e < num_envs; ++e) {
        mjData* d = g_mjb.data[e];
        
        /* Set qpos using jnt_qposadr indices */
        d->qpos[g_mjb.slider_qposadr] = (mjtNum)cart_x[e];
        for (int p = 0; p < num_poles; ++p) {
            d->qpos[g_mjb.hinge_qposadr[p]] = (mjtNum)theta_in[e * num_poles + p];
        }
        
        /* Set qvel using jnt_dofadr indices */
        d->qvel[g_mjb.slider_dofadr] = (mjtNum)cart_vx[e];
        for (int p = 0; p < num_poles; ++p) {
            d->qvel[g_mjb.hinge_dofadr[p]] = (mjtNum)omega_in[e * num_poles + p];
        }
        
        /* Set control */
        if (g_mjb.motor_id >= 0) {
            d->ctrl[g_mjb.motor_id] = (mjtNum)force[e];
        }
        
        /* Step physics */
        mj_step(m, d);
        
        /* Read back state */
        cart_x_out[e] = (float)d->qpos[g_mjb.slider_qposadr];
        cart_vx_out[e] = (float)d->qvel[g_mjb.slider_dofadr];
        for (int p = 0; p < num_poles; ++p) {
            theta_out[e * num_poles + p] = (float)d->qpos[g_mjb.hinge_qposadr[p]];
            omega_out[e * num_poles + p] = (float)d->qvel[g_mjb.hinge_dofadr[p]];
        }
    }
    
    /* Update dt if different from model default */
    if (fabsf(dt - (float)g_mjb.model->opt.timestep) > 1e-8f) {
        /* mj_step uses model's timestep; we'd need to rebuild model for different dt.
         * For now, warn if dt differs significantly. */
        static int warned = 0;
        if (!warned) {
            fprintf(stderr, "[MuJoCo] Warning: model timestep=%.4f, requested dt=%.4f\n",
                    (float)g_mjb.model->opt.timestep, dt);
            warned = 1;
        }
    }
}

void bear_mujoco_reset_env(int env_id, int num_poles,
                            float cart_x, float cart_vx,
                            const float* theta, const float* omega) {
    if (!g_mjb.initialized || !g_mjb.data || env_id >= g_mjb.num_envs) return;
    
    mjData* d = g_mjb.data[env_id];
    mj_resetData(g_mjb.model, d);
    
    d->qpos[g_mjb.slider_qposadr] = (mjtNum)cart_x;
    d->qvel[g_mjb.slider_dofadr] = (mjtNum)cart_vx;
    
    for (int p = 0; p < num_poles; ++p) {
        d->qpos[g_mjb.hinge_qposadr[p]] = (mjtNum)theta[p];
        d->qvel[g_mjb.hinge_dofadr[p]] = (mjtNum)omega[p];
    }
}

void bear_mujoco_shutdown(void) {
    if (!g_mjb.initialized) return;
    
    if (g_mjb.data) {
        for (int i = 0; i < g_mjb.num_envs; ++i) {
            if (g_mjb.data[i]) mj_deleteData(g_mjb.data[i]);
        }
        free(g_mjb.data);
        g_mjb.data = NULL;
    }
    
    if (g_mjb.model) {
        mj_deleteModel(g_mjb.model);
        g_mjb.model = NULL;
    }
    
    g_mjb.initialized = 0;
    printf("[MuJoCo] Shutdown\n");
}

int bear_mujoco_available(void) {
    return g_mjb.initialized;
}
