/*
 * wubu_warehouse.c  --  MuJoCo Warehouse World Model Implementation
 *
 * Generates MJCF XML for warehouse environments with multiple
 * differential-drive robots, shelves, and obstacles.
 */
#include "wubu_warehouse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

char* warehouse_build_mjcf(const WarehouseConfig* cfg) {
    if (!cfg) return NULL;
    
    /* Estimate buffer: ~10KB + room for bodies */
    size_t sz = 16384 + cfg->num_robots * 4096 + cfg->num_shelves * 512 + cfg->num_obstacles * 256;
    char* xml = (char*)malloc(sz);
    if (!xml) return NULL;
    
    int pos = 0;
    pos += snprintf(xml + pos, sz - pos,
        "<mujoco model=\"warehouse_%.0fx%.0f\">\n"
        "  <compiler angle=\"radian\" autolimits=\"true\" coordinate=\"local\"/>\n"
        "  <option gravity=\"0 0 %f\" timestep=\"%.4f\" iterations=\"100\" tolerance=\"1e-6\"/>\n"
        "  <default>\n"
        "    <geom friction=\"%f .005 .0001\" condim=\"3\"/>\n"
        "    <joint damping=\"0.01\" armature=\"0.001\"/>\n"
        "    <motor ctrllimited=\"true\" ctrlrange=\"-%.0f %.0f\"/>\n"
        "  </default>\n"
        "  <asset>\n"
        "    <texture type=\"skybox\" builtin=\"gradient\" rgb1=\"0.9 0.9 0.9\" rgb2=\"0.5 0.5 0.5\" width=\"800\" height=\"800\"/>\n"
        "    <texture name=\"floor\" type=\"2d\" builtin=\"checker\" rgb1=\"0.8 0.8 0.8\" rgb2=\"0.6 0.6 0.6\" width=\"100\" height=\"100\" mark=\"edge\" markrgb=\"0.5 0.5 0.5\"/>\n"
        "    <material name=\"floor_mat\" texture=\"floor\" texrepeat=\"%d %d\" texuniform=\"true\"/>\n"
        "    <material name=\"robot_mat\" rgba=\"0.2 0.4 0.8 1\"/>\n"
        "    <material name=\"shelf_mat\" rgba=\"0.6 0.4 0.2 1\"/>\n"
        "    <material name=\"obstacle_mat\" rgba=\"0.9 0.2 0.2 1\"/>\n"
        "  </asset>\n"
        "  <worldbody>\n"
        "    <geom name=\"floor\" type=\"plane\" size=\"%.1f %.1f 0.1\" pos=\"0 0 -0.1\" material=\"floor_mat\"/>\n"
        "    <light name=\"sun\" pos=\"0 0 20\" dir=\"0 0 -1\" diffuse=\"0.8 0.8 0.8\" specular=\"0.1 0.1 0.1\"/>\n"
        "    <light name=\"fill\" pos=\"-10 10 5\" dir=\"0.5 -0.5 -0.5\" diffuse=\"0.4 0.4 0.5\"/>\n",
        cfg->floor_x, cfg->floor_y,
        -fabsf(cfg->gravity), cfg->timestep,
        cfg->friction, cfg->max_force, cfg->max_force,
        (int)(cfg->floor_x), (int)(cfg->floor_y),
        cfg->floor_x * 0.5f, cfg->floor_y * 0.5f);
    
    /* -- Robots (differential drive) -- */
    float robot_spacing_x = cfg->floor_x / (cfg->num_robots + 1);
    float robot_spacing_y = cfg->floor_y * 0.3f;
    for (int r = 0; r < cfg->num_robots; ++r) {
        float rx = -cfg->floor_x * 0.5f + (r + 1) * robot_spacing_x;
        float ry = -cfg->floor_y * 0.5f + robot_spacing_y;
        float hw = cfg->robot_radius;
        float hh = cfg->robot_height * 0.5f;
        
        pos += snprintf(xml + pos, sz - pos,
            "    <body name=\"robot_%d\" pos=\"%.2f %.2f %.2f\">\n"
            "      <freejoint/>\n"
            "      <geom name=\"robot_%d_body\" type=\"cylinder\" size=\"%.3f %.3f\" pos=\"0 0 %.3f\" mass=\"%.1f\" material=\"robot_mat\"/>\n"
            "      <geom name=\"robot_%d_top\" type=\"sphere\" size=\"%.3f\" pos=\"0 0 %.3f\" mass=\"0.1\" material=\"robot_mat\"/>\n",
            r, rx, ry, cfg->robot_height,
            r, cfg->robot_radius, hh, hh, cfg->robot_mass,
            r, cfg->robot_radius * 0.8f, cfg->robot_height + cfg->robot_radius * 0.3f);
        
        /* 4 wheels (visual only for now) */
        for (int w = 0; w < 4; ++w) {
            float wx = (w < 2) ? -hw : hw;
            float wy = (w % 2 == 0) ? -hw : hw;
            pos += snprintf(xml + pos, sz - pos,
                "      <geom name=\"robot_%d_wheel_%d\" type=\"cylinder\" size=\"%.3f %.3f\" "
                "pos=\"%.2f %.2f %.3f\" euler=\"0 1.57 0\" mass=\"2.0\" rgba=\"0.1 0.1 0.1 1\"/>\n",
                r, w, cfg->wheel_radius, cfg->wheel_radius * 0.5f, wx, wy, cfg->wheel_radius * 0.3f);
        }
        
        pos += snprintf(xml + pos, sz - pos,
            "    </body>\n");
    }
    
    /* -- Shelves (static obstacles as boxes) -- */
    float shelf_area_x = cfg->floor_x * 0.3f;
    float shelf_area_y = cfg->floor_y * 0.3f;
    for (int s = 0; s < cfg->num_shelves; ++s) {
        float angle = (float)s / cfg->num_shelves * 6.2832f;
        float rad = 2.0f;
        float sx = cosf(angle) * rad;
        float sy = sinf(angle) * rad + cfg->floor_y * 0.2f;
        
        pos += snprintf(xml + pos, sz - pos,
            "    <body name=\"shelf_%d\" pos=\"%.2f %.2f 0\">\n"
            "      <geom name=\"shelf_%d_base\" type=\"box\" size=\"%.2f %.2f %.2f\" pos=\"0 0 %.2f\" "
            "mass=\"%.1f\" material=\"shelf_mat\"/>\n"
            "      <geom name=\"shelf_%d_post1\" type=\"cylinder\" size=\"0.03 %.2f\" pos=\"%.2f %.2f %.2f\" mass=\"5\" material=\"shelf_mat\"/>\n"
            "      <geom name=\"shelf_%d_post2\" type=\"cylinder\" size=\"0.03 %.2f\" pos=\"%.2f %.2f %.2f\" mass=\"5\" material=\"shelf_mat\"/>\n"
            "      <geom name=\"shelf_%d_post3\" type=\"cylinder\" size=\"0.03 %.2f\" pos=\"%.2f %.2f %.2f\" mass=\"5\" material=\"shelf_mat\"/>\n"
            "      <geom name=\"shelf_%d_post4\" type=\"cylinder\" size=\"0.03 %.2f\" pos=\"%.2f %.2f %.2f\" mass=\"5\" material=\"shelf_mat\"/>\n"
            "    </body>\n",
            s, sx, sy,
            s, cfg->shelf_width * 0.5f, cfg->shelf_depth * 0.5f, cfg->shelf_height * 0.5f, cfg->shelf_height * 0.5f, cfg->shelf_mass,
            s, cfg->shelf_height * 0.5f, -cfg->shelf_width * 0.4f, -cfg->shelf_depth * 0.4f, cfg->shelf_height * 0.5f,
            s, cfg->shelf_height * 0.5f, cfg->shelf_width * 0.4f, -cfg->shelf_depth * 0.4f, cfg->shelf_height * 0.5f,
            s, cfg->shelf_height * 0.5f, -cfg->shelf_width * 0.4f, cfg->shelf_depth * 0.4f, cfg->shelf_height * 0.5f,
            s, cfg->shelf_height * 0.5f, cfg->shelf_width * 0.4f, cfg->shelf_depth * 0.4f, cfg->shelf_height * 0.5f);
    }
    
    /* -- Obstacles -- */
    for (int o = 0; o < cfg->num_obstacles; ++o) {
        float ox = -cfg->floor_x * 0.3f + (float)rand() / RAND_MAX * cfg->floor_x * 0.6f;
        float oy = -cfg->floor_y * 0.3f + (float)rand() / RAND_MAX * cfg->floor_y * 0.6f;
        pos += snprintf(xml + pos, sz - pos,
            "    <body name=\"obstacle_%d\" pos=\"%.2f %.2f 0\">\n"
            "      <geom name=\"obstacle_%d_g\" type=\"cylinder\" size=\"%.2f %.2f\" "
            "pos=\"0 0 %.2f\" mass=\"50\" material=\"obstacle_mat\"/>\n"
            "    </body>\n",
            o, ox, oy,
            o, cfg->obstacle_radius, cfg->obstacle_height * 0.5f, cfg->obstacle_height * 0.5f);
    }
    
    pos += snprintf(xml + pos, sz - pos,
        "  </worldbody>\n"
        "</mujoco>\n");
    
    return xml;
}

int warehouse_num_robots(const WarehouseConfig* cfg) {
    return cfg ? cfg->num_robots : 0;
}

int warehouse_num_bodies(const WarehouseConfig* cfg) {
    if (!cfg) return 0;
    /* 1 floor + robots*1 + base + 4 posts + shelves + obstacles */
    return 1 + cfg->num_robots + cfg->num_shelves * 5 + cfg->num_obstacles;
}
