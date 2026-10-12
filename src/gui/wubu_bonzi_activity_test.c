/* src/gui/wubu_bonzi_activity_test.c
 *
 * The companion activity-feedback test (Track B6). Verifies each activity
 * state maps to distinct, sensible animation modifiers:
 *   1. idle = the neutral baseline
 *   2. thinking = narrowed eyes + faster bob than idle
 *   3. working = focused, near-open eyes
 *   4. alert = widest eyes
 *   5. name mapping
 */
#include "wubu_bonzi_activity.h"
#include "wubu_test.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_bonzi_activity_test (companion activity feedback) ===\n");

    wubu_bonzi_anim_mods_t idle = wubu_bonzi_activity_mods(WUBU_ACT_IDLE);
    wubu_bonzi_anim_mods_t think = wubu_bonzi_activity_mods(WUBU_ACT_THINKING);
    wubu_bonzi_anim_mods_t work = wubu_bonzi_activity_mods(WUBU_ACT_WORKING);
    wubu_bonzi_anim_mods_t alert = wubu_bonzi_activity_mods(WUBU_ACT_ALERT);

    /* 1. idle is the neutral baseline */
    CHECK(idle.eye_scale == 1.0f && idle.bob_scale == 1.0f,
          "idle = neutral baseline (eye 1.0, bob 1.0)");

    /* 2. thinking: narrowed eyes + faster bob */
    CHECK(think.eye_scale < idle.eye_scale, "thinking narrows the eyes");
    CHECK(think.bob_scale > idle.bob_scale, "thinking bobs faster");
    CHECK(think.blink_ms < idle.blink_ms, "thinking blinks more often");

    /* 3. working: focused, near-open eyes */
    CHECK(work.eye_scale > think.eye_scale && work.eye_scale <= 1.0f,
          "working = focused, eyes more open than thinking");

    /* 4. alert: widest eyes of all */
    CHECK(alert.eye_scale > work.eye_scale && alert.eye_scale > idle.eye_scale,
          "alert = widest eyes");

    /* 5. names */
    CHECK(strcmp(wubu_bonzi_activity_name(WUBU_ACT_THINKING), "thinking") == 0,
          "thinking name");
    CHECK(strcmp(wubu_bonzi_activity_name(WUBU_ACT_IDLE), "idle") == 0,
          "idle name");

    printf("=== wubu_bonzi_activity_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
