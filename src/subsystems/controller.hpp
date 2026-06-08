#pragma once

#include "cgp/cgp.hpp"

#include "environment.hpp"

struct BarrelController {
    mesh_drawable barrel;
    float radius = 0.0f;
    float const crush_radius = 0.85f;
    
    // Physique
    float vel = 0.0f;
    float const rotation_speed = Pi/2; // en radians par seconde
    float const base_acc = 2.5f;
    float const g = 7.5f; 
    float const friction_coeff = 0.5f;
    vec3 smoothed_normal = {0.0f, 0.0f, 1.0f};
    vec3 moving_dir; // Direction de déplacement actuelle du tonneau

    void initialize();
    bool update_physics(float dt, cgp::input_devices const& inputs);
    void draw(cgp::environment_generic_structure const& environment, bool wireframe) const;
};