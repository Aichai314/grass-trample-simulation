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

	vec3 const fog_color = vec3(0.1, 0.2, 0.2); // Light gray fog color

	// Dans scene_structure :
	std::vector<ActiveChunk> active_chunks;
	std::unordered_map<ChunkIndex, Chunk, ChunkIndexHash> terrain_memory;
	std::vector<PendingTask> pending_tasks;
	float const chunk_size = 10.0f;
	int const N_chunks = 5; // number of chunks in each direction (total number of chunks = N_chunks^2)
	int const resolution = 64; // resolution of the heightmap (number of pixels per chunk)
	int const N_instances = 80000; // number of grass instances per chunk


	// ****************************** //
	// Callback functions
	// ****************************** //
	void mouse_move_event();
	void mouse_click_event();
	void keyboard_event();
	void idle_frame();

};





