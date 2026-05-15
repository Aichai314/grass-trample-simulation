#pragma once

#include <future>
#include <chrono>

#include "cgp/cgp.hpp"

#include "camera_follow.hpp"

#include "environment.hpp"

using cgp::mesh_drawable;



struct gui_parameters {
	bool display_frame = false;
	bool display_wireframe = false;
};

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

struct Firefly {
    cgp::vec3 position; // According to the center of the swarm
    cgp::vec3 velocity;
    cgp::vec3 target_velocity;
    float time_since_turn;
};

// The structure of the custom scene
struct scene_structure : cgp::scene_inputs_generic {
	
	// ****************************** //
	// Standard Functions
	// ****************************** //

	void initialize();  // Standard initialization to be called before the animation loop
	void display_frame();     // The frame display to be called within the animation loop
	void display_gui(); // The display of the GUI, also called within the animation loop

	void update_chunks();
	void update_grass_trampling(vec3 const& barrel_front_dir, vec3 const& barrel_right_dir);
	void update_fireflies(float dt);

	// ****************************** //
	// Context
	// ****************************** //

	// Environment controller (background color, )
	environment_structure environment; 
	// Window where the scene is displayed
	window_structure window; 
	// Storage for inputs status (mouse, keyboard, window dimension)
	input_devices inputs; 
	// Standard GUI element storage
	gui_parameters gui; 

	// Display information at the start of the program
	void display_info();

	// ****************************** //
	// Camera controller
	// ****************************** //

	// Controller of the camera (extrinsic parameters: position/orientation) -- to be adapted to the desired model and behavior
	camera_controller_follow camera_control; 

	// The model of camera projection (intrinsic parameters)
	camera_projection_perspective camera_projection;

	
	
	// ****************************** //
	// Elements and shapes of the scene
	// ****************************** //

	mesh_drawable global_frame;          // The standard global frame

	timer_basic timer;

	
	mesh_drawable tree;
	mesh_drawable barrel;
	float barrel_radius = 0.0f;
	float const crush_radius = 0.85f;
	mesh_drawable terrain;
	mesh_drawable grass;
	mesh_drawable firefly;

	float const wind_speed = 4.0f; // m/s
	float const wind_scale = 250.0f;
	vec2 wind_offset = {0.0f, 0.0f};
	float vel = 0.0f;
	float const base_acc = 2.0f;
	float const g = 7.5f; // Gravity strength (adjust as needed)
	float const friction_coeff = 0.5f;
	vec3 smoothed_normal = {0.0f, 0.0f, 1.0f}; // Normal lissé pour éviter les micro-mouvements sur les terrains accidentés

	// Dans scene_structure :
	std::vector<ActiveChunk> active_chunks;
	std::unordered_map<ChunkIndex, Chunk, ChunkIndexHash> terrain_memory;
	std::vector<PendingTask> pending_tasks;
	
	float const chunk_size = 10.0f;
	static const int N_chunks = 5; // number of chunks in each direction (total number of chunks = N_chunks^2)
	int const resolution = 64; // resolution of the heightmap (number of pixels per chunk)
	int const N_instances = 80000; // number of grass instances per chunk
	static const int N_fireflies = N_chunks * N_chunks * 10; // number of fireflies in the scene
	std::array<Firefly, N_fireflies> fireflies;
	cgp::numarray<vec3> firefly_positions; // positions des lucioles pour le VBO
	cgp::numarray<vec2> firefly_scales;
	std::unordered_map<int, std::vector<Firefly*>> firefly_distr;
	int const firefly_grid_side = static_cast<int>(N_chunks * chunk_size / 5.0f); // Number of cells on the side of the grid for spatial partitioning
	vec3 fireflies_center = {0.0f, 0.0f, 0.0f}; // Center of the firefly swarm
	vec3 fireflies_velocity = {0.0f, 0.0f, 0.0f}; // Velocity of the firefly swarm
	vec3 fireflies_target_velocity = {0.0f, 0.0f, 0.0f};
	vec3 const fog_color = vec3(0.1, 0.2, 0.2); // Light gray fog color
	bool herd_behavior = false;
	float const fog_radius = (N_chunks/2)*chunk_size; // Distance at which the fog is fully opaque


	// ****************************** //
	// Callback functions
	// ****************************** //
	void mouse_move_event();
	void mouse_click_event();
	void keyboard_event();
	void idle_frame();

};





