/*
 * bear_qcontroller.c  --  Q-Learning Learning Rate Controller (C port of bytropix OPTIMIZERS/q-controller)
 * Pure C11 implementation.
 */

#include "bear_qcontroller.h"
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

/* Xorshift64* RNG for action selection */
static inline uint64_t xorshift64_star(uint64_t state[2]) {
    uint64_t s1 = state[0];
    uint64_t s0 = state[1];
    state[0] = s0;
    s1 ^= s1 << 23;
    state[1] = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return state[1] * 0x2545F4914F6CDD1Dull;
}

static inline float xorshift64_star_float(uint64_t state[2]) {
    return (xorshift64_star(state) >> 11) * (1.0f / 9007199254740992.0f); /* 1/2^53 */
}

static inline int xorshift64_star_int(uint64_t state[2], int min, int max) {
    return min + (int)(xorshift64_star_float(state) * (max - min));
}

void bear_qcontroller_init(BearQController* qc, const BearQControllerConfig* config, float target_lr, uint64_t seed) {
    for (int i = 0; i < config->num_lr_actions; ++i) qc->q_table[i] = 0.0f;
    for (int i = 0; i < config->metric_history_len; ++i) qc->metric_history[i] = 0.0f;
    qc->history_head = 0;
    qc->history_count = 0;
    qc->current_lr = config->warmup_lr_start;
    qc->exploration_rate = config->exploration_rate;
    qc->step_count = 0;
    qc->last_action_idx = -1;
    qc->status_code = 0;
    qc->rng_state[0] = seed ^ 0xDEADBEEFDEADBEEFull;
    qc->rng_state[1] = (seed ^ 0xCAFEBABECAFEBABEull) + 1;
    (void)target_lr; /* Used in choose_action */
}

/* Helper: compute mean of last N entries in circular buffer */
static float qc_mean_last_n(const BearQController* qc, const BearQControllerConfig* config, int n) {
    if (qc->history_count < n) n = qc->history_count;
    if (n <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        int idx = (qc->history_head - 1 - i);
        if (idx < 0) idx += config->metric_history_len;
        sum += qc->metric_history[idx];
    }
    return sum / n;
}

void bear_qcontroller_choose_action(BearQController* qc, const BearQControllerConfig* config, float target_lr) {
    if (qc->step_count < config->warmup_steps) {
        /* Warmup: linear interpolation from warmup_lr_start to target_lr */
        float alpha = (float)qc->step_count / (float)config->warmup_steps;
        qc->current_lr = config->warmup_lr_start * (1.0f - alpha) + target_lr * alpha;
        qc->step_count++;
        qc->status_code = 0;
        qc->last_action_idx = -1;
        return;
    }

    /* Regular Q-learning action selection (epsilon-greedy) */
    float rand_val = xorshift64_star_float(qc->rng_state);
    int action_idx;

    if (rand_val < qc->exploration_rate) {
        /* Explore: random action */
        action_idx = xorshift64_star_int(qc->rng_state, 0, config->num_lr_actions);
    } else {
        /* Exploit: best Q-value */
        float best_q = qc->q_table[0];
        action_idx = 0;
        for (int i = 1; i < config->num_lr_actions; ++i) {
            if (qc->q_table[i] > best_q) {
                best_q = qc->q_table[i];
                action_idx = i;
            }
        }
    }

    /* Apply LR change factor */
    float factor = config->lr_change_factors[action_idx];
    float new_lr = qc->current_lr * factor;
    if (new_lr < config->lr_min) new_lr = config->lr_min;
    if (new_lr > config->lr_max) new_lr = config->lr_max;
    qc->current_lr = new_lr;

    qc->step_count++;
    qc->last_action_idx = action_idx;
    qc->status_code = 0; /* Will be updated in bear_qcontroller_update */

    /* Decay exploration rate */
    if (qc->exploration_rate > config->min_exploration_rate) {
        qc->exploration_rate *= config->exploration_decay;
        if (qc->exploration_rate < config->min_exploration_rate) {
            qc->exploration_rate = config->min_exploration_rate;
        }
    }
}

void bear_qcontroller_update(BearQController* qc, const BearQControllerConfig* config, float metric_value) {
    /* Store metric in circular buffer */
    qc->metric_history[qc->history_head] = metric_value;
    qc->history_head = (qc->history_head + 1) % config->metric_history_len;
    if (qc->history_count < config->metric_history_len) qc->history_count++;

    /* Can only update after warmup and after taking an action */
    if (qc->step_count <= config->warmup_steps || qc->last_action_idx < 0) {
        qc->status_code = 0;
        return;
    }

    /* Reward = negative mean of last 10 metrics (lower loss = better) */
    float recent_mean = qc_mean_last_n(qc, config, 10);
    float older_mean = qc_mean_last_n(qc, config, 20);

    /* Compare recent vs older to detect improvement */
    int is_improving = (recent_mean > older_mean) ? 1 : 0; /* Higher return = improving */
    qc->status_code = is_improving ? 1 : 2;

    /* Q-learning update: Q(s,a) = Q(s,a) + alpha * (r - Q(s,a)) */
    float reward = recent_mean; /* Use mean return as reward directly */
    float old_q = qc->q_table[qc->last_action_idx];
    float new_q = old_q + config->learning_rate_q * (reward - old_q);
    qc->q_table[qc->last_action_idx] = new_q;
}