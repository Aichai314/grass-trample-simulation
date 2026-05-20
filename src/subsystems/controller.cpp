#include "controller.hpp"

#include "utils.hpp"

void BarrelController::initialize() {
    mesh barrel_mesh = mesh_load_file_obj(project::path + "assets/barrel/barrel.obj");
	for(vec3 const& p : barrel_mesh.position) {
		float distance_from_center = std::sqrt(p.x*p.x + p.z*p.z); 
		if(distance_from_center > radius) {
			radius = distance_from_center;
		}
	}
	std::cout << "Le rayon exact du tonneau est : " << radius << " metres" << std::endl;

	// Géométrie
	barrel.initialize_data_on_gpu(barrel_mesh);
	// Textures
	barrel.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_baseColor.png");
	barrel.supplementary_texture["normal_map"].load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_normal.png");
	barrel.supplementary_texture["pbr_map"].load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_metallicRoughness.png");
	// Shader personnalisé qui sait lire le PBR
	barrel.shader.load(
		project::path + "shaders/barrel/p_b_r.vert.glsl", 
		project::path + "shaders/barrel/p_b_r.frag.glsl"
	);
	
	barrel.model.translation = {2, 2, get_terrain_height(2,2) + 0.5f};
}

bool BarrelController::update_physics(float dt, cgp::input_devices const& inputs) {
    bool moved = false;

	float eps = radius;
    float bx = barrel.model.translation.x;
    float by = barrel.model.translation.y;
    float h_gauche = get_terrain_height(bx - eps, by);
    float h_droite = get_terrain_height(bx + eps, by);
    float h_bas    = get_terrain_height(bx, by - eps);
    float h_haut   = get_terrain_height(bx, by + eps);
	// Approximate terrain normal using central differences
    vec3 raw_normal = normalize(vec3(h_gauche - h_droite, h_bas - h_haut, 2.0f * eps));
	smoothed_normal = normalize(cgp::interpolation_linear(0.25f, smoothed_normal, raw_normal));

	vec3 barrel_right_dir = -barrel.model.rotation.matrix_col_y(); // direction droite du tonneau	
	vec3 barrel_front_dir = normalize(cross({0,0,1}, barrel_right_dir)); // direction avant du tonneau	

	float acc = g*dot(smoothed_normal, barrel_front_dir) - friction_coeff * vel; // Friction proportionnelle à la vitesse (force de frottement = -k * v)
    if (inputs.keyboard.is_pressed(GLFW_KEY_W) || inputs.keyboard.is_pressed(GLFW_KEY_Z)) {
		acc += base_acc;
    }
    if (inputs.keyboard.is_pressed(GLFW_KEY_S)) {
		acc += -base_acc;
	}

	// Friction statique:
	bool no_input = !(inputs.keyboard.is_pressed(GLFW_KEY_W) || inputs.keyboard.is_pressed(GLFW_KEY_Z) || inputs.keyboard.is_pressed(GLFW_KEY_S));
	if (no_input && std::abs(vel) < 0.2f && std::abs(dot(smoothed_normal, barrel_front_dir)) < 0.05f) {
        vel = 0.0f; // On force l'arrêt complet
        acc = 0.0f; // On annule la micro-gravité résiduelle
    }
	vel += acc * dt;
	if (std::abs(vel) > 0.0001f) {
		barrel.model.translation += barrel_front_dir*vel * dt;
		barrel.model.rotation = rotation_transform::from_axis_angle(barrel_right_dir, -vel * dt / radius) * barrel.model.rotation;
		moved = true;
	}
    if (inputs.keyboard.is_pressed(GLFW_KEY_A) || inputs.keyboard.is_pressed(GLFW_KEY_Q)) {
		barrel.model.rotation = rotation_transform::from_axis_angle({0,0,1}, rotation_speed * dt) * barrel.model.rotation;
		moved = true;
    }
    if (inputs.keyboard.is_pressed(GLFW_KEY_D)) {
		barrel.model.rotation = rotation_transform::from_axis_angle({0,0,1}, -rotation_speed * dt) * barrel.model.rotation;
		moved = true;
	}

    barrel.model.translation.z = get_terrain_height(barrel.model.translation.x, barrel.model.translation.y) + radius;

	// ==========================================
    // Barrel alignment with the terrain normal laterally
    // ==========================================
    
	vec3 barrel_right = -barrel.model.rotation.matrix_col_y();
	vec3 up = vec3(0, 0, 1);
	vec3 barrel_front = normalize(cross(up, barrel_right));
	vec3 barrel_up = normalize(up - dot(up, barrel_right) * barrel_right); // axe "Haut" du tonneau avant correction
	// Projection du normal sur le plan défini par l'axe droit du tonneau
	vec3 proj_normal = normalize(smoothed_normal - dot(smoothed_normal, barrel_front)*barrel_front);

    rotation_transform alignment = rotation_transform::from_frame_transform(barrel_up, barrel_front, proj_normal, barrel_front);
    barrel.model.rotation = alignment * barrel.model.rotation;

	if (vel < 0.0f) {
		moving_dir = -barrel_front_dir; // Inverser la direction avant pour le calcul de l'écrasement
	} else {
		moving_dir = barrel_front_dir;
	}

    return moved;
}

void BarrelController::draw(cgp::environment_generic_structure const& environment, bool wireframe) {
    cgp::draw(barrel, environment);

    if (wireframe) {
        cgp::draw_wireframe(barrel, environment);
    }
}