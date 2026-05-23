#include "scene.hpp"
#include "grass.hpp"
#include <chrono>


using namespace cgp;

// Main initialization function called once at program startup
// Sets up the camera, 3D scene elements, and the image animation system
void scene_structure::initialize()
{
	auto init_start = std::chrono::high_resolution_clock::now();
	
	std::cout << "Start function scene_structure::initialize()" << std::endl;

	// General information
	display_info();

	// Create 3D coordinate frame (x, y, z axes) for visual reference
	global_frame.initialize_data_on_gpu(mesh_primitive_frame());

	// Initialize the shapes of the scene
	// ***************************************** //

	gui.display_frame = true;

	// ======== INITIALIZE SUBSYSTEMS ======== //
	player.initialize();
	terrain_system.initialize();
	firefly_swarm.initialize(terrain_system);

	// ======== CAMERA INITIALIZATION ======== //
	camera_control.initialize(inputs, window); 
	camera_control.set_rotation_axis_z(); // camera rotates around z-axis

	camera_projection = camera_projection_perspective{
		50.0f * Pi/180, // Field of view
		1.0f,           // Aspect ratio
		0.01f,          // Depth min
		1000            // Depth max
	};
	camera_control.look_at(
		player.barrel.model.translation + vec3{-5,0,2} /* position of the camera in the 3D scene */,
		player.barrel.model.translation /* targeted point in 3D scene */,
		{0,0,1} /* direction of the "up" vector */
	);

	auto init_end = std::chrono::high_resolution_clock::now();
	auto init_duration = std::chrono::duration<double, std::milli>(init_end - init_start).count();
	
	std::cout << "End function scene_structure::initialize()" << std::endl;
	std::cout << "[TIMING] Initialization completed in " << init_duration << " ms" << std::endl;
}

// This function is called permanently at every new frame
// Note that you should avoid having costly computation and large allocation defined there. This function is mostly used to call the draw() functions on pre-existing data.
void scene_structure::display_frame()
{
	auto frame_start = std::chrono::high_resolution_clock::now();
	
    camera_projection.aspect_ratio = window.aspect_ratio();
	float dt = timer.update();

	// Update the environment information (camera position, light position, background color, etc.) that will be sent to the shaders
	environment.camera_projection = camera_projection.matrix();
	environment.camera_view = camera_control.camera_model.matrix_view();
	environment.light = camera_control.camera_model.position();
	environment.background_color = terrain_system.fog_color;
	environment.uniform_generic.uniform_vec3["fog_color"] = terrain_system.fog_color;
	environment.uniform_generic.uniform_float["fog_radius"] = terrain_system.fog_radius;

	// ====== UPDTATE SUBSYSTEMS ====== //
	bool moved = player.update_physics(dt, inputs);
	camera_control.update_target_position(player.barrel.model.translation);

	if (moved) {
		vec3 barrel_right_dir = -player.barrel.model.rotation.matrix_col_y();
		auto grass_update_start = std::chrono::high_resolution_clock::now();
		terrain_system.update_grass_trampling(player.barrel.model.translation, player.crush_radius, player.moving_dir, barrel_right_dir);
		auto grass_update_end = std::chrono::high_resolution_clock::now();
		accumulated_grass_update_time += std::chrono::duration<double, std::milli>(grass_update_end - grass_update_start).count();
	}
	
	auto chunks_update_start = std::chrono::high_resolution_clock::now();
	terrain_system.update_chunks(player.barrel.model.translation);
	auto chunks_update_end = std::chrono::high_resolution_clock::now();
	double chunks_update_time = std::chrono::duration<double, std::milli>(chunks_update_end - chunks_update_start).count();
	accumulated_chunks_update_time += chunks_update_time;
	
	if (chunks_update_time > 3.0) {
		std::cout << "[WARNING] Chunks update time exceeded threshold: " << chunks_update_time << " ms" << std::endl;
	}
	
	terrain_system.update_wind(timer.t, dt);

	auto firefly_update_start = std::chrono::high_resolution_clock::now();
	firefly_swarm.update(dt, player.barrel.model.translation, terrain_system);
	auto firefly_update_end = std::chrono::high_resolution_clock::now();
	accumulated_firefly_update_time += std::chrono::duration<double, std::milli>(firefly_update_end - firefly_update_start).count();

	// Draw the 3D reference frame axes if enabled
	auto draw_start = std::chrono::high_resolution_clock::now();
	
	if (gui.display_frame)
		draw(global_frame, environment);

	// ========= DRAW SUBSYSTEMS ======== //
	auto const& camera = camera_control.camera_model;
	vec3 const cam_pos = camera.position();
	vec3 const cam_front = camera.front();
	vec3 const cam_right = camera.right();
	vec3 const cam_up = camera.up();

	player.draw(environment, gui.display_wireframe);
	terrain_system.draw(environment, cam_pos, cam_front, timer.t, gui.display_wireframe);
	firefly_swarm.draw(environment, cam_right, cam_up);
	
	auto draw_end = std::chrono::high_resolution_clock::now();
	accumulated_draw_time += std::chrono::duration<double, std::milli>(draw_end - draw_start).count();
	
	// Measure GPU synchronization time
	auto gpu_wait_start = std::chrono::high_resolution_clock::now();
	glFinish(); // Force GPU to complete all pending commands
	auto gpu_wait_end = std::chrono::high_resolution_clock::now();
	double gpu_wait_time = std::chrono::duration<double, std::milli>(gpu_wait_end - gpu_wait_start).count();
	accumulated_gpu_wait_time += gpu_wait_time;

	// Frame timing and statistics
	auto frame_end = std::chrono::high_resolution_clock::now();
	auto frame_duration = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
	accumulated_frame_time += frame_duration;
	
	frame_count_since_reset++;
	time_since_last_stats_print += dt;
	
	// Print statistics every 20 seconds
	if (time_since_last_stats_print >= STATS_PRINT_INTERVAL) {
		double avg_chunks_time = accumulated_chunks_update_time / frame_count_since_reset;
		double avg_grass_time = accumulated_grass_update_time / frame_count_since_reset;
		double avg_firefly_time = accumulated_firefly_update_time / frame_count_since_reset;
		double avg_draw_time = accumulated_draw_time / frame_count_since_reset;
		double avg_gpu_wait_time = accumulated_gpu_wait_time / frame_count_since_reset;
		double avg_frame_time = accumulated_frame_time / frame_count_since_reset;
		
		std::cout << "\n[TIMING STATISTICS - Averaged over " << frame_count_since_reset << " frames (" 
				  << time_since_last_stats_print << " seconds)]" << std::endl;
		std::cout << "  Chunks update time:     " << avg_chunks_time << " ms" << std::endl;
		std::cout << "  Grass update time:      " << avg_grass_time << " ms" << std::endl;
		std::cout << "  Firefly update time:    " << avg_firefly_time << " ms" << std::endl;
		std::cout << "  GPU drawing time:       " << avg_draw_time << " ms" << std::endl;
		std::cout << "  GPU wait time:          " << avg_gpu_wait_time << " ms" << std::endl;
		std::cout << "  Total frame time:       " << avg_frame_time << " ms" << std::endl;
		std::cout << "  Estimated FPS:          " << (1000.0 / avg_frame_time) << " fps" << std::endl;
		
		// Reset accumulators
		accumulated_chunks_update_time = 0.0;
		accumulated_grass_update_time = 0.0;
		accumulated_firefly_update_time = 0.0;
		accumulated_draw_time = 0.0;
		accumulated_gpu_wait_time = 0.0;
		accumulated_frame_time = 0.0;
		frame_count_since_reset = 0;
		time_since_last_stats_print = 0.0;
	}
}


void scene_structure::display_gui()
{
	ImGui::Checkbox("Frame", &gui.display_frame);
	ImGui::Checkbox("Wireframe", &gui.display_wireframe);
	ImGui::Checkbox("Herd behavior for fireflies", &firefly_swarm.herd_behavior);
}




void scene_structure::mouse_move_event()
{
	if (!inputs.keyboard.shift)
		camera_control.action_mouse_move();
	
}
void scene_structure::mouse_click_event()
{
	camera_control.action_mouse_click();
}
void scene_structure::keyboard_event()
{
	camera_control.action_keyboard();
}
void scene_structure::idle_frame()
{
	camera_control.idle_frame();
	
}

void scene_structure::display_info()
{
	std::cout << "\nCAMERA CONTROL:" << std::endl;
	std::cout << "-----------------------------------------------" << std::endl;
	std::cout << camera_control.doc_usage() << std::endl;
	std::cout << "-----------------------------------------------\n" << std::endl;


	std::cout << "\nSCENE INFO:" << std::endl;
	std::cout << "-----------------------------------------------" << std::endl;
	std::cout << "Grass trampling project" << std::endl;
	std::cout << "-----------------------------------------------\n" << std::endl;
}