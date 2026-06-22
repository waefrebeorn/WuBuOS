/*
 * bear_kernels.cu  --  BearRL CUDA Kernels (Tensor Core MMA)
 * 
 * Compiled with: nvcc -arch=sm_80 -c bear_kernels.cu -o bear_kernels.o
 * PTX extracted with: nvcc -arch=sm_80 -ptx bear_kernels.cu -o bear_kernels.ptx
 * 
 * Targets NVIDIA Ampere (A100, RTX 30xx) with MMA m16n8k16 FP16 tensor cores
 */

#include <cuda_fp16.h>
#include <mma.h>

using namespace nvcuda;

/* ===================================================================
 * Tensor Core Matrix Multiply Kernel (m16n8k16 FP16)
 * 
 * Each warp computes a 16x8 fragment of C = A * B
 * A: M x K (row-major), B: K x N (col-major), C: M x N (row-major)
 * Grid: (N/8, M/16, batch), Block: (32, 1, 1) = 1 warp per block
 * =================================================================== */

__global__ void bear_mma_f16_m16n8k16_kernel(
    const half* __restrict__ A,
    const half* __restrict__ B,
    half* __restrict__ C,
    int M, int N, int K)
{
    // Warp-level MMA using nvcuda::wmma
    // Each warp computes one 16x8 tile of output
    
    // Declare fragments
    wmma::fragment<wmma::matrix_a, 16, 8, 16, half, wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, 16, 8, 16, half, wmma::col_major> b_frag;
    wmma::fragment<wmma::accumulator, 16, 8, 16, float> c_frag;  // FP32 accumulation
    
    // Initialize output fragment to zero
    wmma::fill_fragment(c_frag, 0.0f);
    
    // Global tile indices
    int tile_m = blockIdx.y * 16;  // Each block handles one 16x8 tile
    int tile_n = blockIdx.x * 8;
    
    // Bounds check
    if (tile_m >= M || tile_n >= N) return;
    
    // Loop over K dimension in chunks of 16
    for (int tile_k = 0; tile_k < K; tile_k += 16) {
        // Load A tile (16x16) from global memory
        wmma::load_matrix_sync(a_frag, 
            A + tile_m * K + tile_k, 
            K);
        
        // Load B tile (16x16) from global memory
        wmma::load_matrix_sync(b_frag,
            B + tile_k * N + tile_n,
            N);
        
        // Matrix multiply-accumulate: C += A * B
        wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    
    // Store result (convert FP32 accumulator to FP16)
    wmma::store_matrix_sync(
        C + tile_m * N + tile_n,
        c_frag,
        N,
        wmma::mem_row_major);
}

/* ===================================================================
 * Batched Matrix Multiply for Multiple Environments
 * 
 * For BearRL vectorized environments: batch multiple matmuls
 * Grid: (N/8, M/16, batch_size)
 * =================================================================== */

__global__ void bear_mma_f16_batched_kernel(
    const half* __restrict__ A,
    const half* __restrict__ B,
    half* __restrict__ C,
    int M, int N, int K,
    int batch_size,
    int stride_A, int stride_B, int stride_C)
{
    wmma::fragment<wmma::matrix_a, 16, 8, 16, half, wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, 16, 8, 16, half, wmma::col_major> b_frag;
    wmma::fragment<wmma::accumulator, 16, 8, 16, float> c_frag;
    
    wmma::fill_fragment(c_frag, 0.0f);
    
    int batch = blockIdx.z;
    if (batch >= batch_size) return;
    
    int tile_m = blockIdx.y * 16;
    int tile_n = blockIdx.x * 8;
    
    if (tile_m >= M || tile_n >= N) return;
    
    const half* A_batch = A + batch * stride_A;
    const half* B_batch = B + batch * stride_B;
    half* C_batch = C + batch * stride_C;
    
    for (int tile_k = 0; tile_k < K; tile_k += 16) {
        wmma::load_matrix_sync(a_frag, A_batch + tile_m * K + tile_k, K);
        wmma::load_matrix_sync(b_frag, B_batch + tile_k * N + tile_n, N);
        wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    
    wmma::store_matrix_sync(C_batch + tile_m * N + tile_n, c_frag, N, wmma::mem_row_major);
}

/* ===================================================================
 * Policy Network Forward Pass Kernel
 * 
 * obs [B, obs_dim] -> actions [B, act_dim], logprobs [B], values [B], h_out [B, hidden_dim]
 * =================================================================== */

__global__ void bear_policy_forward_kernel(
    const float* __restrict__ obs,       // [batch, obs_dim]
    const float* __restrict__ h_in,      // [batch, hidden_dim]
    const float* __restrict__ W1,        // [hidden_dim, obs_dim]
    const float* __restrict__ b1,        // [hidden_dim]
    const float* __restrict__ W2,        // [act_dim + 1 + hidden_dim, hidden_dim]
    const float* __restrict__ b2,        // [act_dim + 1 + hidden_dim]
    float* __restrict__ actions,         // [batch, act_dim]
    float* __restrict__ logprobs,        // [batch]
    float* __restrict__ values,          // [batch]
    float* __restrict__ h_out,           // [batch, hidden_dim]
    int batch, int obs_dim, int hidden_dim, int act_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch) return;
    
    // Pointer to this batch element
    const float* x = obs + idx * obs_dim;
    const float* h = h_in + idx * hidden_dim;
    float* a = actions + idx * act_dim;
    float* lp = logprobs + idx;
    float* v = values + idx;
    float* h_o = h_out + idx * hidden_dim;
    
    // Layer 1: hidden = relu(x @ W1.T + b1)
    // Using FP32 for stability, could use FP16 with tensor cores for large dims
    for (int j = 0; j < hidden_dim; j++) {
        float sum = 0.0f;
        for (int k = 0; k < obs_dim; k++) {
            sum += x[k] * W1[j * obs_dim + k];
        }
        sum += b1[j];
        h_o[j] = fmaxf(sum, 0.0f);  // ReLU
    }
    
    // Layer 2: output = h_o @ W2.T + b2
    // Output: [act_dim (mean) + 1 (logstd) + 1 (value) + hidden_dim (h_out)]
    int out_dim = act_dim + 1 + 1 + hidden_dim;
    
    // Compute action mean
    for (int j = 0; j < act_dim; j++) {
        float sum = 0.0f;
        for (int k = 0; k < hidden_dim; k++) {
            sum += h_o[k] * W2[j * hidden_dim + k];
        }
        a[j] = sum + b2[j];
    }
    
    // Value head
    float v_sum = 0.0f;
    for (int k = 0; k < hidden_dim; k++) {
        v_sum += h_o[k] * W2[(act_dim + 1) * hidden_dim + k];
    }
    v[0] = v_sum + b2[act_dim + 1];
    
    // Logprob computation (simplified - would need proper Gaussian entropy)
    *lp = 0.0f;
    for (int j = 0; j < act_dim; j++) {
        float logstd = b2[act_dim];  // Shared logstd
        *lp += -0.5f * logf(2 * 3.14159265f) - logstd - 0.5f * (a[j] * a[j]) / expf(2 * logstd);
    }
}

/* ===================================================================
 * GAE Advantage Computation Kernel
 * =================================================================== */

__global__ void bear_gae_kernel(
    const float* __restrict__ rewards,
    const uint8_t* __restrict__ dones,
    const float* __restrict__ values,
    float* __restrict__ advantages,
    float* __restrict__ returns,
    int T, int B,
    float gamma, float gae_lambda)
{
    int b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= B) return;
    
    float gae = 0.0f;
    for (int t = T - 1; t >= 0; t--) {
        int idx = t * B + b;
        float v_t = values[idx];
        float v_next = (t == T - 1) ? 0.0f : values[(t + 1) * B + b];
        
        float delta = rewards[idx] + gamma * (1.0f - (float)dones[idx]) * v_next - v_t;
        gae = delta + gamma * gae_lambda * (1.0f - (float)dones[idx]) * gae;
        
        advantages[idx] = gae;
        returns[idx] = gae + v_t;
    }
}

/* ===================================================================
 * N-pole CartPole Physics Step Kernel (Vectorized)
 * =================================================================== */

__global__ void bear_npole_step_kernel(
    float* x, float* x_dot,
    float* theta, float* theta_dot,
    float* reward, uint8_t* done,
    const float* force,
    int n_poles, int num_envs,
    const float* pole_mass, const float* pole_length,
    const float* pole_com, const float* pole_inertia,
    float gravity, float cart_mass, float dt, float force_mag,
    float angle_threshold, float pos_threshold)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_envs) return;
    
    float f = force[idx] * force_mag;
    
    // Simplified single-pole dynamics (extend for N-pole)
    float th = theta[idx];
    float th_d = theta_dot[idx];
    float x_p = x[idx];
    float x_d = x_dot[idx];
    
    float m_p = pole_mass[0];
    float l_p = pole_length[0];
    float com = pole_com[0];
    float I_p = pole_inertia[0];
    
    // Equations of motion for cart-pole
    float total_mass = cart_mass + m_p;
    float pole_mass_length = m_p * l_p;
    
    float cos_th = cosf(th);
    float sin_th = sinf(th);
    
    float temp = (f + pole_mass_length * th_d * th_d * sin_th) / total_mass;
    float th_ddot = (gravity * sin_th - cos_th * temp) / (l_p * (4.0f/3.0f - m_p * cos_th * cos_th / total_mass));
    float x_ddot = temp - pole_mass_length * th_ddot * cos_th / total_mass;
    
    // Semi-implicit Euler integration
    x_d = x_d + x_ddot * dt;
    x_p = x_p + x_d * dt;
    th_d = th_d + th_ddot * dt;
    th = th + th_d * dt;
    
    // Update state
    x[idx] = x_p;
    x_dot[idx] = x_d;
    theta[idx] = th;
    theta_dot[idx] = th_d;
    
    // Reward and done
    float angle = fabsf(th);
    float pos = fabsf(x_p);
    reward[idx] = 1.0f;
    done[idx] = (angle > angle_threshold || pos > pos_threshold) ? 1 : 0;
}

/* ===================================================================
 * PTX/SMALLPTX Entry Points for HolyC FFI
 * 
 * These functions are callable from HolyC via extern "C" declarations
 * and resolve to the compiled CUDA kernels via CUDA driver API.
 * =================================================================== */

extern "C" {
    
/* Launch policy forward pass */
void hc_builtin_bear_policy_forward(
    const float* obs, const float* h_in,
    const float* W1, const float* b1,
    const float* W2, const float* b2,
    float* actions, float* logprobs, float* values, float* h_out,
    int batch, int obs_dim, int hidden_dim, int act_dim)
{
    dim3 block(256);
    dim3 grid((batch + 255) / 256);
    bear_policy_forward_kernel<<<grid, block>>>(
        obs, h_in, W1, b1, W2, b2,
        actions, logprobs, values, h_out,
        batch, obs_dim, hidden_dim, act_dim);
    cudaDeviceSynchronize();
}

/* Launch GAE computation */
void hc_builtin_bear_gae(
    const float* rewards, const uint8_t* dones,
    const float* values,
    float* advantages, float* returns,
    int T, int B, float gamma, float gae_lambda)
{
    dim3 block(256);
    dim3 grid((B + 255) / 256);
    bear_gae_kernel<<<grid, block>>>(
        rewards, dones, values, advantages, returns,
        T, B, gamma, gae_lambda);
    cudaDeviceSynchronize();
}

/* Launch N-pole cartpole step */
void hc_builtin_bear_npole_step(
    float* x, float* x_dot,
    float* theta, float* theta_dot,
    float* reward, uint8_t* done,
    const float* force,
    int n_poles, int num_envs,
    const float* pole_mass, const float* pole_length,
    const float* pole_com, const float* pole_inertia,
    float gravity, float cart_mass, float dt, float force_mag,
    float angle_threshold, float pos_threshold)
{
    dim3 block(256);
    dim3 grid((num_envs + 255) / 256);
    bear_npole_step_kernel<<<grid, block>>>(
        x, x_dot, theta, theta_dot,
        reward, done, force,
        n_poles, num_envs,
        pole_mass, pole_length, pole_com, pole_inertia,
        gravity, cart_mass, dt, force_mag,
        angle_threshold, pos_threshold);
    cudaDeviceSynchronize();
}

/* Launch MMA FP16 matmul */
void hc_builtin_bear_mma_f16(
    const half* A, const half* B, half* C,
    int M, int N, int K)
{
    dim3 grid((N + 7) / 8, (M + 15) / 16);
    dim3 block(32);  // 1 warp per block
    bear_mma_f16_m16n8k16_kernel<<<grid, block>>>(A, B, C, M, N, K);
    cudaDeviceSynchronize();
}

} // extern "C"
