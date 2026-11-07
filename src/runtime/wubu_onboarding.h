/*
 * wubu_onboarding.h -- P4 Omarchy first-boot onboarding (docs/GAP_ROADMAP, GAP F).
 *
 * The FIRST-RUN STREAM decision: on first boot (no completed-onboarding
 * sentinel) the session launches a GREETER/configurator; once the greeter
 * writes its config we persist a sentinel, and the next boot launches the real
 * DESKTOP. This is the orchestration that turns "session boots" into
 * "configured once, then straight to the desktop".
 *
 * The sentinel path is INJECTED (wubu_onboarding_load) so a hosted test can
 * point it at a temp file and drive the whole flow end-to-end; on the booted
 * kernel the caller supplies a path on the boot volume. Hosted/runtime C11.
 */
#ifndef WUBU_ONBOARDING_H
#define WUBU_ONBOARDING_H

typedef enum {
    WUBU_ONBOARD_GREETER = 0,   /* first boot: launch the configurator */
    WUBU_ONBOARD_DESKTOP        /* configured: launch the real desktop */
} wubu_onboard_target;

/* (Re)load first-boot state from `sentinel_path`. NULL => nothing persisted
 * (treated as first boot). Repeated calls re-read the file (a second boot in
 * the same process sees the sentinel the greeter wrote). */
void wubu_onboarding_load(const char *sentinel_path);

/* 1 until a complete() has persisted a sentinel at the loaded path. */
int wubu_onboarding_first_boot(void);

/* What the session should launch right now. */
wubu_onboard_target wubu_onboarding_target(void);

/* Mark onboarding complete: persist the greeter's config `signature` to the
 * sentinel path and flip this instance to DESKTOP. 0 on success, -1 if no
 * path was loaded or the write failed. */
int wubu_onboarding_complete(const char *signature);

/* The signature recorded at load time ("" if none / first boot). */
const char *wubu_onboarding_signature(void);

#endif /* WUBU_ONBOARDING_H */
