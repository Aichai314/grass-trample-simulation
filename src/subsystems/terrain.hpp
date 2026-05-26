#pragma once

#include <future>
#include <chrono>

#include "cgp/cgp.hpp"

#include "environment.hpp"

struct ChunkIndex {
    int x, y;
    bool operator==(const ChunkIndex& other) const {
        return x == other.x && y == other.y;
    }
};

// Fonction de hachage pour ChunkIndex
struct ChunkIndexHash {
    size_t operator()(const ChunkIndex& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

struct Chunk {
    ChunkIndex index;
    cgp::grid_2D<cgp::vec3> data;
	cgp::grid_2D<cgp::vec3> size_data; // Currently stores grass height on red channel
};

struct ActiveChunk {
	Chunk* chunk;
	vec2 world_position; // Position du centre du chunk dans le monde
	cgp::opengl_texture_image_structure heightmap;
	cgp::opengl_texture_image_structure grass_size_map;
	bool is_updating = false; // Flag for parralell update
};

struct PendingTask {
    ActiveChunk* ac;
    ChunkIndex new_index;
    vec2 new_world_pos;
    std::future<Chunk> future_data;
};

struct TerrainSystem {
    mesh_drawable terrain;
    mesh_drawable grass;
    
    float const wind_speed = 4.0f; // m/s
	float const wind_scale = 250.0f;
	vec2 wind_offset = {0.0f, 0.0f};
    vec2 wind_dir = {1.0f, 0.0f};
    float const chunk_size = 10.0f;
    static const int N_chunks = 5; // number of chunks in each direction (total number of chunks = N_chunks^2)
    int const resolution = 64; // resolution of the heightmap (number of pixels per chunk)
    int const N_instances = 80000; // number of grass instances per chunk

    vec3 const fog_color = vec3(0.1, 0.2, 0.2); // Light gray fog color
	float const fog_radius = (N_chunks/2)*chunk_size; // Distance at which the fog is fully opaque

    std::vector<ActiveChunk> active_chunks;
    std::unordered_map<ChunkIndex, Chunk, ChunkIndexHash> memory;
    std::vector<PendingTask> pending_tasks;

    void initialize();
    void update_chunks(vec3 const& barrel_position);
    void update_grass_trampling(vec3 const& barrel_pos, float crush_radius, vec3 const& barrel_moving_dir, vec3 const& barrel_right_dir);
    // Le draw prendra le vent et la position de la caméra pour le Frustum Culling
    void update_wind(float t, float dt);
    void draw(cgp::environment_generic_structure const& environment, vec3 const& cam_pos, vec3 const& cam_front, float t, bool draw_wireframe);
};