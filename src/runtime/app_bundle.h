/*
 * app_bundle.h -- the .app bundle container (NT plan, stage 4).
 *
 * WHY THIS EXISTS
 *   The naming law says a Windows .exe is a WuBuOS `.app`. Until now a program
 *   could only be launched because something already knew its format: the
 *   desktop searched the app registry for a launch callback, and the run dialog
 *   looked up an app NAME. Nothing could take a FILE, look at its contents and
 *   decide what it was or how to start it -- which is exactly what double-
 *   clicking an arbitrary downloaded program requires.
 *
 *   A `.app` is that decision, made explicit: a small container that names a
 *   PERSONALITY (which of the existing executable formats this is), carries the
 *   entry payload, and lists the `.spell` modules it needs. Launching one is
 *   then mechanical: hand the entry payload to the exec layer, which already
 *   knows every personality.
 *
 * WHAT IT DOES NOT DO
 *   It does not detect formats -- wubu_detect_payload_type() already does that.
 *   It does not execute anything -- wubu_exec() already does that. This file
 *   only describes a bundle and refuses to describe a dishonest one.
 *
 * FAILURE POLICY
 *   Fail closed, same as wubu_spellmod: bad magic, unknown version, undefined
 *   flags, counts over their caps, regions not fitting whole, a personality the
 *   tree does not implement, or an entry that runs past the end -- all refused,
 *   with the caller's outputs untouched. A bundle that lies about where its
 *   entry point lives must never reach the exec layer.
 */
#ifndef WUBU_APP_BUNDLE_H
#define WUBU_APP_BUNDLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_APP_MAGIC        "WUAPP01"
#define WUBU_APP_VERSION      1

#define WUBU_APP_NAME_MAX     64
#define WUBU_APP_STR_MAX      32
#define WUBU_APP_NEEDS_MAX    32

typedef struct {
    char     name[WUBU_APP_NAME_MAX];
    char     version[WUBU_APP_STR_MAX];
    uint32_t personality;          /* a WUBU_PAYLOAD_TYPE value */
} wubu_app_info_t;

typedef struct wubu_app_bundle wubu_app_bundle_t;

/* Parse and validate. Returns NULL on any structural problem. The buffer must
 * outlive the handle; nothing is copied. */
wubu_app_bundle_t *wubu_app_open(const uint8_t *data, size_t len);
void               wubu_app_close(wubu_app_bundle_t *a);

/* Identity and declared personality. */
int wubu_app_info(const wubu_app_bundle_t *a, wubu_app_info_t *out);

/* The `.spell` modules this bundle declares it needs, so a loader can check
 * them before handing over control. Copies at most `cap` entries. */
int wubu_app_needs(const wubu_app_bundle_t *a, char out[][WUBU_APP_NAME_MAX], int cap);

/* The entry payload -- the bytes wubu_exec() wants, with no container around
 * them. Returns NULL when the bundle carries no entry (a data-only bundle). */
const uint8_t *wubu_app_entry(const wubu_app_bundle_t *a, uint32_t *len);

/* Human-readable personality name, for the UI and for logs. */
const char *wubu_app_personality_name(uint32_t personality);

/* Launch the bundle's entry through the existing exec layer.
 * Returns a WUBU_EXEC_RESULT code. Refuses (without calling the exec layer)
 * when the bundle has no entry or names a personality this tree cannot run. */
int wubu_app_launch(const wubu_app_bundle_t *a);

#endif /* WUBU_APP_BUNDLE_H */
