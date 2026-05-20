#include "scene.hpp"
#include "grass.hpp"


using namespace cgp;

// Main initialization function called once at program startup
// Sets up the camera, 3D scene elements, and the image animation system
void scene_structure::initialize()
{
	
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

	std::cout << "End function scene_structure::initialize()" << std::endl;
}

// This function is called permanently at every new frame
// Note that you should avoid having costly computation and large allocation defined there. This function is mostly used to call the draw() functions on pre-existing data.
void scene_structure::display_frame()
{
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
		terrain_system.update_grass_trampling(player.barrel.model.translation, player.crush_radius, player.moving_dir, barrel_right_dir);
	}
	terrain_system.update_chunks(player.barrel.model.translation);
	terrain_system.update_wind(timer.t, dt);

	firefly_swarm.update(dt, player.barrel.model.translation, terrain_system);
	

	// Draw the 3D reference frame axes if enabled
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