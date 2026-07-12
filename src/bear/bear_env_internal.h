/*
 * bear_env_internal.h -- WuBuOS BearRL environment shared state
 *
 * Holds the episode-tracking globals and helpers shared between the core
 * bear_env.c and the extracted subsystems (N-pole cartpole, ...).  This is the
 * C11 opaque-safe "internal header" pattern: public API stays in bear_env.h,
 * cross-TU implementation details live here.  No god headers.
 */

#ifndef WUBU_BEAR_ENV_INTERNAL_H
#define WUBU_BEAR_ENV_INTERNAL_H

#include "bear_env.h"
#include "bear_arena.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Episode-tracking scratch buffers (canonical definitions live in bear_env.c).
 * Exposed here so extracted subsystems (e.g. N-pole cartpole) can read/write
 * g_episode_return without duplicating storage. */
extern int    *g_episode_step;
extern float  *g_episode_return;
extern float  *g_episode_return_snapshot;
extern int     g_max_envs;

/* Grow the episode arrays to hold at least num_envs entries. */
void ensure_episode_arrays(int num_envs);

#endif /* WUBU_BEAR_ENV_INTERNAL_H */
