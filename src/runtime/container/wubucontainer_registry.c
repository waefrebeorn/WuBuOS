/*
 * wubucontainer_registry.c -- In-memory handler registry for the WuBuContainer
 * agentic layer.
 *
 * This is a SELF-CONTAINED module: it performs only pure in-memory bookkeeping
 * against the opaque engine struct (declared in wubucontainer_internal.h). It
 * does NOT spawn the TypeScript/IPC engine and does NOT touch the network or
 * json-c -- so handler registration/enumeration works with no external
 * dependency present. That keeps the registry usable (and testable) even when
 * the optional Bun/Electron subprocess is unavailable.
 *
 * Split out of the monolithic wubucontainer.c per the WuBuOS monolith-split
 * discipline (opaque struct + minimal includes + no god header).
 */
#include "wubucontainer_internal.h"

#include <string.h>

int wubu_container_register_handler(WubuContainerEngine *engine,
                                     const WubuContainerHandler *handler) {
    if (!engine || !handler) return WUBU_CTR_ERR_INVAL;
    if (handler->name[0] == '\0') return WUBU_CTR_ERR_INVAL;

    /* Reject duplicate registration (same handler name). */
    for (int i = 0; i < engine->custom_handler_count; i++) {
        if (strncmp(engine->custom_handlers[i].name, handler->name,
                    sizeof(handler->name)) == 0)
            return WUBU_CTR_ERR_INVAL;
    }
    if (engine->custom_handler_count >= WUBU_CONTAINER_MAX_HANDLERS)
        return WUBU_CTR_ERR_INVAL;

    /* Real work: persist the handler descriptor into the engine registry so
     * it can be enumerated by wubu_container_get_handlers() and used by the
     * agentic routing layer. */
    WubuContainerHandler *slot =
        &engine->custom_handlers[engine->custom_handler_count];
    memcpy(slot, handler, sizeof(*slot));
    engine->custom_handler_count++;
    return WUBU_CTR_OK;
}

int wubu_container_registered_count(const WubuContainerEngine *engine) {
    if (!engine) return 0;
    return engine->custom_handler_count;
}

const char *wubu_container_registered_name(const WubuContainerEngine *engine, int idx) {
    if (!engine || idx < 0 || idx >= engine->custom_handler_count) return "";
    return engine->custom_handlers[idx].name;
}
