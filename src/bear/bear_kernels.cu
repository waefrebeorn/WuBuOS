/*
 * bear_kernels.cu — BearRL CUDA Kernels
 *
 * CUDA implementations of core BearRL operations:
 * - MatMul (GEMM) with shared memory tiling
 * - Element-wise operations (add, relu, sigmoid, softmax)
 * - MinGRU recurrent step
 * - N-pole cartpole physics step
 * - Adam optimizer step
 * - PPO loss and backward operations
 */

#include "bear_cuda.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_env.h"
#include "bear_opt.h"
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <math.h>
#include <stdio.h>

/* ══════════════════════════════════════════════════════════════════
 * Utility Kernels
 * ══════════════════════════════════════════════════════════════════ */

__inline__ __device__ float warp_reduce_sum(float val) {
    for (int offset = 16; offset > 0; offset >>= 1)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;
}

__inline__ __device__ float warp_reduce_max(float val) {
    for (int offset = 16; offset > 0; offset >>= 1)
        val = fmaxf(val, __shfl_down_sync(0xffffffff, val, offset));
    return val;
}

/* ══════════════════════════════════════════════════════════════════
 * MatMul Kernel: C = A @ B  (A: MxK, B: KxN, C: MxN)
 * ══════════════════════════════════════════════════════════════════ */

#define MATMUL_TILE 16

__global__ void matmul_kernel(const float* __restrict__ A,
                               const float* __restrict__ B,
                               float* __restrict__ C,
                               int M, int K, int N) {
    __shared__ float As[MATMUL_TILE][MATMUL_TILE];
    __shared__ float Bs[MATMUL_TILE][MATMUL_TILE];

    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    float sum = 0.0f;

    for (int t = 0; t < (K + MATMUL_TILE - 1) / MATMUL_TILE; ++t) {
        int tK = t * MATMUL_TILE;

        // Load tile of A
        if (row < M && tK + threadIdx.x < K)
            As[threadIdx.y][threadIdx.x] = A[row * K + tK + threadIdx.x];
        else
            As[threadIdx.y][threadIdx.x] = 0.0f;

        // Load tile of B
        if (tK + threadIdx.y < K && col < N)
            Bs[threadIdx.y][threadIdx.x] = B[(tK + threadIdx.y) * N + col];
        else
            Bs[threadIdx.y][threadIdx.x] = 0.0f;

        __syncthreads();

        for (int k = 0; k < MATMUL_TILE; ++k) {
            sum += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        }

        __syncthreads();
    }

    if (row < M && col < N)
        C[row * N + col] = sum;
}

/* ══════════════════════════════════════════════════════════════════
 * Element-wise Kernels
 * ══════════════════════════════════════════════════════════════════ */

__global__ void add_kernel(const float* __restrict__ A,
                           const float* __restrict__ B,
                           float* __restrict__ C,
                           int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) C[idx] = A[idx] + B[idx];
}

__global__ void relu_kernel(const float* __restrict__ A,
                            float* __restrict__ C,
                            int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) C[idx] = fmaxf(0.0f, A[idx]);
}

__global__ void sigmoid_kernel(const float* __restrict__ A,
                                float* __restrict__ C,
                                int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) C[idx] = 1.0f / (1.0f + expf(-A[idx]));
}

__global__ void softmax_kernel(const float* __restrict__ A,
                                float* __restrict__ C,
                                int n) {
    extern __shared__ float s_data[];
    float* s_vals = s_data;
    float* s_max = &s_data[blockDim.x];
    float* s_sum = &s_data[blockDim.x + 1];

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int lid = threadIdx.x;

    // Load data
    if (idx < n) s_vals[lid] = A[idx];
    else s_vals[lid] = -1e30f;
    __syncthreads();

    // Max reduction
    float val = s_vals[lid];
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (lid < stride) {
            float other = s_vals[lid + stride];
            val = fmaxf(val, other);
            s_vals[lid] = val;
        }
        __syncthreads();
    }

    if (lid == 0) *s_max = s_vals[0];
    __syncthreads();
    val = *s_max;
    __syncthreads();

    // Subtract max and exp
    if (idx < n) {
        val = expf(s_vals[lid] - val);
        s_vals[lid] = val;
    } else {
        val = 0.0f;
    }
    __syncthreads();

    // Sum reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (lid < stride) {
            float other = s_vals[lid + stride];
            val += other;
            s_vals[lid] = val;
        }
        __syncthreads();
    }

    if (lid == 0) *s_sum = s_vals[0];
    __syncthreads();
    val = *s_sum;
    __syncthreads();

    // Normalize
    if (idx < n) C[idx] = s_vals[lid] / val;
}

/* ══════════════════════════════════════════════════════════════════
 * MinGRU Step Kernel
 * ══════════════════════════════════════════════════════════════════ */

__global__ void mingru_step_kernel(
    const float* __restrict__ x,      // [batch, hid]
    const float* __restrict__ h_in,   // [batch, hid]
    const float* __restrict__ Wz,     // [hid, hid]
    const float* __restrict__ Uz,     // [hid, hid]
    const float* __restrict__ bz,     // [hid]
    const float* __restrict__ Wr,     // [hid, hid]
    const float* __restrict__ Ur,     // [hid, hid]
    const float* __restrict__ br,     // [hid]
    const float* __restrict__ Wn,     // [hid, hid]
    const float* __restrict__ Un,     // [hid, hid]
    const float* __restrict__ bn,     // [hid]
    float* __restrict__ h_out,        // [batch, hid]
    int batch, int hid) {
    
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch * hid) return;
    
    int b = idx / hid;
    int j = idx % hid;
    
    // z = sigmoid(x @ Wz^T + h_in @ Uz^T + bz)
    float z = bz[j];
    for (int k = 0; k < hid; ++k) {
        z += x[b * hid + k] * Wz[j * hid + k];
        z += h_in[b * hid + k] * Uz[j * hid + k];
    }
    z = 1.0f / (1.0f + expf(-z));
    
    // r = sigmoid(x @ Wr^T + h_in @ Ur^T + br)
    float r = br[j];
    for (int k = 0; k < hid; ++k) {
        r += x[b * hid + k] * Wr[j * hid + k];
        r += h_in[b * hid + k] * Ur[j * hid + k];
    }
    r = 1.0f / (1.0f + expf(-r));
    
    // n = tanh(x @ Wn^T + (r * h_in) @ Un^T + bn)
    float n = bn[j];
    for (int k = 0; k < hid; ++k) {
        n += x[b * hid + k] * Wn[j * hid + k];
        n += r * h_in[b * hid + k] * Un[j * hid + k];
    }
    n = tanhf(n);
    
    // h_out = (1 - z) * h_in + z * n
    h_out[idx] = (1.0f - z) * h_in[idx] + z * n;
}

/* ══════════════════════════════════════════════════════════════════
 * N-Pole Cartpole Physics Kernel (Simplified — single pole for now)
 * ══════════════════════════════════════════════════════════════════ */

__global__ void npole_step_kernel(
    const float* __restrict__ pole_mass,
    const float* __restrict__ pole_length,
    const float* __restrict__ pole_com,
    const float* __restrict__ pole_inertia,
    float* __restrict__ x,
    float* __restrict__ x_dot,
    float* __restrict__ theta,
    float* __restrict__ theta_dot,
    const float* __restrict__ force,
    float* __restrict__ reward,
    unsigned char* __restrict__ done,
    int n_poles, int num_envs,
    float gravity, float cart_mass, float dt, float force_mag,
    float angle_threshold, float pos_threshold) {
    
    int env = blockIdx.x * blockDim.x + threadIdx.x;
    if (env >= num_envs) return;
    
    float x_val = x[env];
    float xd = x_dot[env];
    
    // Total mass
    float total_mass = cart_mass;
    for (int i = 0; i < n_poles; ++i) total_mass += pole_mass[i];
    
    float f = force[env];
    
    // Simplified: single pole dynamics
    float mp = pole_mass[0];
    float L = pole_length[0];
    float mc = cart_mass;
    float theta_val = theta[env];
    float thd = theta_dot[env];
    float mpc = mp * pole_com[0];
    float I = pole_inertia[0];
    
    float s = sinf(theta_val);
    float c = cosf(theta_val);
    
    // Inverse inertia matrix (2x2 for cart + single pole)
    float det = (mc + mp) * (I + mpc * mpc) - mpc * mpc * c * c;
    float inv_11 = (I + mpc * mpc) / det;
    float inv_12 = mpc * c / det;
    float inv_22 = (mc + mp) / det;
    
    float C0 = f + mpc * thd * thd * s;
    float C1 = -mpc * gravity * s;
    
    float xdd = inv_11 * C0 + inv_12 * C1;
    float thdd = inv_12 * C0 + inv_22 * C1;
    
    // Semi-implicit Euler
    float new_xd = xd + dt * xdd;
    float new_thd = thd + dt * thdd;
    float new_x = x_val + dt * new_xd;
    float new_theta = theta_val + dt * new_thd;
    
    x[env] = new_x;
    x_dot[env] = new_xd;
    theta[env] = new_theta;
    theta_dot[env] = new_thd;
    
    // Reward: r = 1.0 + 0.5 * cos(theta)
    reward[env] = 1.0f + 0.5f * cosf(new_theta);
    
    // Done
    done[env] = (fabsf(new_theta) > angle_threshold || fabsf(new_x) > pos_threshold) ? 1 : 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Adam Optimizer Kernel
 * ══════════════════════════════════════════════════════════════════ */

__global__ void adam_kernel(
    float* __restrict__ param,
    const float* __restrict__ grad,
    float* __restrict__ mom,
    float* __restrict__ var,
    int n, float lr, float beta1, float beta2, float eps, float wd, int step) {
    
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    
    float g = grad[idx];
    float p = param[idx];
    
    if (wd != 0.0f) g += wd * p;
    
    float m = beta1 * mom[idx] + (1.0f - beta1) * g;
    mom[idx] = m;
    
    float v = beta2 * var[idx] + (1.0f - beta2) * g * g;
    var[idx] = v;
    
    float beta1_pow = powf(beta1, (float)step);
    float beta2_pow = powf(beta2, (float)step);
    float m_hat = m / (1.0f - beta1_pow);
    float v_hat = v / (1.0f - beta2_pow);
    
    param[idx] = p - lr * m_hat / (sqrtf(v_hat) + eps);
}

/* ══════════════════════════════════════════════════════════════════
 * PPO Loss Kernel
 * ══════════════════════════════════════════════════════════════════ */

__global__ void ppo_loss_kernel(
    const float* __restrict__ new_logprobs,
    const float* __restrict__ old_logprobs,
    const float* __restrict__ advantages,
    const float* __restrict__ returns,
    const float* __restrict__ old_values,
    const float* __restrict__ new_values,
    float* __restrict__ policy_loss_out,
    float* __restrict__ value_loss_out,
    float* __restrict__ entropy_out,
    float* __restrict__ approx_kl_out,
    float* __restrict__ clip_frac_out,
    int batch, float clip_coef, float vf_coef, float ent_coef,
    int act_dim, int act_discrete) {
    
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch) return;
    
    float new_lp = new_logprobs[idx];
    float old_lp = old_logprobs[idx];
    float adv = advantages[idx];
    float ret = returns[idx];
    float old_v = old_values[idx];
    float new_v = new_values[idx];
    
    // Policy loss
    float diff = new_lp - old_lp;
    diff = fmaxf(fminf(diff, 20.0f), -20.0f);
    float ratio = expf(diff);
    
    float clipped = fmaxf(fminf(ratio, 1.0f + clip_coef), 1.0f - clip_coef);
    float surr1 = ratio * adv;
    float surr2 = clipped * adv;
    float sample_policy_loss = -fminf(surr1, surr2);
    
    // Value loss (clipped)
    float v_pred = new_v;
    float v_clipped = fmaxf(fminf(v_pred, old_v + clip_coef), old_v - clip_coef);
    float loss1 = (v_pred - ret) * (v_pred - ret);
    float loss2 = (v_clipped - ret) * (v_clipped - ret);
    float sample_value_loss = 0.5f * fmaxf(loss1, loss2);
    
    // Entropy (for discrete actions - approximate)
    float entropy = 0.0f;
    if (act_discrete && act_dim > 1) {
        // This would need the actual action probs - simplified
        entropy = 0.5f; // Placeholder
    }
    
    // Atomic adds for reduction
    atomicAdd(policy_loss_out, sample_policy_loss);
    atomicAdd(value_loss_out, sample_value_loss);
    atomicAdd(entropy_out, entropy);
    atomicAdd(approx_kl_out, (ratio - 1.0f) - logf(ratio + 1e-8f));
    atomicAdd(clip_frac_out, (ratio > 1.0f + clip_coef || ratio < 1.0f - clip_coef) ? 1.0f : 0.0f);
}

/* ══════════════════════════════════════════════════════════════════
 * Host Launch Wrappers
 * ══════════════════════════════════════════════════════════════════ */

void bear_cuda_matmul(const float* A, const float* B, float* C, int M, int K, int N) {
    dim3 block(MATMUL_TILE, MATMUL_TILE);
    dim3 grid((N + MATMUL_TILE - 1) / MATMUL_TILE, (M + MATMUL_TILE - 1) / MATMUL_TILE);
    matmul_kernel<<<grid, block>>>(A, B, C, M, K, N);
    cudaDeviceSynchronize();
}

void bear_cuda_add(const float* A, const float* B, float* C, int n) {
    int block = 256;
    int grid = (n + block - 1) / block;
    add_kernel<<<grid, block>>>(A, B, C, n);
    cudaDeviceSynchronize();
}

void bear_cuda_relu(const float* A, float* C, int n) {
    int block = 256;
    int grid = (n + block - 1) / block;
    relu_kernel<<<grid, block>>>(A, C, n);
    cudaDeviceSynchronize();
}

void bear_cuda_sigmoid(const float* A, float* C, int n) {
    int block = 256;
    int grid = (n + block - 1) / block;
    sigmoid_kernel<<<grid, block>>>(A, C, n);
    cudaDeviceSynchronize();
}

void bear_cuda_softmax(const float* A, float* C, int n) {
    int block = 256;
    int grid = (n + block - 1) / block;
    size_t shared_mem = (block + 2) * sizeof(float);
    softmax_kernel<<<grid, block, shared_mem>>>(A, C, n);
    cudaDeviceSynchronize();
}

void bear_cuda_mingru_step(
    const float* x, const float* h_in,
    const float* Wz, const float* Uz, const float* bz,
    const float* Wr, const float* Ur, const float* br,
    const float* Wn, const float* Un, const float* bn,
    float* h_out, int batch, int hid) {
    
    int block = 256;
    int grid = (batch * hid + block - 1) / block;
    mingru_step_kernel<<<grid, block>>>(x, h_in, Wz, Uz, bz, Wr, Ur, br, Wn, Un, bn, h_out, batch, hid);
    cudaDeviceSynchronize();
}

void bear_cuda_npole_step(
    const float* pole_mass, const float* pole_length, const float* pole_com, const float* pole_inertia,
    float* x, float* x_dot, float* theta, float* theta_dot,
    const float* force, float* reward, unsigned char* done,
    int n_poles, int num_envs,
    float gravity, float cart_mass, float dt, float force_mag,
    float angle_threshold, float pos_threshold) {
    
    int block = 256;
    int grid = (num_envs + block - 1) / block;
    npole_step_kernel<<<grid, block>>>(
        pole_mass, pole_length, pole_com, pole_inertia,
        x, x_dot, theta, theta_dot,
        force, reward, done,
        n_poles, num_envs,
        gravity, cart_mass, dt, force_mag, angle_threshold, pos_threshold);
    cudaDeviceSynchronize();
}

void bear_cuda_adam(
    float* param, const float* grad, float* mom, float* var,
    int n, float lr, float beta1, float beta2, float eps, float wd, int step) {
    
    int block = 256;
    int grid = (n + block - 1) / block;
    adam_kernel<<<grid, block>>>(param, grad, mom, var, n, lr, beta1, beta2, eps, wd, step);
    cudaDeviceSynchronize();
}

void bear_cuda_ppo_loss(
    const float* new_logprobs, const float* old_logprobs,
    const float* advantages, const float* returns, const float* old_values, const float* new_values,
    float* policy_loss, float* value_loss, float* entropy, float* approx_kl, float* clip_frac,
    int batch, float clip_coef, float vf_coef, float ent_coef, int act_dim, int act_discrete) {
    
    // Initialize output scalars to 0
    cudaMemset(policy_loss, 0, sizeof(float));
    cudaMemset(value_loss, 0, sizeof(float));
    cudaMemset(entropy, 0, sizeof(float));
    cudaMemset(approx_kl, 0, sizeof(float));
    cudaMemset(clip_frac, 0, sizeof(float));
    
    int block = 256;
    int grid = (batch + block - 1) / block;
    ppo_loss_kernel<<<grid, block>>>(new_logprobs, old_logprobs, advantages, returns, old_values, new_values,
                                      policy_loss, value_loss, entropy, approx_kl, clip_frac,
                                      batch, clip_coef, vf_coef, ent_coef, act_dim, act_discrete);
    cudaDeviceSynchronize();
    
    // Divide by batch on host
    float h_policy_loss, h_value_loss, h_entropy, h_approx_kl, h_clip_frac;
    cudaMemcpy(&h_policy_loss, policy_loss, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_value_loss, value_loss, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_entropy, entropy, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_approx_kl, approx_kl, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_clip_frac, clip_frac, sizeof(float), cudaMemcpyDeviceToHost);
    
    h_policy_loss /= batch;
    h_value_loss /= batch;
    h_entropy /= batch;
    h_approx_kl /= batch;
    h_clip_frac /= batch;
    
    cudaMemcpy(policy_loss, &h_policy_loss, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(value_loss, &h_value_loss, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(entropy, &h_entropy, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(approx_kl, &h_approx_kl, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(clip_frac, &h_clip_frac, sizeof(float), cudaMemcpyHostToDevice);
}
