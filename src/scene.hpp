#pragma once

#include "cgp/cgp.hpp"

#include "camera_follow.hpp"

#include "environment.hpp"

#include "subsystems/controller.hpp"
#include "subsystems/terrain.hpp"
#include "subsystems/firefly.hpp"

using cgp::mesh_drawable;



struct gui_parameters {
	bool display_frame = false;
	bool display_wireframe = false;
};

// The structure of the custom scene
struct scene_structure : cgp::scene_inputs_generic {
	
	// ****************************** //
	// Standard Functions
	// ****************************** //

	void initialize();  // Standard initialization to be called before the animation loop
	void display_frame();     // The frame display to be called within the animation loop
	void display_gui(); // The display of the GUI, also called within the animation loop

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

	
	BarrelController player;
    TerrainSystem terrain_system;
    FireflySwarm firefly_swarm;

	// ****************************** //
	// Timing statistics
	// ****************************** //
	// Accumulated frame times (in milliseconds)
	double accumulated_chunks_update_time = 0.0;
	double accumulated_grass_update_time = 0.0;
	double accumulated_firefly_update_time = 0.0;
	double accumulated_draw_time = 0.0;
	double accumulated_gpu_wait_time = 0.0;
	double accumulated_frame_time = 0.0;
	
	// Counters and tracking
	int frame_count_since_reset = 0;
	double time_since_last_stats_print = 0.0;
	const double STATS_PRINT_INTERVAL = 20.0; // Print statistics every 20 seconds

	// ****************************** //
	// Callback functions
	// ****************************** //
	void mouse_move_event();
	void mouse_click_event();
	void keyboard_event();
	void idle_frame();

};





