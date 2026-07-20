/*
 * wubufx.h -- WuBuFX: WuBuOS Application Framework (public API)
 *
 * Steve-Jobs-lens design rules (see docs/WUBU_FRAMEWORK_100POINT_PLAN.md):
 *   - One artifact, one target. No JVM, no container image, no arch matrix.
 *   - Namespace-first: an app IS a Styx9 namespace; composition = mount.
 *   - Execution is LIVE HolyC (wubu_holyc_eval) or a native binary node.
 *   - Every first-party action is disclosed to EDR by construction.
 *   - No configuration tax, no annotation magic, no hidden control flow.
 *
 * This header is the ONLY public face. All structs are opaque; the Styx9
 * node layout and HolyC session bookkeeping live in wubufx_internal.h.
 *
 * C11, minimal includes, self-contained (depends only on the AGI + EDR
 * public headers and the Styx9 mount entry points). No god header.
 */

#ifndef WUBUFX_H
#define WUBUFX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -- Errors -----------------------------------------------------------
 * Explicit, not thrown. Every op returns WubuFxErr; 0 == OK.
 * Errors are also surfaced as Styx9 events under /edr/... so the desktop
 * notification center + EDR see them (no silent failure). */
typedef enum {
    WUBUFX_OK            = 0,
    WUBUFX_ERR_PARAM     = 1,   /* null/bad argument */
    WUBUFX_ERR_NOMOUNT   = 2,   /* namespace not mounted */
    WUBUFX_ERR_NOENT     = 3,   /* node missing */
    WUBUFX_ERR_CAP       = 4,   /* capability denied (EDR) */
    WUBUFX_ERR_EVAL      = 5,   /* HolyC compile/run failed */
    WUBUFX_ERR_SIGN      = 6,   /* signature/attestation failed */
    WUBUFX_ERR_LIMIT     = 7,   /* resource cap exceeded */
    WUBUFX_ERR_INTERNAL  = 8
} WubuFxErr;

/* -- Capabilities (node ACL; capability model, not ACL cruft) ----------
 * Least-privilege by default: an app gets exactly the bits it declares. */
typedef uint32_t WubuFxCap;
#define WUBUFX_CAP_READ    (1u << 0)
#define WUBUFX_CAP_WRITE   (1u << 1)
#define WUBUFX_CAP_EXEC    (1u << 2)
#define WUBUFX_CAP_AGI     (1u << 3)   /* may invoke wubufx_agent_eval */
#define WUBUFX_CAP_EDR     (1u << 4)   /* may read EDR feed */
#define WUBUFX_CAP_ALL     0xFFFFFFFFu

/* -- Opaque handles --------------------------------------------------- */
typedef struct WubuFxApp      WubuFxApp;   /* a mounted app namespace */
typedef struct WubuFxNode     WubuFxNode;  /* a node within a namespace */

/* -- Composition root ------------------------------------------------- */
/* Idempotent. Initializes the framework (AGI daemon + EDR linkage). */
WubuFxErr wubufx_init(void);
void      wubufx_shutdown(void);

/* -- Lifecycle: mount -> open -> eval -> close -> unmount -------------
 * An app namespace is content-addressed: `id` resolves under /app/<id>/
 * (or a content hash under /lib/<sha256>). Mounting verifies the signature
 * via EDR attestation (point 5/53 of the plan). */
WubuFxErr wubufx_mount(const char *id, WubuFxCap caps, WubuFxApp **out_app);
void      wubufx_close(WubuFxApp *app);
void      wubufx_unmount(WubuFxApp *app);

/* Open a node inside the app namespace (e.g. "/win", "/main", "/state").
 * Returns NULL on miss. The node carries its own capability bits. */
WubuFxNode *wubufx_open(WubuFxApp *app, const char *path);
void        wubufx_node_close(WubuFxNode *node);

/* -- Execution --------------------------------------------------------
 * wubufx_eval:    compile+run HolyC LIVE in the app's own namespace session
 *                 (isolated symbol table; apps can't clobber each other).
 * wubufx_agent_eval: same, but disclosed to EDR as an AGI action. The AGI
 *                 CANNOT bypass this path (point 44/52). */
WubuFxErr wubufx_eval(WubuFxApp *app, const char *holyc_src,
                      char *out, size_t out_size);
WubuFxErr wubufx_agent_eval(WubuFxApp *app, const char *holyc_src,
                            char *out, size_t out_size);

/* -- State (explicit, no @State magic) --------------------------------
 * Read/write a node's state as a string. State lives in a Styx9 node you
 * can `ls`, so it is never hidden (point 28). */
WubuFxErr wubufx_state_get(WubuFxNode *node, char *out, size_t out_size);
WubuFxErr wubufx_state_set(WubuFxNode *node, const char *value);

/* -- Error text ------------------------------------------------------- */
const char *wubufx_strerr(WubuFxErr e);

#ifdef __cplusplus
}
#endif
#endif /* WUBUFX_H */
