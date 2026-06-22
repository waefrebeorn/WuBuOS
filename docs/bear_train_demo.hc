/*
 * bear_train_demo.hc  --  BearRL Training Demo for HolyC Terminal
 * 
 * Compile and run in HolyC Terminal:
 *   #include "bear_train_demo.hc"
 *   BearTrainDemo();
 * 
 * This demonstrates:
 * 1. CUDA tensor core MMA matmul via HolyC FFI
 * 2. Vulkan compute shader policy forward pass
 * 3. GAE advantage computation
 * 4. N-pole cartpole physics on GPU
 * 5. Real-time VBE plots of training curves
 */

#include "holyc.h"
#include "vbe.h"
#include "wubu_theme.h"

/* ===================================================================
 * HolyC Extern Declarations for GPU Functions
 * =================================================================== */

extern "C" void hc_builtin_bear_policy_forward(
    const float* obs, const float* h_in,
    const float* W1, const float* b1,
    const float* W2, const float* b2,
    float* actions, float* logprobs, float* values, float* h_out,
    int batch, int obs_dim, int hidden_dim, int act_dim);

extern "C" void hc_builtin_bear_gae(
    const float* rewards, const uint8_t* dones,
    const float* values,
    float* advantages, float* returns,
    int T, int B, float gamma, float gae_lambda);

extern "C" void hc_builtin_bear_npole_step(
    float* x, float* x_dot,
    float* theta, float* theta_dot,
    float* reward, uint8_t* done,
    const float* force,
    int n_poles, int num_envs,
    const float* pole_mass, const float* pole_length,
    const float* pole_com, const float* pole_inertia,
    float gravity, float cart_mass, float dt, float force_mag,
    float angle_threshold, float pos_threshold);

extern "C" void hc_builtin_bear_mma_f16(
    const half* A, const half* B, half* C,
    int M, int N, int K);

/* ===================================================================
 * Demo: MMA FP16 Tensor Core MatMul
 * =================================================================== */

U0 BearMMA_SimpleTest() {
    Print("=== MMA FP16 Tensor Core MatMul Test ===\n");
    
    /* Small test: 16x16 @ 16x16 = 16x16 */
    I64 M = 16, N = 16, K = 16;
    
    /* Allocate FP16 matrices on host */
    half* A = malloc(M * K * sizeof(half));
    half* B = malloc(K * N * sizeof(half));
    half* C = malloc(M * N * sizeof(half));
    
    /* Initialize A with identity-like pattern */
    for (I64 i = 0; i < M; i++) {
        for (I64 j = 0; j < K; j++) {
            A[i * K + j] = __float2half((i == j) ? 1.0 : 0.1);
        }
    }
    
    /* Initialize B with random pattern */
    for (I64 i = 0; i < K; i++) {
        for (I64 j = 0; j < N; j++) {
            B[i * N + j] = __float2half((float)(i + j) * 0.01);
        }
    }
    
    Print("Launching MMA FP16 kernel: M=%d, N=%d, K=%d\n", M, N, K);
    
    /* Launch MMA kernel */
    hc_builtin_bear_mma_f16(A, B, C, M, N, K);
    
    /* Verify results */
    Print("Result C[0][0] = %f\n", __half2float(C[0]));
    Print("Result C[15][15] = %f\n", __half2float(C[15 * N + 15]));
    
    free(A); free(B); free(C);
    
    Print("MMA Test Complete!\n");
}

/* ===================================================================
 * Demo: Policy Network Forward Pass
 * =================================================================== */

U0 BearPolicyForwardTest() {
    Print("=== Policy Network Forward Pass Test ===\n");
    
    I64 batch = 4;
    I64 obs_dim = 8;
    I64 hidden_dim = 64;
    I64 act_dim = 2;
    
    /* Allocate tensors */
    float* obs = malloc(batch * obs_dim * sizeof(float));
    float* h_in = malloc(batch * hidden_dim * sizeof(float));
    float* W1 = malloc(hidden_dim * obs_dim * sizeof(float));
    float* b1 = malloc(hidden_dim * sizeof(float));
    float* W2 = malloc((act_dim + 1 + 1 + hidden_dim) * hidden_dim * sizeof(float));
    float* b2 = malloc((act_dim + 1 + 1 + hidden_dim) * sizeof(float));
    
    float* actions = malloc(batch * act_dim * sizeof(float));
    float* logprobs = malloc(batch * sizeof(float));
    float* values = malloc(batch * sizeof(float));
    float* h_out = malloc(batch * hidden_dim * sizeof(float));
    
    /* Initialize with test data */
    for (I64 i = 0; i < batch * obs_dim; i++) obs[i] = (float)i * 0.01;
    for (I64 i = 0; i < batch * hidden_dim; i++) h_in[i] = 0.0f;
    for (I64 i = 0; i < hidden_dim * obs_dim; i++) W1[i] = 0.1f;
    for (I64 i = 0; i < hidden_dim; i++) b1[i] = 0.01f;
    for (I64 i = 0; i < (act_dim + 1 + 1 + hidden_dim) * hidden_dim; i++) W2[i] = 0.05f;
    for (I64 i = 0; i < (act_dim + 1 + 1 + hidden_dim); i++) b2[i] = 0.0f;
    b2[act_dim] = -1.0f;  // logstd = -1.0 (std = 0.37)
    
    Print("Launching Policy Forward: batch=%d, obs=%d, hidden=%d, act=%d\n", 
          batch, obs_dim, hidden_dim, act_dim);
    
    hc_builtin_bear_policy_forward(obs, h_in, W1, b1, W2, b2,
                                    actions, logprobs, values, h_out,
                                    batch, obs_dim, hidden_dim, act_dim);
    
    Print("Actions[0]: %f, %f\n", actions[0], actions[1]);
    Print("Logprob[0]: %f\n", logprobs[0]);
    Print("Value[0]: %f\n", values[0]);
    
    free(obs); free(h_in); free(W1); free(b1); free(W2); free(b2);
    free(actions); free(logprobs); free(values); free(h_out);
    
    Print("Policy Forward Test Complete!\n");
}

/* ===================================================================
 * Demo: CartPole Physics on GPU
 * =================================================================== */

U0 BearCartPoleTest() {
    Print("=== N-pole CartPole GPU Test ===\n");
    
    I64 n_poles = 1;
    I64 num_envs = 8;
    
    float* x = malloc(num_envs * sizeof(float));
    float* x_dot = malloc(num_envs * sizeof(float));
    float* theta = malloc(num_envs * n_poles * sizeof(float));
    float* theta_dot = malloc(num_envs * n_poles * sizeof(float));
    float* reward = malloc(num_envs * sizeof(float));
    uint8_t* done = malloc(num_envs * sizeof(uint8_t));
    float* force = malloc(num_envs * sizeof(float));
    
    float pole_mass[4] = {0.1f, 0.05f, 0.025f, 0.01f};
    float pole_length[4] = {0.5f, 0.4f, 0.3f, 0.2f};
    float pole_com[4] = {0.25f, 0.2f, 0.15f, 0.1f};
    float pole_inertia[4] = {0.004f, 0.002f, 0.001f, 0.0005f};
    
    /* Initialize environments */
    for (I64 i = 0; i < num_envs; i++) {
        x[i] = 0.0f;
        x_dot[i] = 0.0f;
        theta[i * n_poles] = 0.01f * (float)(i + 1);  // Small initial angle
        theta_dot[i * n_poles] = 0.0f;
        force[i] = (i % 2 == 0) ? 1.0f : -1.0f;  // Alternate forces
    }
    
    Print("Running %d steps on %d environments...\n", 10, num_envs);
    
    for (I64 step = 0; step < 10; step++) {
        hc_builtin_bear_npole_step(x, x_dot, theta, theta_dot, reward, done,
                                    force, n_poles, num_envs,
                                    pole_mass, pole_length, pole_com, pole_inertia,
                                    9.8f, 1.0f, 0.02f, 10.0f,
                                    0.209f, 2.4f);  // 12 deg, 2.4 m
        
        if (step % 3 == 0) {
            Print("Step %d: x[0]=%f, th[0]=%f, r[0]=%f, done[0]=%d\n",
                  step, x[0], theta[0], reward[0], done[0]);
        }
    }
    
    free(x); free(x_dot); free(theta); free(theta_dot);
    free(reward); free(done); free(force);
    
    Print("CartPole Test Complete!\n");
}

/* ===================================================================
 * Demo: GAE Advantage Computation
 * =================================================================== */

U0 BearGAETest() {
    Print("=== GAE Advantage Computation Test ===\n");
    
    I64 T = 10;
    I64 B = 4;
    float gamma = 0.99f;
    float gae_lambda = 0.95f;
    
    float* rewards = malloc(T * B * sizeof(float));
    uint8_t* dones = malloc(T * B * sizeof(uint8_t));
    float* values = malloc(T * B * sizeof(float));
    float* advantages = malloc(T * B * sizeof(float));
    float* returns = malloc(T * B * sizeof(float));
    
    /* Initialize test trajectory */
    for (I64 t = 0; t < T; t++) {
        for (I64 b = 0; b < B; b++) {
            I64 idx = t * B + b;
            rewards[idx] = 1.0f;
            dones[idx] = (t == T - 1) ? 1 : 0;
            values[idx] = (float)(T - t) * 0.1f;
        }
    }
    
    Print("Computing GAE: T=%d, B=%d, gamma=%f, lambda=%f\n", T, B, gamma, gae_lambda);
    
    hc_builtin_bear_gae(rewards, dones, values, advantages, returns, T, B, gamma, 0.95f);
    
    Print("Advantages[0]: %f, Returns[0]: %f\n", advantages[0], returns[0]);
    Print("Advantages[T-1]: %f, Returns[T-1]: %f\n", advantages[(T-1)*B], returns[(T-1)*B]);
    
    free(rewards); free(dones); free(values); free(advantages); free(returns);
    
    Print("GAE Test Complete!\n");
}

/* ===================================================================
 * Real-time VBE Plot: Training Loss Curve
 * =================================================================== */

U0 BearPlotTrainingCurve() {
    Print("=== Real-time Training Curve Plot ===\n");
    
    VBEState* vs = vbe_state();
    if (!vs) {
        Print("VBE not initialized!\n");
        return;
    }
    
    const WubuThemeColors* tc = wubu_theme_colors();
    I64 win_w = 800, win_h = 600;
    I64 margin = 40;
    I64 plot_w = win_w - 2 * margin;
    I64 plot_h = win_h - 2 * margin;
    
    /* Generate mock training loss data */
    const I64 MAX_POINTS = 200;
    float losses[MAX_POINTS];
    float values[MAX_POINTS];
    
    for (I64 i = 0; i < MAX_POINTS; i++) {
        losses[i] = 1.0f / (1.0f + (float)i * 0.05f) + 0.02f * sinf((float)i * 0.1f);
        values[i] = 10.0f + 5.0f * sinf((float)i * 0.15f);
    }
    
    /* Find min/max for scaling */
    float min_loss = losses[0], max_loss = losses[0];
    float min_val = values[0], max_val = values[0];
    for (I64 i = 1; i < MAX_POINTS; i++) {
        if (losses[i] < min_loss) min_loss = losses[i];
        if (losses[i] > max_loss) max_loss = losses[i];
        if (values[i] < min_val) min_val = values[i];
        if (values[i] > max_val) max_val = values[i];
    }
    
    /* Animation loop */
    Print("Plotting training curves... Close window to exit.\n");
    Print("Press any key to exit animation.\n");
    
    I64 frame = 0;
    while (1) {
        /* Clear background */
        vbe_fill_rect(margin, margin, plot_w, plot_h, tc->win_face);
        vbe_rect(margin, margin, plot_w, plot_h, tc->border_dark);
        
        /* Draw grid */
        for (I64 i = 1; i < 10; i++) {
            I64 y = margin + i * plot_h / 10;
            vbe_hline(margin, margin + plot_w, y, 0xFF303030);
        }
        for (I64 i = 1; i < 10; i++) {
            I64 x = margin + i * plot_w / 10;
            vbe_vline(x, margin, margin + plot_h, 0xFF303030);
        }
        
        /* Draw loss curve (red) */
        float visible_loss = 0.01f + frame * 0.001f;
        for (I64 i = 1; i < MAX_POINTS; i++) {
            I64 x1 = margin + (i - 1) * plot_w / MAX_POINTS;
            I64 y1 = margin + plot_h - (I64)((losses[i - 1] - min_loss) / (max_loss - min_loss + 0.001f) * plot_h);
            I64 x2 = margin + i * plot_w / MAX_POINTS;
            I64 y2 = margin + plot_h - (I64)((losses[i] - min_loss) / (max_loss - min_loss + 0.001f) * plot_h);
            vbe_line(x1, y1, x2, y2, 0xFFFF0000);
        }
        
        /* Draw value curve (blue, offset for visibility) */
        for (I64 i = 1; i < MAX_POINTS; i++) {
            I64 x1 = margin + (i - 1) * plot_w / MAX_POINTS;
            I64 y1 = margin + plot_h - (I64)((values[i - 1] - min_val) / (max_val - min_val + 0.001f) * plot_h * 0.5f) - plot_h / 4;
            I64 x2 = margin + i * plot_w / MAX_POINTS;
            I64 y2 = margin + plot_h - (I64)((values[i] - min_val) / (max_val - min_val + 0.001f) * plot_h * 0.5f) - plot_h / 4;
            vbe_line(x1, y1, x2, y2, 0xFF0000FF);
        }
        
        /* Labels */
        vbe_draw_text(margin + 10, margin + 10, "Training Loss (Red) / Value (Blue)", 0xFFFFFFFF, 1);
        vbe_draw_text(margin + 10, margin + 25, "Frame: ", 0xFFFFFF00, 1);
        
        char frame_str[32];
        sprintf(frame_str, "%d", frame);
        vbe_draw_text(margin + 90, margin + 25, frame_str, 0xFFFFFF00, 1);
        
        vbe_swap();
        
        frame++;
        Sleep(50);  // ~20 FPS
        
        /* Check for key press to exit */
        if (kbhit()) break;
    }
    
    Print("Plot closed.\n");
}

/* ===================================================================
 * Main Demo Entry Point
 * =================================================================== */

U0 BearTrainDemo() {
    Print("════════════════════════════════════════\n");
    Print("   BearRL Training Demo for HolyC Term \n");
    Print("════════════════════════════════════════\n\n");
    
    Print("Available demos:\n");
    Print("  1 - MMA FP16 Tensor Core MatMul\n");
    Print("  2 - Policy Network Forward Pass\n");
    Print("  3 - N-pole CartPole GPU Physics\n");
    Print("  4 - GAE Advantage Computation\n");
    Print("  5 - Real-time Training Curve Plot\n");
    Print("  6 - Run All Tests\n\n");
    Print("Enter demo number: ");
    
    I64 choice = 0;
    Scan(&choice);
    
    switch (choice) {
        case 1: BearMMA_SimpleTest(); break;
        case 2: BearPolicyForwardTest(); break;
        case 3: BearCartPoleTest(); break;
        case 4: BearGAETest(); break;
        case 5: BearPlotTrainingCurve(); break;
        case 6:
            BearMMA_SimpleTest();
            BearPolicyForwardTest();
            BearCartPoleTest();
            BearGAETest();
            break;
        default:
            Print("Invalid choice!\n");
    }
    
    Print("\nDemo complete. Type 'BearTrainDemo()' to run again.\n");
}

/* ===================================================================
 * Quick Test Macro
 * =================================================================== */

#define BEAR_QUICK_TEST() \
    do { \
        BearMMA_SimpleTest(); \
        BearPolicyForwardTest(); \
        BearCartPoleTest(); \
        BearGAETest(); \
    } while(0)
