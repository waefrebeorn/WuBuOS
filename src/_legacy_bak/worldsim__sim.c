#include "worldsim.h"
#include <string.h>

void ws_sim_init(ws_simulation_t *sim, uint32_t seed) {
    memset(sim, 0, sizeof(*sim));
    
    /* Generate terrain */
    ws_terrain_generate(&sim->terrain, seed);
    ws_terrain_erode(&sim->terrain, 20);
    
    /* Init physics */
    ws_physics_init(&sim->physics);
    
    /* Init world (entities)  --  already zeroed */
    sim->world.next_id = 0;
    sim->world.count = 0;
    
    sim->tick = 0;
    sim->paused = 0;
}

void ws_sim_step(ws_simulation_t *sim) {
    if (sim->paused) return;
    
    /* AI step  --  wander/chase/flee */
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        ws_entity_t *e = &sim->world.entities[i];
        if (!e->active) continue;
        if (!(e->component_mask & (1 << WS_COMP_AI))) continue;
        
        ws_ai_t *ai = (ws_ai_t *)&e->components[WS_COMP_AI];
        ws_position_t *pos = NULL;
        ws_velocity_t *vel = NULL;
        
        if (e->component_mask & (1 << WS_COMP_POSITION))
            pos = (ws_position_t *)&e->components[WS_COMP_POSITION];
        if (e->component_mask & (1 << WS_COMP_VELOCITY))
            vel = (ws_velocity_t *)&e->components[WS_COMP_VELOCITY];
        
        if (!pos || !vel) continue;
        
        ai->timer -= sim->physics.dt;
        
        switch (ai->ai_type) {
        case 1: /* wander */
            if (ai->timer <= 0.0f) {
                uint64_t rng = sim->tick + e->id;
                vel->dx = (ws_rng_float(&rng) - 0.5f) * 20.0f;
                vel->dz = (ws_rng_float(&rng) - 0.5f) * 20.0f;
                ai->timer = 2.0f + ws_rng_float(&rng) * 3.0f;
            }
            break;
        case 2: /* chase  --  move toward origin */
            vel->dx = -pos->x * 0.1f;
            vel->dz = -pos->z * 0.1f;
            break;
        case 3: /* flee  --  move away from origin */
            vel->dx = pos->x * 0.1f;
            vel->dz = pos->z * 0.1f;
            break;
        }
    }
    
    /* Physics step */
    ws_physics_step(&sim->world, &sim->terrain, &sim->physics);
    
    sim->tick++;
}

void ws_sim_render(ws_simulation_t *sim) {
    ws_render_terrain(&sim->terrain, &sim->render);
    ws_render_entities(&sim->world, &sim->render);
    /* Minimap in top-right corner */
    ws_render_minimap(&sim->terrain, &sim->render, sim->render.fb_w - 134, 4, 128);
}
