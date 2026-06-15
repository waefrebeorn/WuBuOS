/*
 * wubu_warehouse.h  --  MuJoCo Warehouse World Model
 *
 * Generates MJCF XML for warehouse-scale environments:
 * - Mobile robots (differential drive or omnidirectional)
 * - Shelves, pallets, obstacles
 * - Configurable floor size, robot count, shelf layout
 * - For use with MuJoCo physics backend
 */
#ifndef WUBU_WAREHOUSE_H
#define WUBU_WAREHOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Warehouse configuration */
typedef struct {
    /* Floor dimensions */
    float floor_x;        /* meters */
    float floor_y;
    
    /* Robot configuration */
    int    num_robots;
    float robot_radius;
    float robot_height;
    float robot_mass;
    float wheel_radius;
    float max_force;      /* per wheel */
    
    /* Shelf configuration */
    int    num_shelves;
    float shelf_width;
    float shelf_depth;
    float shelf_height;
    float shelf_mass;
    
    /* Obstacles */
    int    num_obstacles;
    float obstacle_radius;
    float obstacle_height;
    
    /* Physics */
    float timestep;
    float gravity;
    float friction;
} WarehouseConfig;

/* Default warehouse config (20m × 15m, 3 robots, 8 shelves) */
static inline WarehouseConfig warehouse_default_config(void) {
    WarehouseConfig cfg;
    cfg.floor_x = 20.0f;
    cfg.floor_y = 15.0f;
    cfg.num_robots = 3;
    cfg.robot_radius = 0.3f;
    cfg.robot_height = 0.25f;
    cfg.robot_mass = 50.0f;
    cfg.wheel_radius = 0.1f;
    cfg.max_force = 100.0f;
    cfg.num_shelves = 8;
    cfg.shelf_width = 1.2f;
    cfg.shelf_depth = 0.8f;
    cfg.shelf_height = 1.8f;
    cfg.shelf_mass = 200.0f;
    cfg.num_obstacles = 5;
    cfg.obstacle_radius = 0.3f;
    cfg.obstacle_height = 0.5f;
    cfg.timestep = 0.005f;
    cfg.gravity = -9.81f;
    cfg.friction = 0.5f;
    return cfg;
}

/* Generate MJCF XML for warehouse world.
 * Returns malloc'd string, caller must free. */
char* warehouse_build_mjcf(const WarehouseConfig* cfg);

/* Get suggested number of warehouse robots (for array allocation) */
int warehouse_num_robots(const WarehouseConfig* cfg);
int warehouse_num_bodies(const WarehouseConfig* cfg);

/* Robot joint naming convention:
 *   robot_N_slider_x, robot_N_slider_y, robot_N_hinge (steering)
 *   robot_N_wheel_FL, robot_N_wheel_FR, robot_N_wheel_RL, robot_N_wheel_RR
 * Actuator naming:
 *   robot_N_motor_FL, robot_N_motor_FR, robot_N_motor_RL, robot_N_motor_RR
 */

#ifdef __cplusplus
}
#endif

#endif /* WUBU_WAREHOUSE_H */
