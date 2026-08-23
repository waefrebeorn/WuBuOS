/*
 * wubu_colonel.h -- the everything-through-the-Colonel dispatcher.
 * C11. The HolyC Colonel (the TempleOS/ZealOS lineage) is the OS core:
 * EVERY command -- app launches, OS actions, cross-OS payloads, AGI
 * evals -- dispatches through the Colonel. This module routes a
 * command string to the HolyC engine (hd_eval) and returns a typed
 * result. The unit tests drive it WITHOUT the GUI (the pure core).
 */
#ifndef WUBU_COLONEL_H
#define WUBU_COLONEL_H

#include <stdint.h>

/* The typed dispatch results. */
enum {
    WUBU_COLONEL_OK = 0,       /* eval returned; result valid */
    WUBU_COLONEL_EMPTY,        /* empty command (no-op) */
    WUBU_COLONEL_UNKNOWN,      /* unknown command class */
    WUBU_COLONEL_EVAL_ERR,     /* the HolyC eval failed */
    WUBU_COLONEL_BAD           /* bad args */
};

/* The command classes (what the Colonel routes). */
enum {
    WUBU_COL_CMD_APP = 1,      /* launch an app: run <name> */
    WUBU_COL_CMD_EVAL,         /* evaluate HolyC: eval <source> */
    WUBU_COL_CMD_OS,           /* an OS action: os <verb> */
    WUBU_COL_CMD_SYS,          /* a syscall-ish action: sys <verb> */
    WUBU_COL_CMD_AGI,          /* an AGI action: agi <verb> */
    WUBU_COL_CMD_LOAD,         /* load a payload: load <fmt> <path> */
};

typedef struct {
    int   class;               /* WUBU_COL_CMD_* */
    int   result;              /* WUBU_COLONEL_* */
    int64_t value;             /* the eval result */
    char  cmd[64];             /* the parsed command word */
    char  arg[256];            /* the parsed argument */
} wubu_colonel_t;

/* Parse a command string into its class + args (no eval). */
int wubu_colonel_parse(const char *line, wubu_colonel_t *c);

/* Dispatch: parse + evaluate through the HolyC engine.
 * Returns the result enum. */
int wubu_colonel_dispatch(const char *line, wubu_colonel_t *c,
                          int64_t (*eval_fn)(const char *));

/* The app registry the Colonel consults for `run <name>`.
 * Returns 1 if the app is known (the GUI launches it), else 0. */
int wubu_colonel_app_known(const char *name);

/* Register a NEW app name at runtime (the Revolver Doctrine: the app
 * set is a hot-swappable cylinder, not a soldered const list). A new
 * .wubu container or GUI app registers its name so `colonel run <name>`
 * and app_known() learn it without a recompile. Idempotent. */
int wubu_colonel_app_register(const char *name);

#endif
