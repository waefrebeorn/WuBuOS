/*
 * wubu_bonzi_observe.h -- the companion's live-state observer.
 *
 * The single entry point the render loop calls to push live companion + WM
 * state to the meta-layer (/n/bonzi mirror + ambient context). Weak-linked
 * so the renderer never hard-depends on the namespace/context modules.
 */
#ifndef WUBU_BONZI_OBSERVE_H
#define WUBU_BONZI_OBSERVE_H

/* Push the live companion + focused-window state to the meta-layer
 * consumers. Safe every frame; no-op if the modules aren't linked. */
void wubu_bonzi_observe(void);

#endif /* WUBU_BONZI_OBSERVE_H */
