/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "bear_nn.h"
#include "bear_arena.h"
#include <stdio.h>

/* ===================================================================
 * Checkpointing — binary serialization of policy network state
 *
 * File format (all integers little-endian, float = IEEE-754):
 *   magic         : uint32  'WUBR' (0x52425557)
 *   version       : uint32  1
 *   type          : int32   BearNetType
 *   num_layers    : int32
 *   obs_dim       : int32
 *   act_dim       : int32
 *   act_discrete  : int32
 *   hid_size      : int32
 *   has_logstd    : int32   (1 if net->logstd != NULL)
 *   logstd_fixed  : float
 *   For each layer:
 *     weight_rows : int32
 *     weight_cols : int32
 *     bias_len    : int32   (0 if no bias)
 *     weight_data : float[rows * cols]
 *     bias_data   : float[bias_len]
 *   If type == BEAR_NET_MINGRU:
 *     gru_count   : int32  (always 9)
 *     For each of 9 GRU params:
 *       rows      : int32
 *       cols      : int32
 *       data      : float[rows * cols]
 *   If has_logstd:
 *     act_dim floats
 *
 * Load validates magic, version, and struct dims against the target net
 * (the net must be pre-created via bear_policy_create_mlp/mingru).
 * =================================================================== */


#define BEAR_CKPT_MAGIC   0x52425557u  /* 'WUBR' little-endian */
#define BEAR_CKPT_VERSION 1u

static int bear_ckpt_write_u32(FILE* f, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_write_i32(FILE* f, int32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_write_f32(FILE* f, float v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_write_floats(FILE* f, const float* data, int n) {
    return fwrite(data, sizeof(float), (size_t)n, f) == (size_t)n ? 0 : -1;
}
static int bear_ckpt_read_u32(FILE* f, uint32_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_read_i32(FILE* f, int32_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_read_f32(FILE* f, float* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int bear_ckpt_read_floats(FILE* f, float* data, int n) {
    return fread(data, sizeof(float), (size_t)n, f) == (size_t)n ? 0 : -1;
}

int bear_checkpoint_save(const BearPolicyNet* net, const char* path) {
    if (!net || !path) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    int rc = -1;
    do {
        if (bear_ckpt_write_u32(f, BEAR_CKPT_MAGIC)) break;
        if (bear_ckpt_write_u32(f, BEAR_CKPT_VERSION)) break;
        if (bear_ckpt_write_i32(f, (int32_t)net->type)) break;
        if (bear_ckpt_write_i32(f, net->num_layers)) break;
        if (bear_ckpt_write_i32(f, net->obs_dim)) break;
        if (bear_ckpt_write_i32(f, net->act_dim)) break;
        if (bear_ckpt_write_i32(f, net->act_discrete)) break;
        if (bear_ckpt_write_i32(f, net->hid_size)) break;
        if (bear_ckpt_write_i32(f, net->logstd ? 1 : 0)) break;
        if (bear_ckpt_write_f32(f, net->logstd_fixed)) break;

        /* Serialize each layer: weight + bias */
        for (int i = 0; i < net->num_layers; ++i) {
            BearLayer* l = &net->layers[i];
            int w_rows = (int)l->param->weight.shape[0];
            int w_cols = (int)l->param->weight.shape[1];
            int b_len  = l->param->bias.data ?
                         (int)bear_tensor_numel(&l->param->bias) : 0;
            if (bear_ckpt_write_i32(f, w_rows)) { rc = -2; goto done; }
            if (bear_ckpt_write_i32(f, w_cols)) goto fail;
            if (bear_ckpt_write_i32(f, b_len))  goto fail;
            int w_n = w_rows * w_cols;
            if (bear_ckpt_write_floats(f, (float*)l->param->weight.data, w_n))
                goto fail;
            if (b_len > 0)
                if (bear_ckpt_write_floats(f, (float*)l->param->bias.data, b_len))
                    goto fail;
        }

        /* Serialize GRU params if MinGRU */
        if (net->gru) {
            BearParam* params[] = { &net->gru->Wz, &net->gru->Uz, &net->gru->bz,
                                    &net->gru->Wr, &net->gru->Ur, &net->gru->br,
                                    &net->gru->Wn, &net->gru->Un, &net->gru->bn };
            if (bear_ckpt_write_i32(f, 9)) goto fail;
            for (int i = 0; i < 9; ++i) {
                int rows = (int)params[i]->weight.shape[0];
                int cols = (int)params[i]->weight.shape[1];
                int n = rows * cols;
                if (bear_ckpt_write_i32(f, rows)) goto fail;
                if (bear_ckpt_write_i32(f, cols)) goto fail;
                if (bear_ckpt_write_floats(f, (float*)params[i]->weight.data, n))
                    goto fail;
            }
        }

        /* Serialize logstd if present */
        if (net->logstd)
            if (bear_ckpt_write_floats(f, net->logstd, net->act_dim)) goto fail;

        rc = 0;
        goto done;
    fail:
        rc = -2;
    } while (0);

done:
    fclose(f);
    return rc;
}

int bear_checkpoint_load(BearPolicyNet* net, const char* path) {
    if (!net || !path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    int rc = -1;
    uint32_t magic = 0, version = 0;
    int32_t type = 0, num_layers = 0, obs_dim = 0, act_dim = 0;
    int32_t act_discrete = 0, hid_size = 0, has_logstd = 0;
    float logstd_fixed = 0;

    do {
        if (bear_ckpt_read_u32(f, &magic) || magic != BEAR_CKPT_MAGIC) break;
        if (bear_ckpt_read_u32(f, &version) || version != BEAR_CKPT_VERSION) break;
        if (bear_ckpt_read_i32(f, &type)) break;
        if (bear_ckpt_read_i32(f, &num_layers)) break;
        if (bear_ckpt_read_i32(f, &obs_dim)) break;
        if (bear_ckpt_read_i32(f, &act_dim)) break;
        if (bear_ckpt_read_i32(f, &act_discrete)) break;
        if (bear_ckpt_read_i32(f, &hid_size)) break;
        if (bear_ckpt_read_i32(f, &has_logstd)) break;
        if (bear_ckpt_read_f32(f, &logstd_fixed)) break;

        /* Validate against the pre-created network */
        if (net->type != (BearNetType)type) { rc = -2; break; }
        if (net->num_layers != num_layers)   { rc = -3; break; }
        if (net->obs_dim != obs_dim)         { rc = -4; break; }
        if (net->act_dim != act_dim)          { rc = -5; break; }

        /* Read each layer's weight + bias */
        for (int i = 0; i < net->num_layers; ++i) {
            BearLayer* l = &net->layers[i];
            int32_t w_rows, w_cols, b_len;
            if (bear_ckpt_read_i32(f, &w_rows)) { rc = -6; goto done; }
            if (bear_ckpt_read_i32(f, &w_cols)) { rc = -6; goto done; }
            if (bear_ckpt_read_i32(f, &b_len))   { rc = -6; goto done; }
            /* Validate dims match */
            if ((int)l->param->weight.shape[0] != w_rows ||
                (int)l->param->weight.shape[1] != w_cols) { rc = -7; goto done; }
            int w_n = w_rows * w_cols;
            if (bear_ckpt_read_floats(f, (float*)l->param->weight.data, w_n)) {
                rc = -6; goto done;
            }
            if (b_len > 0) {
                if (!l->param->bias.data) { rc = -8; goto done; }
                if ((int)bear_tensor_numel(&l->param->bias) != b_len) {
                    rc = -7; goto done;
                }
                if (bear_ckpt_read_floats(f, (float*)l->param->bias.data, b_len)) {
                    rc = -6; goto done;
                }
            }
        }

        /* Read GRU params if MinGRU */
        if (net->gru) {
            int32_t gru_count = 0;
            if (bear_ckpt_read_i32(f, &gru_count) || gru_count != 9) { rc = -9; break; }
            BearParam* params[] = { &net->gru->Wz, &net->gru->Uz, &net->gru->bz,
                                    &net->gru->Wr, &net->gru->Ur, &net->gru->br,
                                    &net->gru->Wn, &net->gru->Un, &net->gru->bn };
            for (int i = 0; i < 9; ++i) {
                int32_t rows, cols;
                if (bear_ckpt_read_i32(f, &rows)) { rc = -6; goto done; }
                if (bear_ckpt_read_i32(f, &cols)) { rc = -6; goto done; }
                if ((int)params[i]->weight.shape[0] != rows ||
                    (int)params[i]->weight.shape[1] != cols) { rc = -7; goto done; }
                int n = rows * cols;
                if (bear_ckpt_read_floats(f, (float*)params[i]->weight.data, n)) {
                    rc = -6; goto done;
                }
            }
        }

        /* Read logstd if present */
        if (has_logstd) {
            if (!net->logstd) { rc = -10; break; }
            if (bear_ckpt_read_floats(f, net->logstd, net->act_dim)) { rc = -6; break; }
        }
        net->logstd_fixed = logstd_fixed;

        rc = 0;
    } while (0);

done:
    fclose(f);
    return rc;
}
