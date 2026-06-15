/*
 * bear_qcontroller.h  --  Q-Learning Learning Rate Controller (C port of bytropix OPTIMIZERS/q-controller)
 * Pure C11, no dependencies. Adjusts optimizer LR based on training metrics.
 */

#ifndef BEAR_QCONTROLLER_H
#define BEAR_QCONTROLLER_H

#include <stdint.h>

/* Q-Controller configuration */
typedef struct {
    int num_lr_actions;                    /* Number of discrete LR actions (default: 5) */
    float lr_change_factors[5];            /* LR multipliers for each action */
    float learning_rate_q;                 /* Q-learning rate (default: 0.1) */
    float lr_min;                          /* Minimum LR (default: 1e-6) */
    float lr_max;                          /* Maximum LR (default: 1e-2) */
    int metric_history_len;                /* History buffer size (default: 100) */
    float exploration_rate;                /* Initial exploration rate (default: 0.3) */
    float min_exploration_rate;            /* Minimum exploration (default: 0.05) */
    float exploration_decay;               /* Exploration decay per step (default: 0.9998) */
    int warmup_steps;                      /* Warmup steps (default: 500) */
    float warmup_lr_start;                 /* Starting LR during warmup (default: 1e-6) */
} BearQControllerConfig;

/* Q-Controller state */
typedef struct {
    float q_table[5];                      /* Q-values for each LR action */
    float metric_history[100];             /* Circular buffer of recent metrics */
    int history_head;                      /* Current write position in history */
    int history_count;                     /* Number of valid entries */
    float current_lr;                      /* Current learning rate */
    float exploration_rate;                /* Current exploration rate */
    int step_count;                        /* Total steps taken */
    int last_action_idx;                   /* Last action chosen (-1 = none) */
    int status_code;                       /* 0=warmup, 1=improving, 2=not improving */
    uint64_t rng_state[2];                 /* RNG state for action selection */
} BearQController;

/* Default configuration */
static inline BearQControllerConfig bear_qcontroller_default_config(void) {
    BearQControllerConfig cfg = {0};
    cfg.num_lr_actions = 5;
    cfg.lr_change_factors[0] = 0.9f;
    cfg.lr_change_factors[1] = 0.95f;
    cfg.lr_change_factors[2] = 1.0f;
    cfg.lr_change_factors[3] = 1.05f;
    cfg.lr_change_factors[4] = 1.1f;
    cfg.learning_rate_q = 0.1f;
    cfg.lr_min = 1e-6f;
    cfg.lr_max = 1e-2f;
    cfg.metric_history_len = 100;
    cfg.exploration_rate = 0.3f;
    cfg.min_exploration_rate = 0.05f;
    cfg.exploration_decay = 0.9998f;
    cfg.warmup_steps = 500;
    cfg.warmup_lr_start = 1e-6f;
    return cfg;
}

/* Initialize Q-controller with target LR (used after warmup) */
void bear_qcontroller_init(BearQController* qc, const BearQControllerConfig* config, float target_lr, uint64_t seed);

/* Choose next LR action (epsilon-greedy) and update internal LR */
void bear_qcontroller_choose_action(BearQController* qc, const BearQControllerConfig* config, float target_lr);

/* Update Q-table based on observed metric (e.g., episode return) */
void bear_qcontroller_update(BearQController* qc, const BearQControllerConfig* config, float metric_value);

/* Get current LR */
static inline float bear_qcontroller_get_lr(const BearQController* qc) {
    return qc->current_lr;
}

/* Get status: 0=warmup, 1=improving, 2=not improving */
static inline int bear_qcontroller_get_status(const BearQController* qc) {
    return qc->status_code;
}

#endif /* BEAR_QCONTROLLER_H */