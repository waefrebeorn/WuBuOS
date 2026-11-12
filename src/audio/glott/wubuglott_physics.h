/*
 * wubuglott_physics.h -- physical constants for airway acoustics.
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/physics.rs.
 * CGS throughout: cm, g, s. Pressure in dyn/cm^2 (= 0.1 Pa); volume
 * velocity in cm^3/s.
 *
 * Header-only constants, no code.
 */
#ifndef WUBUGLOTT_PHYSICS_H
#define WUBUGLOTT_PHYSICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Air density at body temperature, saturated (g/cm^3). */
#define WG_RHO      1.14e-3
/* Speed of sound in warm humid air (cm/s). */
#define WG_C_SOUND  35000.0
/* Dynamic viscosity of air (poise = g/(cm*s)). */
#define WG_MU       1.86e-4
/* Kinematic viscosity (cm^2/s). */
#define WG_NU       (WG_MU / WG_RHO)
/* rho*c^2 -- bulk modulus of air (dyn/cm^2). */
#define WG_RHO_C2   (WG_RHO * WG_C_SOUND * WG_C_SOUND)
/* 1 cm H2O in dyn/cm^2. */
#define WG_CM_H2O   980.665

/* Critical Reynolds number for onset of turbulence noise. */
#define WG_RE_CRIT  1800.0

/* Soft-tissue wall parameters per unit area (Ishizaka, French & Flanagan
 * 1975, cheek/neck): mass (g/cm^2), resistance (dyn*s/cm^3), stiffness
 * (dyn/cm^3). */
typedef struct {
    double mass;
    double resistance;
    double stiffness;
} wg_wall_params;

#define WG_WALL_SOFT   ((wg_wall_params){ 1.5, 1600.0, 3.0e5 })
#define WG_WALL_TRACHEA ((wg_wall_params){ 0.6, 1000.0, 1.0e6 })
#define WG_WALL_NASAL  ((wg_wall_params){ 1.0, 3000.0, 1.0e6 })

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_PHYSICS_H */
