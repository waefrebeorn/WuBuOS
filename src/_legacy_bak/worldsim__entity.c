#include "worldsim.h"
#include <string.h>

ws_entity_id ws_entity_create(ws_world_t *w) {
    /* Find free slot */
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        if (!w->entities[i].active) {
            ws_entity_t *e = &w->entities[i];
            e->id = ++w->next_id;
            e->component_mask = 0;
            e->active = 1;
            memset(e->components, 0, sizeof(e->components));
            w->count++;
            return e->id;
        }
    }
    return 0; /* no space */
}

void ws_entity_destroy(ws_world_t *w, ws_entity_id id) {
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        if (w->entities[i].active && w->entities[i].id == id) {
            /* Free sprite pixels if present */
            if (w->entities[i].component_mask & (1 << WS_COMP_SPRITE)) {
                ws_sprite_t *s = (ws_sprite_t *)&w->entities[i].components[WS_COMP_SPRITE];
                if (s->pixels) { /* would free in real OS; here just null */ s->pixels = NULL; }
            }
            w->entities[i].active = 0;
            w->entities[i].component_mask = 0;
            w->count--;
            return;
        }
    }
}

ws_entity_t *ws_entity_get(ws_world_t *w, ws_entity_id id) {
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        if (w->entities[i].active && w->entities[i].id == id)
            return &w->entities[i];
    }
    return NULL;
}

void *ws_entity_add_component(ws_entity_t *e, ws_component_type type) {
    e->component_mask |= (1 << type);
    memset(&e->components[type], 0, sizeof(ws_component_data_t));
    return &e->components[type];
}

void *ws_entity_get_component(ws_entity_t *e, ws_component_type type) {
    if (e->component_mask & (1 << type))
        return &e->components[type];
    return NULL;
}

void ws_entity_remove_component(ws_entity_t *e, ws_component_type type) {
    if (type == WS_COMP_SPRITE) {
        ws_sprite_t *s = (ws_sprite_t *)&e->components[WS_COMP_SPRITE];
        if (s->pixels) s->pixels = NULL;
    }
    e->component_mask &= ~(1 << type);
    memset(&e->components[type], 0, sizeof(ws_component_data_t));
}
