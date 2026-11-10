/*
 * wubu_ns_effect.h -- the /n/effect control subtree.
 */
#ifndef WUBU_NS_EFFECT_H
#define WUBU_NS_EFFECT_H

#include "wubu_effect.h"

/* publish the /n/effect tree. */
int wubu_ns_publish_effect(void);

/* republish /n/effect/stack from the live stack. */
int wubu_ns_effect_refresh(void);

/* the live stack a program edits through /n/effect/set (may be NULL). */
const WubuEffectStack *wubu_ns_effect_stack(void);

/* parse one /n/effect/set payload: "tint 50 ff0000", or "clear". */
int wubu_ns_effect_parse(const char *text);

#endif
