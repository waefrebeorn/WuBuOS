#include "worldsim.h"
#include <math.h>

void ws_physics_init(ws_physics_config_t *cfg) {
    cfg->gravity = -9.81f;
    cfg->drag = 0.01f;
    cfg->dt = 1.0f / 60.0f;
    cfg->restitution = 0.5f;
    cfg->terrain_collide = 1;
    cfg->body_collide = 1;
}

void ws_physics_step(ws_world_t *w, const ws_terrain_t *t, const ws_physics_config_t *cfg) {
    float dt = cfg->dt;
    
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        ws_entity_t *e = &w->entities[i];
        if (!e->active) continue;
        if (!(e->component_mask & (1 << WS_COMP_POSITION))) continue;
        
        ws_position_t *pos = (ws_position_t *)&e->components[WS_COMP_POSITION];
        ws_velocity_t *vel = NULL;
        ws_physics_t *phys = NULL;
        
        if (e->component_mask & (1 << WS_COMP_VELOCITY))
            vel = (ws_velocity_t *)&e->components[WS_COMP_VELOCITY];
        if (e->component_mask & (1 << WS_COMP_PHYSICS))
            phys = (ws_physics_t *)&e->components[WS_COMP_PHYSICS];
        
        if (vel) {
            /* Apply gravity */
            if (phys && phys->gravity) {
                vel->dy += cfg->gravity * dt;
            }
            
            /* Apply drag */
            vel->dx *= (1.0f - cfg->drag);
            vel->dy *= (1.0f - cfg->drag);
            vel->dz *= (1.0f - cfg->drag);
            
            /* Integrate position */
            pos->x += vel->dx * dt;
            pos->y += vel->dy * dt;
            pos->z += vel->dz * dt;
        }
        
        /* Terrain collision  --  clamp to surface */
        if (cfg->terrain_collide && phys && phys->collide && t) {
            int tx = (int)pos->x;
            int ty = (int)pos->z; /* z maps to terrain Y */
            if (tx >= 0 && tx < t->width && ty >= 0 && ty < t->height) {
                float ground = (float)t->heightmap[ty * t->width + tx];
                if (pos->y < ground) {
                    pos->y = ground;
                    if (vel) vel->dy = 0.0f; /* stop vertical velocity */
                }
            }
        }
    }

    /* Rigid-body vs rigid-body collision response (sphere approximation).
     * Each collidable entity is a sphere of radius = mean bbox half-extent.
     * On overlap: push apart along the contact normal (mass-weighted) and
     * apply an elastic impulse scaled by restitution. */
    if (cfg->body_collide) {
        for (int i = 0; i < WS_MAX_ENTITIES; i++) {
            ws_entity_t *a = &w->entities[i];
            if (!a->active) continue;
            if (!(a->component_mask & ((1 << WS_COMP_POSITION) | (1 << WS_COMP_PHYSICS)))) continue;
            ws_physics_t *pa = (ws_physics_t *)&a->components[WS_COMP_PHYSICS];
            if (!pa->collide) continue;
            ws_position_t *posa = (ws_position_t *)&a->components[WS_COMP_POSITION];
            ws_velocity_t *vela = (a->component_mask & (1 << WS_COMP_VELOCITY))
                                  ? (ws_velocity_t *)&a->components[WS_COMP_VELOCITY] : NULL;
            float ra = (pa->bbox_w + pa->bbox_h + pa->bbox_d) / 3.0f;
            float ma = pa->mass > 0.0f ? pa->mass : 1.0f;

            for (int j = i + 1; j < WS_MAX_ENTITIES; j++) {
                ws_entity_t *b = &w->entities[j];
                if (!b->active) continue;
                if (!(b->component_mask & ((1 << WS_COMP_POSITION) | (1 << WS_COMP_PHYSICS)))) continue;
                ws_physics_t *pb = (ws_physics_t *)&b->components[WS_COMP_PHYSICS];
                if (!pb->collide) continue;
                ws_position_t *posb = (ws_position_t *)&b->components[WS_COMP_POSITION];
                ws_velocity_t *velb = (b->component_mask & (1 << WS_COMP_VELOCITY))
                                      ? (ws_velocity_t *)&b->components[WS_COMP_VELOCITY] : NULL;
                float rb = (pb->bbox_w + pb->bbox_h + pb->bbox_d) / 3.0f;
                float mb = pb->mass > 0.0f ? pb->mass : 1.0f;

                float dx = posb->x - posa->x;
                float dy = posb->y - posa->y;
                float dz = posb->z - posa->z;
                float dist2 = dx*dx + dy*dy + dz*dz;
                float rsum = ra + rb;
                if (dist2 >= rsum*rsum || dist2 < 1e-12f) continue;

                float dist = sqrtf(dist2);
                float nx = dx / dist, ny = dy / dist, nz = dz / dist;
                float overlap = rsum - dist;

                /* Positional correction: push apart proportional to inverse mass */
                float inva = 1.0f / ma, invb = 1.0f / mb;
                float invsum = inva + invb;
                if (invsum < 1e-12f) continue;
                posa->x -= nx * overlap * (inva / invsum);
                posa->y -= ny * overlap * (inva / invsum);
                posa->z -= nz * overlap * (inva / invsum);
                posb->x += nx * overlap * (invb / invsum);
                posb->y += ny * overlap * (invb / invsum);
                posb->z += nz * overlap * (invb / invsum);

                /* Impulse: relative velocity along the normal */
                if (vela && velb) {
                    float rvx = velb->dx - vela->dx;
                    float rvy = velb->dy - vela->dy;
                    float rvz = velb->dz - vela->dz;
                    float vn = rvx*nx + rvy*ny + rvz*nz;
                    if (vn < 0.0f) {  /* approaching */
                        float e = cfg->restitution;
                        float jimp = -(1.0f + e) * vn / invsum;
                        vela->dx -= jimp * nx * inva;
                        vela->dy -= jimp * ny * inva;
                        vela->dz -= jimp * nz * inva;
                        velb->dx += jimp * nx * invb;
                        velb->dy += jimp * ny * invb;
                        velb->dz += jimp * nz * invb;
                    }
                }
            }
        }
    }
}

/* -- Collision layers (WORLDSIM #567) ---------------------------------- */

int ws_collision_allows(const ws_collision_t *a, const ws_collision_t *b)
{
    if (!a || !b) return 0;
    return (a->collides & b->layer) && (b->collides & a->layer);
}

int ws_query_layer_pairs(const ws_collision_t *bodies, int n,
                         int *out_a, int *out_b, int max_pairs)
{
    if (!bodies || n <= 0 || !out_a || !out_b || max_pairs <= 0) return 0;

    int k = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!ws_collision_allows(&bodies[i], &bodies[j])) continue;
            if (k >= max_pairs) return k;
            out_a[k] = i;
            out_b[k] = j;
            k++;
        }
    }
    return k;
}
