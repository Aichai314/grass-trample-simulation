#pragma once

#include "cgp/cgp.hpp"

#include "environment.hpp"

#include "terrain.hpp"

struct Firefly {
    cgp::vec3 position; // According to the center of the swarm
    cgp::vec3 velocity;
    cgp::vec3 target_velocity;
    float time_since_turn;
};

struct FireflySwarm {
    mesh_drawable firefly;

    int N_fireflies;
    
    std::vector<Firefly> fireflies;
    cgp::numarray<vec3> positions; 
    cgp::numarray<vec2> scales;
    std::unordered_map<int, std::vector<Firefly*>> distribution; // Spatial partitioning of fireflies for optimization (cell index -> list of fireflies in the cell)
    
    int grid_side; // Number of cells on the side of the grid for spatial partitioning 
    vec3 center = {0.0f, 0.0f, 0.0f}; // Center of the swarm as a whole (used for following the barrel)
    vec3 velocity = {0.0f, 0.0f, 0.0f}; // Velocity of the swarm as a whole (used to follow the barrel)
    vec3 target_velocity = {0.0f, 0.0f, 0.0f}; // Target velocity of the swarm as a whole (used to follow the barrel)
    bool herd_behavior = false;

    void initialize(TerrainSystem const& terrain);
    // L'update prend en paramètre ce qu'il a besoin de savoir du reste du monde
    void update(float dt, vec3 const& barrel_pos, TerrainSystem const& terrain);
    void draw(cgp::environment_generic_structure const& environment, vec3 const& cam_right, vec3 const& cam_up);
};