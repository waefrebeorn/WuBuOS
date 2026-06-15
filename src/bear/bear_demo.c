/*
 * bear_demo.c  --  Live Cartoon Demo for WuBuOS / BearRL
 * Runs trained N-Pole Cartpole policy and renders to X11 window.
 * Pure C11, Xlib only (no heavy toolkits).
 */

#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_simd.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/select.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------ */
/* Demo Configuration                                                 */

#define WINDOW_W       1024
#define WINDOW_H       768
#define FPS            60
#define PHYSICS_DT     0.02f
#define TARGET_POLES   10
#define WORLD_W        8.0f      /* world units = ±4.0 (more room for 1.0m poles + swing) */
#define CART_W         1.0f      /* wider cart */
#define CART_H         0.5f      /* taller cart */
#define POLE_LEN       1.0f      /* standard 1.0m full pole length (matches physics) */
#define POLE_W         14        /* thick poles */
#define SEG_COUNT      5         /* visible segments per pole */
#define JOINT_R        3         /* small joint circles (was 10 = giant meatballs) */

/* Colors (Win98 palette) */
#define COL_BG         0x000080  /* Navy */
#define COL_GROUND     0x808080  /* Gray */
#define COL_CART       0x0000FF  /* Blue */
#define COL_WHEEL      0x000000  /* Black */
#define COL_POLE       0xFFFFFF  /* White */
#define COL_POLE_DARK  0xAAAAAA  /* Darker for 3D effect */
#define COL_JOINT      0xFF0000  /* Red joints */
#define COL_TEXT       0xFFFF00  /* Yellow */
#define COL_HUD_BG     0x000080
#define COL_HUD_BORDER 0xFFFFFF

/* ------------------------------------------------------------------ */
/* X11 Rendering State                                                */

typedef struct {
    Display* dpy;
    Window win;
    GC gc;
    int screen;
    Visual* visual;
    Colormap cmap;
    XFontStruct* font;
    int width, height;
    float world_to_screen_x;  /* world -> screen scale */
    float screen_x_offset;    /* origin shift */
    int ground_y;             /* screen Y of ground line */
} X11Context;

/* ------------------------------------------------------------------ */

static void x11_init(X11Context* ctx, int w, int h) {
    ctx->width = w;
    ctx->height = h;
    ctx->dpy = XOpenDisplay(NULL);
    if (!ctx->dpy) {
        fprintf(stderr, "ERROR: XOpenDisplay failed - DISPLAY=%s\n", getenv("DISPLAY"));
        exit(1);
    }
    fprintf(stderr, "DEBUG: X11 connected to display %s\n", getenv("DISPLAY"));
    ctx->screen = DefaultScreen(ctx->dpy);
    ctx->visual = DefaultVisual(ctx->dpy, ctx->screen);
    ctx->cmap = DefaultColormap(ctx->dpy, ctx->screen);
    
    ctx->win = XCreateSimpleWindow(ctx->dpy, RootWindow(ctx->dpy, ctx->screen),
                                    0, 0, w, h, 2,
                                    BlackPixel(ctx->dpy, ctx->screen),
                                    BlackPixel(ctx->dpy, ctx->screen));
    
    XSelectInput(ctx->dpy, ctx->win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XStoreName(ctx->dpy, ctx->win, "WuBuOS . BearRL . N-Pole Cartpole Live Demo");
    XMapWindow(ctx->dpy, ctx->win);
    
    ctx->gc = XCreateGC(ctx->dpy, ctx->win, 0, NULL);
    
    /* Load a fixed-width font */
    ctx->font = XLoadQueryFont(ctx->dpy, "fixed");
    if (ctx->font) XSetFont(ctx->dpy, ctx->gc, ctx->font->fid);
    else fprintf(stderr, "WARNING: Could not load 'fixed' font\n");
    
    /* World to screen mapping */
    ctx->world_to_screen_x = (float)w / WORLD_W;
    ctx->screen_x_offset = w / 2.0f;
    ctx->ground_y = h * 5 / 6;
    
    /* Wait for MapNotify */
    XEvent ev;
    fprintf(stderr, "DEBUG: Waiting for MapNotify...\n");
    int map_received = 0;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    while (!map_received) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(ConnectionNumber(ctx->dpy), &fds);
        int ret = select(ConnectionNumber(ctx->dpy) + 1, &fds, NULL, NULL, &tv);
        if (ret > 0 && XEventsQueued(ctx->dpy, QueuedAlready)) {
            XNextEvent(ctx->dpy, &ev);
            if (ev.type == MapNotify) {
                fprintf(stderr, "DEBUG: MapNotify received\n");
                map_received = 1;
            }
        } else {
            fprintf(stderr, "DEBUG: Timeout waiting for MapNotify, continuing anyway\n");
            break;
        }
    }
}

static void x11_cleanup(X11Context* ctx) {
    if (ctx->font) XFreeFont(ctx->dpy, ctx->font);
    XFreeGC(ctx->dpy, ctx->gc);
    XDestroyWindow(ctx->dpy, ctx->win);
    XCloseDisplay(ctx->dpy);
}

static inline void x11_set_color(X11Context* ctx, unsigned int rgb) {
    XSetForeground(ctx->dpy, ctx->gc, rgb);
}

static inline int world_to_screen_x(X11Context* ctx, float wx) {
    return (int)(ctx->screen_x_offset + wx * ctx->world_to_screen_x);
}

static inline int pole_top_y(X11Context* ctx, float wx, float angle) {
    float px = wx + POLE_LEN * sinf(angle);
    float py = -POLE_LEN * cosf(angle);  /* y up in world coords */
    float sx = ctx->screen_x_offset + px * ctx->world_to_screen_x;
    int sy = ctx->ground_y - (int)(py * ctx->world_to_screen_x);
    return sy;
}

static void x11_draw_cartpole(X11Context* ctx, float cart_x, float* thetas, int num_poles, int step, float episode_return) {
    /* Clear */
    x11_set_color(ctx, COL_BG);
    XFillRectangle(ctx->dpy, ctx->win, ctx->gc, 0, 0, ctx->width, ctx->height);
    
    /* Ground line */
    x11_set_color(ctx, COL_GROUND);
    XFillRectangle(ctx->dpy, ctx->win, ctx->gc, 0, ctx->ground_y, ctx->width, ctx->height - ctx->ground_y);
    XDrawLine(ctx->dpy, ctx->win, ctx->gc, 0, ctx->ground_y, ctx->width, ctx->ground_y);
    
    /* Cart position in screen coords */
    int cart_sx = world_to_screen_x(ctx, cart_x);
    int cart_y = ctx->ground_y - (int)(CART_H * ctx->world_to_screen_x / 2);
    int cart_w = (int)(CART_W * ctx->world_to_screen_x);
    int cart_h = (int)(CART_H * ctx->world_to_screen_x);
    int wheel_r = cart_h / 4;
    
    /* Cart body */
    x11_set_color(ctx, COL_CART);
    XFillRectangle(ctx->dpy, ctx->win, ctx->gc, 
                   cart_sx - cart_w/2, cart_y, cart_w, cart_h);
    XDrawRectangle(ctx->dpy, ctx->win, ctx->gc, 
                   cart_sx - cart_w/2, cart_y, cart_w, cart_h);
    
    /* Wheels */
    x11_set_color(ctx, COL_WHEEL);
    XFillArc(ctx->dpy, ctx->win, ctx->gc,
             cart_sx - cart_w/2 - wheel_r, cart_y + cart_h - wheel_r,
             wheel_r*2, wheel_r*2, 0, 360*64);
    XFillArc(ctx->dpy, ctx->win, ctx->gc,
             cart_sx + cart_w/2 - wheel_r, cart_y + cart_h - wheel_r,
             wheel_r*2, wheel_r*2, 0, 360*64);

    /* Cart detail - roof */
    x11_set_color(ctx, 0x0000AA);
    XFillRectangle(ctx->dpy, ctx->win, ctx->gc,
                   cart_sx - cart_w/2 + 4, cart_y - 6, cart_w - 8, 6);
    XDrawRectangle(ctx->dpy, ctx->win, ctx->gc,
                   cart_sx - cart_w/2 + 4, cart_y - 6, cart_w - 8, 6);

    /* Poles (upright = 0 rad) - Draw as thick segmented rectangles with joints */
    for (int p = 0; p < num_poles; ++p) {
        float angle = thetas[p];
        float sin_a = sinf(angle);
        float cos_a = cosf(angle);
        
        /* Base of pole at cart center top */
        float base_x = cart_x;
        float base_y = CART_H;  /* world Y = cart height (pole attaches to cart top) */
        
        /* Pole thickness in world units */
        float pole_thick = POLE_W / ctx->world_to_screen_x;
        float half_thick = pole_thick / 2.0f;
        
        /* Draw each segment */
        float seg_len = POLE_LEN / SEG_COUNT;
        float curr_x = base_x;
        float curr_y = base_y;
        
        for (int s = 0; s < SEG_COUNT; ++s) {
            /* Segment endpoints */
            float next_x = curr_x + seg_len * sin_a;
            float next_y = curr_y + seg_len * cos_a;
            
            /* Perpendicular vector for thickness */
            float perp_x = -cos_a * half_thick;
            float perp_y = sin_a * half_thick;
            
            /* Four corners of segment rectangle */
            XPoint pts[4];
            pts[0].x = world_to_screen_x(ctx, curr_x + perp_x);
            pts[0].y = ctx->ground_y - (int)(curr_y + perp_y) * ctx->world_to_screen_x;
            pts[1].x = world_to_screen_x(ctx, curr_x - perp_x);
            pts[1].y = ctx->ground_y - (int)(curr_y - perp_y) * ctx->world_to_screen_x;
            pts[2].x = world_to_screen_x(ctx, next_x - perp_x);
            pts[2].y = ctx->ground_y - (int)(next_y - perp_y) * ctx->world_to_screen_x;
            pts[3].x = world_to_screen_x(ctx, next_x + perp_x);
            pts[3].y = ctx->ground_y - (int)(next_y + perp_y) * ctx->world_to_screen_x;
            
            /* Draw filled polygon for segment */
            x11_set_color(ctx, (s % 2 == 0) ? COL_POLE : COL_POLE_DARK);
            XFillPolygon(ctx->dpy, ctx->win, ctx->gc, pts, 4, Convex, CoordModeOrigin);
            
            /* Draw segment outline */
            x11_set_color(ctx, 0x888888);
            XDrawLines(ctx->dpy, ctx->win, ctx->gc, pts, 4, CoordModeOrigin);
            
            /* Draw joint circle at segment boundary */
            if (s < SEG_COUNT - 1) {
                int joint_x = world_to_screen_x(ctx, next_x);
                int joint_y = ctx->ground_y - (int)(next_y * ctx->world_to_screen_x);
                int joint_r_screen = (int)(JOINT_R * ctx->world_to_screen_x / POLE_W);
                if (joint_r_screen < 2) joint_r_screen = 2;
                
                x11_set_color(ctx, COL_JOINT);
                XFillArc(ctx->dpy, ctx->win, ctx->gc,
                         joint_x - joint_r_screen, joint_y - joint_r_screen,
                         joint_r_screen * 2, joint_r_screen * 2, 0, 360*64);
                x11_set_color(ctx, 0x880000);
                XDrawArc(ctx->dpy, ctx->win, ctx->gc,
                         joint_x - joint_r_screen, joint_y - joint_r_screen,
                         joint_r_screen * 2, joint_r_screen * 2, 0, 360*64);
            }
            
            /* Draw tip cap on last segment */
            if (s == SEG_COUNT - 1) {
                int tip_x = world_to_screen_x(ctx, next_x);
                int tip_y = ctx->ground_y - (int)(next_y * ctx->world_to_screen_x);
                int tip_r = (int)(pole_thick * ctx->world_to_screen_x * 0.6f);
                
                x11_set_color(ctx, COL_JOINT);
                XFillArc(ctx->dpy, ctx->win, ctx->gc,
                         tip_x - tip_r, tip_y - tip_r,
                         tip_r * 2, tip_r * 2, 0, 360*64);
            }
            
            curr_x = next_x;
            curr_y = next_y;
        }
    }

    /* HUD */
    x11_set_color(ctx, COL_HUD_BG);
    XFillRectangle(ctx->dpy, ctx->win, ctx->gc, 10, 10, 320, 140);
    x11_set_color(ctx, COL_HUD_BORDER);
    XDrawRectangle(ctx->dpy, ctx->win, ctx->gc, 10, 10, 320, 140);
    
    x11_set_color(ctx, COL_TEXT);
    char buf[128];
    snprintf(buf, sizeof(buf), "WuBuOS BearRL Demo");
    XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, 30, buf, strlen(buf));
    
    snprintf(buf, sizeof(buf), "Poles: %d  Step: %d", num_poles, step);
    XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, 50, buf, strlen(buf));
    
    snprintf(buf, sizeof(buf), "Cart X: %.3f  (thresh: ±2.4)", cart_x);
    XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, 70, buf, strlen(buf));
    
    snprintf(buf, sizeof(buf), "Episode Return: %.1f", episode_return);
    XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, 90, buf, strlen(buf));
    
    for (int p = 0; p < num_poles; ++p) {
        snprintf(buf, sizeof(buf), "  θ%d: %+.3f rad (%+.1f°)", p+1, thetas[p], thetas[p]*180/M_PI);
        XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, 110 + p*15, buf, strlen(buf));
    }
    
    /* Visual angle indicator: upright arrow */
    x11_set_color(ctx, COL_TEXT);
    for (int p = 0; p < num_poles; ++p) {
        float angle = thetas[p];
        int arrow_x = 300;
        int arrow_y = 110 + p*15 + 5;
        int arrow_len = 10;
        int tip_x = arrow_x + (int)(arrow_len * sinf(angle));
        int tip_y = arrow_y - (int)(arrow_len * cosf(angle));
        XDrawLine(ctx->dpy, ctx->win, ctx->gc, arrow_x, arrow_y, tip_x, tip_y);
        /* Arrow head */
        XDrawLine(ctx->dpy, ctx->win, ctx->gc, tip_x, tip_y, tip_x - 3, tip_y + 3);
        XDrawLine(ctx->dpy, ctx->win, ctx->gc, tip_x, tip_y, tip_x + 3, tip_y + 3);
    }
    
    snprintf(buf, sizeof(buf), "Max reward/step = 1.00  |  F1=Restart  Esc=Quit  P=Toggle");
    XDrawString(ctx->dpy, ctx->win, ctx->gc, 20, ctx->height - 20, buf, strlen(buf));
    
    XFlush(ctx->dpy);
}

/* ------------------------------------------------------------------ */
/* Policy Inference (deterministic = mean action)                     */

static void policy_deterministic_inference(BearPolicyNet* net, const float* obs, float* action, BearArena* temp) {
    /* Build observation tensor [1, obs_dim] */
    BearTensor obs_t;
    int64_t obs_shape[2] = { 1, net->obs_dim };
    bear_tensor_create(temp, &obs_t, obs_shape, 2, BEAR_DTYPE_F32, "demo.obs");
    memcpy(obs_t.data, obs, net->obs_dim * sizeof(float));
    
    /* Output tensors */
    BearTensor act_t, logp_t, val_t;
    int64_t act_shape[2] = { 1, net->act_dim };
    bear_tensor_create(temp, &act_t, act_shape, 2, BEAR_DTYPE_F32, "demo.act");
    int64_t scalar_shape[1] = { 1 };
    bear_tensor_create(temp, &logp_t, scalar_shape, 1, BEAR_DTYPE_F32, "demo.logp");
    bear_tensor_create(temp, &val_t, scalar_shape, 1, BEAR_DTYPE_F32, "demo.val");
    
    /* Forward + deterministic (mean) action */
    bear_policy_forward(net, &obs_t, NULL, &act_t, &logp_t, &val_t, NULL, temp);
    bear_policy_deterministic(net, &act_t);
    
    /* Get action (clipped to force_mag) */
    float a = ((float*)act_t.data)[0];
    float force_mag = 10.0f;
    if (a > force_mag) a = force_mag;
    if (a < -force_mag) a = -force_mag;
    *action = a;
}

/* ------------------------------------------------------------------ */
/* PD Controller fallback (for visualization when no trained weights) */

static float pd_control(const float* obs, int num_poles) {
    /* obs = [x, vx, sinθ1, cosθ1, ω1, sinθ2, cosθ2, ω2, ...] */
    float x = obs[0];
    float vx = obs[1];
    
    /* PD on first pole angle (dominant for stability)  --  extract from sin/cos */
    float sin1 = obs[2];
    float cos1 = obs[3];
    float theta1 = atan2f(sin1, cos1);  /* Returns [-π, π], 0 = upright */
    float omega1 = obs[4];
    
    /* Simple PD: force = -Kp * θ - Kd * ω + cart_pos_damping */
    float Kp = 200.0f;   /* Increased for longer poles */
    float Kd = 30.0f;
    float Kx = 8.0f;
    float Kv = 3.0f;
    
    float force = -Kp * theta1 - Kd * omega1 - Kx * x - Kv * vx;
    
    /* Additional poles - add their contributions */
    for (int p = 1; p < num_poles; ++p) {
        float sin_p = obs[2 + 4*p];
        float cos_p = obs[3 + 4*p];
        float theta_p = atan2f(sin_p, cos_p);
        float omega_p = obs[4 + 4*p];
        force += -80.0f * theta_p - 15.0f * omega_p;
    }
    
    /* Clip to [-10, 10] */
    if (force > 10.0f) force = 10.0f;
    if (force < -10.0f) force = -10.0f;
    return force;
}

/* ------------------------------------------------------------------ */

static void run_demo(int num_poles) {
    /* Arena setup */
    const size_t global_cap = 64 * 1024 * 1024;
    const size_t step_cap = 4 * 1024 * 1024;
    BearArena global_arena, step_arena;
    bear_arena_create(&global_arena, global_cap);
    bear_arena_create(&step_arena, step_cap);
    
    /* Create environment (single env for demo) */
    BearEnv* env = bear_env_create_npole(num_poles, 1, &global_arena);
    if (!env) {
        fprintf(stderr, "Failed to create env\n");
        return;
    }
    printf("Env: obs_dim=%d act_dim=%d\n", env->spec.obs_dim, env->spec.act_dim);
    
    /* Create policy network (MLP 128,128 matching training) */
    BearPolicyNet policy;
    int hid[] = { 128, 128 };
    bear_policy_create_mlp(&policy, &global_arena, env->spec.obs_dim, env->spec.act_dim, 0, hid, 2);
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd_fixed = 0.693147f;  /* log(2.0) - but we use deterministic anyway */
    
    /* Control mode: 0=PD, 1=Policy (needs trained weights) */
    int control_mode = 0;
    
    /* Init X11 */
    X11Context x11;
    x11_init(&x11, WINDOW_W, WINDOW_H);
    
    /* Episode state */
    float episode_return = 0.0f;
    int step = 0;
    int running = 1;
    int max_steps = 10000;
    
    /* Reset env */
    bear_env_reset_all(env, &global_arena);
    
    /* Main loop */
    struct timespec frame_start, frame_end;
    while (running && step < max_steps) {
        clock_gettime(CLOCK_MONOTONIC, &frame_start);
        
        /* Handle X11 events (non-blocking) */
        while (XPending(x11.dpy)) {
            XEvent ev;
            XNextEvent(x11.dpy, &ev);
            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) running = 0;
                if (ks == XK_F1) {  /* Restart */
                    bear_env_reset_all(env, &global_arena);
                    episode_return = 0.0f;
                    step = 0;
                }
                if (ks == XK_p || ks == XK_P) {  /* Toggle PD/Policy */
                    control_mode = 1 - control_mode;
                    printf("Control mode: %s\n", control_mode ? "Policy" : "PD");
                }
            }
        }
        
        /* Get observation for env 0 */
        float* obs = (float*)env->obs.data;
        float obs_single[BEAR_MAX_OBS_DIM];
        int state_dim = env->spec.obs_dim;
        for (int i = 0; i < state_dim; ++i) obs_single[i] = obs[i];
        
        /* Action: PD controller (mode 0) or policy (mode 1) */
        float action = 0.0f;
        if (control_mode == 0) {
            action = pd_control(obs_single, num_poles);
        } else {
            bear_arena_reset(&step_arena);
            policy_deterministic_inference(&policy, obs_single, &action, &step_arena);
        }
        
        /* Step environment */
        float* act_data = (float*)env->actions.data;
        act_data[0] = action;
        
        bear_arena_reset(&step_arena);
        bear_env_step(env, &env->actions, &env->rewards, &env->dones, &env->obs, &step_arena);
        
        /* Extract state for rendering  --  convert sin/cos to angle */
        float cart_x = obs_single[0];
        float cart_vx = obs_single[1];
        float thetas[10];  /* max 10 poles */
        for (int p = 0; p < num_poles; ++p) {
            float sin_p = obs_single[2 + 4*p];
            float cos_p = obs_single[3 + 4*p];
            thetas[p] = atan2f(sin_p, cos_p);
        }
        
        /* Episode return */
        float reward = ((float*)env->rewards.data)[0];
        episode_return += reward;
        
        /* Check done */
        uint8_t done = ((uint8_t*)env->dones.data)[0];
        if (done) {
            printf("Episode done at step %d, return=%.1f\n", step, episode_return);
            bear_env_reset_all(env, &global_arena);
            episode_return = 0.0f;
            step = 0;
            usleep(500000);  /* Pause 0.5s on reset */
            continue;
        }
        
        /* Render */
        x11_draw_cartpole(&x11, cart_x, thetas, num_poles, step, episode_return);
        
        step++;
        
        /* Frame rate limiting */
        clock_gettime(CLOCK_MONOTONIC, &frame_end);
        long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L
                        + (frame_end.tv_nsec - frame_start.tv_nsec);
        long target_ns = 1000000000L / FPS;
        if (elapsed_ns < target_ns) {
            struct timespec rem = { 0, target_ns - elapsed_ns };
            nanosleep(&rem, NULL);
        }
    }
    
    /* Cleanup */
    x11_cleanup(&x11);
    bear_env_close(env);
    bear_arena_destroy(&step_arena);
    bear_arena_destroy(&global_arena);
}

int main(int argc, char** argv) {
    int num_poles = TARGET_POLES;
    if (argc > 1) num_poles = atoi(argv[1]);
    if (num_poles < 1) num_poles = 1;
    if (num_poles > 10) num_poles = 10;
    
    printf("===============================================================\n");
    printf("  WuBuOS BearRL  --  Live N-Pole Cartpole Demo\n");
    printf("  Poles: %d  |  Pure C11 + Xlib  |  Deterministic Policy\n", num_poles);
    printf("===============================================================\n");
    printf("Controls:  Esc=Quit  |  F1=Restart Episode\n");
    printf("---------------------------------------------------------------\n");
    
    run_demo(num_poles);
    return 0;
}