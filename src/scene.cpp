#include "scene.hpp"
#include "grass.hpp"


using namespace cgp;

float get_terrain_height(float x, float y) {
    float height = 0.0f;
    
    // Seulement des grandes collines
    height += 3.0f * noise_perlin({x * 0.02f, y * 0.02f}, 2);
    
    
    return height;
}

// Fonction pour créer une texture de hauteur pour un chunk donné
// On retourne maintenant l'objet complet au lieu du GLuint
grid_2D<vec3> create_heightmap(float chunk_world_x, float chunk_world_y, float chunk_size, int resolution = 64) 
{   
    // On utilise la structure native de CGP : une grille 2D de vec3
    grid_2D<vec3> chunk_data;
    chunk_data.resize(resolution, resolution);

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            float world_x = chunk_world_x + (x / (float)(resolution - 1) - 0.5f) * chunk_size;
            float world_y = chunk_world_y + (y / (float)(resolution - 1) - 0.5f) * chunk_size;
            
            float height = get_terrain_height(world_x, world_y);
            
            // On remplit notre vec3 (Rouge = Hauteur, Vert = Écrasement, Bleu = Vide)
            chunk_data(x, y) = { height, 0.0f, 0.0f }; 
        }
    }

	return chunk_data;
}

grid_2D<vec3> create_grass_size_map(float chunk_world_x, float chunk_world_y, float chunk_size) {
    int res = 20;
    cgp::grid_2D<cgp::vec3> data;
    data.resize(res, res);
    
    for (int y = 0; y < res; ++y) {
        for (int x = 0; x < res; ++x) {
            float world_x = chunk_world_x + (x / (float)(res - 1) - 0.5f) * chunk_size;
            float world_y = chunk_world_y + (y / (float)(res - 1) - 0.5f) * chunk_size;
            
            // Un Perlin Noise doux pour la taille de l'herbe (ex: entre 0.3 et 1.3)
            float size_variation = 0.8f + 0.5f * noise_perlin({world_x * 0.2f, world_y * 0.2f}, 2);
            
            data(x, y) = {size_variation, 0.0f, 0.0f};
        }
    }
    return data;
}

opengl_texture_image_structure initialize_texture(grid_2D<vec3> chunk_data)
{
    opengl_texture_image_structure texture;
    
    // L'appel fonctionne maintenant parfaitement avec la signature de ta bibliothèque !
    texture.initialize_texture_2d_on_gpu(chunk_data, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, false, GL_LINEAR, GL_LINEAR);

    return texture;
}

void scene_structure::update_chunks()
{
    // 1. Position exacte du tonneau
    float bx = barrel.model.translation.x;
    float by = barrel.model.translation.y;

    // 2. Coordonnées du "Centre" logique (le chunk le plus proche du tonneau) en index
    int center_x = static_cast<int>(std::round(bx / chunk_size));
    int center_y = static_cast<int>(std::round(by / chunk_size));

	int limit = N_chunks / 2; // Distance maximale du centre pour les chunks actifs (en index)

	int const max_gpu_uploads_per_frame = 2; 
    int current_uploads = 0;

	for (auto it = pending_tasks.begin(); it != pending_tasks.end() && current_uploads < max_gpu_uploads_per_frame; ) {
		if (it->future_data.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			Chunk new_chunk = it->future_data.get(); 
            
            // On sauvegarde en mémoire
            terrain_memory[new_chunk.index] = new_chunk;

            // On met à jour l'ActiveChunk et sa position
            it->ac->chunk = &terrain_memory[new_chunk.index];
            it->ac->world_position = it->new_world_pos;
            
            // On envoie à la carte graphique (sécurisé sur le Main Thread)
            it->ac->heightmap.update(new_chunk.data);
            it->ac->grass_size_map.update(new_chunk.size_data);
            
            // On libère le chunk
            it->ac->is_updating = false;

            // On supprime la tâche de la liste d'attente
            it = pending_tasks.erase(it);
			++current_uploads;
		} else {
			++it;
		}
	}

    for (ActiveChunk& chunk : active_chunks) {
		// Ignore if chunk is already being updated in parallel
        if (chunk.is_updating) continue;

        bool needs_update = false;
		ChunkIndex index = chunk.chunk->index; // On part de l'index actuel du chunk

        // --- Vérification sur l'axe X ---
        if (chunk.chunk->index.x < center_x - limit) {
            index.x += N_chunks;
            needs_update = true;
        } else if (chunk.chunk->index.x > center_x + limit) {
            index.x -= N_chunks;
            needs_update = true;
        }

        // --- Vérification sur l'axe Y ---
        if (chunk.chunk->index.y < center_y - limit) {
            index.y += N_chunks;
            needs_update = true;
        } else if (chunk.chunk->index.y > center_y + limit) {
            index.y -= N_chunks;
            needs_update = true;
        }

        // --- Mise à jour de la carte graphique ---
        // Si le chunk a été déplacé, il faut recalculer son relief
        if (needs_update) {
			vec2 new_world_pos = vec2(index.x * chunk_size, index.y * chunk_size);
			chunk.is_updating = true;
            // Check in already computed chunks
			
            if (terrain_memory.find(index) == terrain_memory.end()) {
				// Lancement de la tâche asynchrone pour créer le chunk
				float wx = new_world_pos.x;
                float wy = new_world_pos.y;
                float cs = chunk_size;
                int res = resolution;
                ChunkIndex idx = index;

				std::future<Chunk> future_chunk = std::async(std::launch::async, [idx, wx, wy, cs, res]() {
                    return Chunk{ 
                        idx, 
                        create_heightmap(wx, wy, cs, res), 
                        create_grass_size_map(wx, wy, cs) 
                    };
                });
				pending_tasks.push_back({&chunk, index, new_world_pos, std::move(future_chunk)});
			} else {
				// Si déjà en mémoire, on met à jour directement (cas très rapide, pas besoin d'asynchrone)
				chunk.chunk = &terrain_memory[index];
				chunk.world_position = new_world_pos;
				chunk.heightmap.update(chunk.chunk->data);
				chunk.grass_size_map.update(chunk.chunk->size_data);
				chunk.is_updating = false;
			}
        }
    }
}

bool is_chunk_visible(vec3 const& cam_pos, vec3 const& cam_front, vec2 const& chunk_world_pos, float chunk_size) {
    vec3 chunk_center = {chunk_world_pos.x, chunk_world_pos.y, 0.0f};
    vec3 to_chunk = chunk_center - cam_pos;
    float distance = norm(to_chunk);
    float chunk_bounding_radius = chunk_size * 0.75f; // Sphere radius that bounds the chunk
    
    // CONE CULLING
    to_chunk = to_chunk / distance;
    float cos_angle = dot(to_chunk, cam_front);

    // Supposing a 50° field of view (so 25° on each side).
    // We take some margin with 35° to avoid pop-in.
    float const cos_35_deg = 0.819f;

	if (distance < chunk_bounding_radius) {
		return true; // The chunk is so close that it can be visible even if it's outside the camera cone
	}
    
    // Tolerance for the closest chunks
    float apparent_size_tolerance = chunk_bounding_radius / distance;

    if (cos_angle < (cos_35_deg - apparent_size_tolerance)) {
        return false;
    }

    return true;
}

void scene_structure::update_grass_trampling(vec3 const& barrel_moving_dir, vec3 const& barrel_right_dir) 
{
    // Le rayon du tonneau converti en nombre de "pixels" sur la texture
    float radius_pixels = (crush_radius / chunk_size) * (resolution - 1);
    
    // L'angle du mouvement (en radians). Puisqu'on est en GL_RGB32F,
    // on peut stocker des valeurs négatives sans problème !
    float trample_angle = std::atan2(barrel_moving_dir.y, barrel_moving_dir.x);

    for (ActiveChunk& chunk : active_chunks) {
        bool modified_this_frame = false;

        // 1. Convertir la position 3D globale du tonneau en coordonnées de pixels locaux (px, py) pour ce chunk
        float px = ((barrel.model.translation.x - chunk.world_position.x) / chunk_size + 0.5f) * (resolution - 1);
        float py = ((barrel.model.translation.y - chunk.world_position.y) / chunk_size + 0.5f) * (resolution - 1);

        // 2. OPTIMISATION EXTRÊME : Si la trace du tonneau (px +/- radius) ne touche 
        // pas du tout cette grille (qui va de 0 à resolution-1), on passe au chunk suivant !
        if (px + radius_pixels < 0 || px - radius_pixels >= resolution ||
            py + radius_pixels < 0 || py - radius_pixels >= resolution) {
            continue; 
        }

        // 3. Définir le petit carré de pixels à modifier (Bounding Box)
        int x_min = std::max(0, (int)std::floor(px - radius_pixels));
        int x_max = std::min(resolution - 1, (int)std::ceil(px + radius_pixels));
        int y_min = std::max(0, (int)std::floor(py - radius_pixels));
        int y_max = std::min(resolution - 1, (int)std::ceil(py + radius_pixels));

        // 4. On ne boucle QUE sur les pixels sous le tonneau
        for (int y = y_min; y <= y_max; ++y) {
            for (int x = x_min; x <= x_max; ++x) {

                vec2 rel_pos = vec2(x - px, y - py); // Position relative du pixel par rapport au centre du tonneau
				float front = dot(rel_pos, barrel_moving_dir.xy());
				float right = dot(rel_pos, barrel_right_dir.xy());
				float custom_squared_dist = 3.5f*front*front + right*right; // Distance personnalisée qui tient compte de la direction du mouvement
				float radius_squared = radius_pixels*radius_pixels;

                // Si on est bien dans le cercle du tonneau
                if (custom_squared_dist <= radius_squared) {
                    float intensity = 1.0f - custom_squared_dist/radius_squared * custom_squared_dist/radius_squared; // Intensité plus forte au centre, décroissant vers les bords (puissance 4 pour un effet net)

					if (intensity >= chunk.chunk->data(x, y).z) {
						chunk.chunk->data(x, y).y = trample_angle;
	                    chunk.chunk->data(x, y).z = intensity;
						modified_this_frame = true;
					}
                }
            }
        }

        // 5. On renvoie les données à la carte graphique UNIQUEMENT si ce chunk a été touché !
        if (modified_this_frame) {
            chunk.heightmap.update(chunk.chunk->data);
        }
    }
}

void scene_structure::update_fireflies(float dt) {
    float const planar_speed = 0.5f;
    float const max_turn_angle = Pi / 6.0f; // Max 30 degrés de virage par seconde

	if (norm(fireflies_center - vec3{barrel.model.translation.x, barrel.model.translation.y, 0.0f}) > fog_radius/10.0f) {
		fireflies_target_velocity = (vec3{barrel.model.translation.x, barrel.model.translation.y, 0.0f} - fireflies_center)/(fog_radius/10.0f);
	} else {
		fireflies_target_velocity = vec3{0.0f, 0.0f, 0.0f};
	}
	fireflies_velocity = interpolation_linear(10.0f/fog_radius * dt, fireflies_velocity, fireflies_target_velocity);
	fireflies_center += fireflies_velocity * dt;
    
    for(Firefly& f : fireflies) {
        f.time_since_turn += dt;

        // 1. CHANGER DE CIBLE TOUTES LES SECONDES
        if (f.time_since_turn > 1.0f) {
            f.time_since_turn = cgp::rand_uniform(0.0f, 0.5f); // On ajoute un peu de chaos au chrono
            
            // On calcule l'angle actuel de la luciole dans le plan XY
            float current_angle = std::atan2(f.velocity.y, f.velocity.x);
            
            // On ajoute un virage aléatoire "centré sur 0" (entre -90° et +90°)
            float random_turn = cgp::rand_uniform(-max_turn_angle, max_turn_angle);
			float heigh_variation = cgp::rand_uniform(-0.1f, 0.1f); // Variation d'altitude douce
            float new_angle = current_angle + random_turn;
            
            // On définit la nouvelle vitesse cible
            f.target_velocity = {
                std::cos(new_angle) * planar_speed,
                std::sin(new_angle) * planar_speed,
                f.velocity.z + heigh_variation
            };
        }
		// ------------------ Stabalize altitude ----------------
        float ground_h = get_terrain_height(f.position.x + fireflies_center.x, f.position.y + fireflies_center.y);
        float min_h = ground_h + 0.6f;
        float max_h = ground_h + 3.0f;
        
        if (f.position.z < min_h) {
            f.target_velocity.z = (min_h - f.position.z) * 2.0f; 
        } else if (f.position.z > max_h) {
            f.target_velocity.z = (max_h - f.position.z) * 1.0f; // Gentler push downwards 
        }

		// ------------------ Stay in the fog area ----------------
        vec2 to_center = { -f.position.x, -f.position.y };
		float dist_to_center = norm(to_center);
        float recall_radius = fog_radius - 5.0f;

        if (dist_to_center > recall_radius) {
            
            to_center = to_center / dist_to_center;
            
            // Ideal direct return velocity towards the center
            vec2 direct_return_vel = to_center * planar_speed * (dist_to_center - recall_radius);

            float return_force = (dist_to_center - recall_radius) * 1.5f * dt;
            // We ensure that the lerp coeff doen't exceed 1.0f
            return_force = std::min(return_force, 1.0f); 

            f.target_velocity.x = interpolation_linear(return_force, f.target_velocity.x, direct_return_vel.x);
            f.target_velocity.y = interpolation_linear(return_force, f.target_velocity.y, direct_return_vel.y);
        } else {
			// We normalize the xy target_velocity to maintain a constant planar speed, while keeping the z component free for altitude adjustments
			vec2 xy_vel = {f.target_velocity.x, f.target_velocity.y};
			xy_vel = normalize(xy_vel) * planar_speed;
			f.target_velocity.x = xy_vel.x;
			f.target_velocity.y = xy_vel.y;
		}

        float inertia = 2.5f; // Vitesse à laquelle elle tourne vers sa cible
        f.velocity = interpolation_linear(inertia * dt, f.velocity, f.target_velocity);

		// ------------------- Repulsion from barrel ----------------
		vec3 to_barrel = barrel.model.translation - (f.position + fireflies_center);
        float dist_to_barrel = cgp::norm(to_barrel);
        if (dist_to_barrel < 1.5f) {
            to_barrel = to_barrel / dist_to_barrel;
            float repulsion_strength = planar_speed / (dist_to_barrel) * 8.0f; // Stronger repulsion when closer
            // We add a velocity change that pushes the firefly away from the barrel
            f.velocity -= to_barrel * repulsion_strength * dt;
        }

        f.position += f.velocity * dt;
    }
	for (int i = 0; i < N_fireflies; ++i) {
		firefly_positions[i] = fireflies[i].position + fireflies_center;
	}
	firefly.update_supplementary_data_on_gpu(firefly_positions, 4);
}

// Main initialization function called once at program startup
// Sets up the camera, 3D scene elements, and the image animation system
void scene_structure::initialize()
{
	
	std::cout << "Start function scene_structure::initialize()" << std::endl;

	// Set the behavior of the camera and its initial position
	// ********************************************** //
	camera_control.initialize(inputs, window); 
	camera_control.set_rotation_axis_z(); // camera rotates around z-axis

	camera_projection = camera_projection_perspective{
		50.0f * Pi/180, // Field of view
		1.0f,           // Aspect ratio
		0.01f,          // Depth min
		1000            // Depth max
	};


	// General information
	display_info();

	// Create 3D coordinate frame (x, y, z axes) for visual reference
	global_frame.initialize_data_on_gpu(mesh_primitive_frame());

	// Initialize the shapes of the scene
	// ***************************************** //

	gui.display_frame = true;

	terrain_memory.reserve(10000); // Reserve memory for 10 000 chunks (adjust as needed)

	mesh terrain_mesh = mesh_primitive_grid({ -chunk_size/2,-chunk_size/2,0 }, { chunk_size/2,-chunk_size/2,0 },
		{ chunk_size/2,chunk_size/2,0 }, { -chunk_size/2,chunk_size/2,0 }, 50, 50);
	//deform_terrain(terrain_mesh);
	terrain.initialize_data_on_gpu(terrain_mesh);
	terrain.material.color = vec3{79, 53, 7}/255.0f;
	terrain.shader.load(
		project::path + "shaders/terrain/terrain.vert.glsl", 
		project::path + "shaders/terrain/terrain.frag.glsl"
	);
	grass.material.phong.specular = 0.05f;

	// tree.initialize_data_on_gpu(mesh_load_file_obj(project::path + "assets/palm_tree/palm_tree.obj"));
	// tree.model.rotation = rotation_transform::from_axis_angle({ 1,0,0 }, Pi / 2.0f);
	// tree.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/palm_tree/palm_tree.jpg", GL_REPEAT, GL_REPEAT);
	// tree.model.translation = {0,0, get_terrain_height(0, 0)};

	mesh barrel_mesh = mesh_load_file_obj(project::path + "assets/barrel/barrel.obj");
	for(vec3 const& p : barrel_mesh.position) {
		float distance_from_center = std::sqrt(p.x*p.x + p.z*p.z); 
		if(distance_from_center > barrel_radius) {
			barrel_radius = distance_from_center;
		}
	}
	std::cout << "Le rayon exact du tonneau est : " << barrel_radius << " metres" << std::endl;

	// 1. Géométrie
	barrel.initialize_data_on_gpu(barrel_mesh);
	// 2. Textures
	barrel.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_baseColor.png");
	barrel.supplementary_texture["normal_map"].load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_normal.png");
	barrel.supplementary_texture["pbr_map"].load_and_initialize_texture_2d_on_gpu(project::path + "assets/barrel/textures/barrel_metallicRoughness.png");
	// 3. Shader personnalisé qui sait lire le PBR
	barrel.shader.load(
		project::path + "shaders/barrel/p_b_r.vert.glsl", 
		project::path + "shaders/barrel/p_b_r.frag.glsl"
	);
	
	barrel.model.translation = {2, 2, get_terrain_height(2,2) + 0.5f};
	//   look_at(camera_position, targeted_point, up_direction)
	camera_control.look_at(
		barrel.model.translation + vec3{-5,0,2} /* position of the camera in the 3D scene */,
		barrel.model.translation /* targeted point in 3D scene */,
		{0,0,1} /* direction of the "up" vector */);
	
	mesh quad_mesh = mesh_primitive_quadrangle({ -0.5f,0,0 }, { 0.5f,0,0 }, { 0.5f,0,1 }, { -0.5f,0,1 });
	firefly.initialize_data_on_gpu(quad_mesh);
	firefly.model.scaling = 0.2f;
	firefly.shader.load(project::path + "shaders/firefly/firefly.vert.glsl", project::path + "shaders/firefly/firefly.frag.glsl");
	firefly_positions.resize(N_fireflies);
	firefly_scales.resize(N_fireflies);
	float const spawn_radius = fog_radius * 0.7f;
	for (int i=0; i < N_fireflies; ++i) {
		fireflies[i].position = { cgp::rand_uniform(-spawn_radius, spawn_radius), cgp::rand_uniform(-spawn_radius, spawn_radius), cgp::rand_uniform(0.5f, 2.0f) };
		fireflies[i].position.z += get_terrain_height(fireflies[i].position.x, fireflies[i].position.y);
		firefly_positions[i] = fireflies[i].position; // On remplit le tableau de positions pour le VBO
		fireflies[i].velocity = { cgp::rand_uniform(-1.0f, 1.0f), cgp::rand_uniform(-1.0f, 1.0f), cgp::rand_uniform(-0.5f, 0.5f) };
		fireflies[i].target_velocity = fireflies[i].velocity;
		fireflies[i].time_since_turn = cgp::rand_uniform(0.0f, 1.0f);
		firefly_scales[i] = {cgp::rand_uniform(0.4f, 1.2f), 0};
	}
	firefly.initialize_supplementary_data_on_gpu(firefly_positions, /*location*/ 4, /*divisor: 1=per instance, 0=per vertex*/ 1);
	firefly.initialize_supplementary_data_on_gpu(firefly_scales, /*location*/ 5, /*divisor: 1=per instance, 0=per vertex*/ 1);

	grass_structure grass_struct = grass_structure(0.4f, 0.025f);
	
	// Load the custom shader BEFORE initializing GPU data so VBO attributes bind correctly
	grass.shader.load(project::path + "shaders/grass/grass.vert.glsl", project::path + "shaders/grass/grass.frag.glsl");
	
	grass.initialize_data_on_gpu(grass_struct.create_blade_mesh(4));
	grass.supplementary_texture["windmap"].load_and_initialize_texture_2d_on_gpu(project::path + "assets/perlin_512.png", GL_REPEAT, GL_REPEAT);
	grass.material.phong.specular = 0.05f;
	grass.material.phong.specular_exponent = 10.0f;
	grass.material.texture_settings.two_sided = true;

	// Add a per-instance vertex attribute for the position
	numarray<vec3> instance_positions(N_instances);
	numarray<vec4> yxz_rotations_and_scaling(N_instances);
	for(int i=0; i < N_instances; ++i) {
		float x = rand_uniform(-chunk_size/2, chunk_size/2);
		float y = rand_uniform(-chunk_size/2, chunk_size/2);
		instance_positions[i] = { x, y, 0};
		yxz_rotations_and_scaling[i] = {rand_uniform(-Pi/6, Pi/6), rand_uniform(-Pi/6, 0), rand_uniform(-Pi, Pi), rand_uniform(0.6f, 1.4f)};
	}
	// Send these positions as a new buffer of data to the shader
	grass.initialize_supplementary_data_on_gpu(instance_positions, /*location*/ 4, /*divisor: 1=per instance, 0=per vertex*/ 1);
	grass.initialize_supplementary_data_on_gpu(yxz_rotations_and_scaling, /*location*/ 5, /*divisor: 1=per instance, 0=per vertex*/ 1);

	for (int i=0; i < N_chunks; ++i) {
		for (int j=0; j < N_chunks; ++j) {
			ChunkIndex index = {i - N_chunks / 2, j - N_chunks / 2};
			vec2 world_pos = { index.x * chunk_size, index.y * chunk_size };
			terrain_memory[index] = { index, create_heightmap(world_pos.x, world_pos.y, chunk_size),
									 create_grass_size_map(world_pos.x, world_pos.y, chunk_size) };
			Chunk* stored_chunk_ptr = &terrain_memory[index];
			active_chunks.push_back(ActiveChunk{stored_chunk_ptr, world_pos, initialize_texture(stored_chunk_ptr->data),
												initialize_texture(stored_chunk_ptr->size_data)});
		}
	}
	
	// Set the material color to green
	grass.material.color = {0.3f, 0.8f, 0.3f};

	std::cout << "End function scene_structure::initialize()" << std::endl;
}

// This function is called permanently at every new frame
// Note that you should avoid having costly computation and large allocation defined there. This function is mostly used to call the draw() functions on pre-existing data.
void scene_structure::display_frame()
{
    camera_projection.aspect_ratio = window.aspect_ratio();

	// Compute movement of the barrel with keyboard inputs
	float const rotation_speed = Pi/2; // en radians par seconde
    float dt = timer.update();
	bool moved = false;

	float eps = barrel_radius;
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
		barrel.model.rotation = rotation_transform::from_axis_angle(barrel_right_dir, -vel * dt / barrel_radius) * barrel.model.rotation;
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

    barrel.model.translation.z = get_terrain_height(barrel.model.translation.x, barrel.model.translation.y) + barrel_radius;

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
    // ==========================================

	if (moved) {
		if (vel < 0.0f) {
			barrel_front_dir = -barrel_front_dir; // Inverser la direction avant pour le calcul de l'écrasement
		}
		update_grass_trampling(barrel_front_dir, barrel_right_dir);
	}
	update_chunks();

    camera_control.update_target_position(barrel.model.translation);

	// Compute wind offset for grass animation
	vec2 wind_dir = {std::cos(timer.t / 63.45f), std::sin(timer.t / 49.59f)};
	wind_offset += wind_dir * wind_speed * dt;
	// Use fmod to wrap the wind offset within the wind scale,
	// creating a seemless looping effect and avoiding precision issues with large offsets
	wind_offset.x = std::fmod(wind_offset.x, wind_scale);
	wind_offset.y = std::fmod(wind_offset.y, wind_scale);

	environment.camera_projection = camera_projection.matrix();
	environment.camera_view = camera_control.camera_model.matrix_view();
	environment.light = camera_control.camera_model.position();
	environment.background_color = fog_color;
	environment.uniform_generic.uniform_vec3["fog_color"] = fog_color;
	environment.uniform_generic.uniform_float["fog_radius"] = fog_radius;
	

	// Draw the 3D reference frame axes if enabled
	if (gui.display_frame)
		draw(global_frame, environment);

	// Draw all the shapes
	//draw(tree, environment);
	draw(barrel, environment);

	for (const ActiveChunk& chunk : active_chunks) {
		// Culling: Skip drawing this chunk if it's not visible in the camera's view
		if (!is_chunk_visible(camera_control.camera_model.position(), camera_control.camera_model.front(),
		chunk.world_position, chunk_size)) {
            continue; 
        }

		terrain.model.translation = {chunk.world_position.x, chunk.world_position.y, 0.0f};

		terrain.supplementary_texture["heightmap"] = chunk.heightmap;
		draw(terrain, environment);

		grass.supplementary_texture["heightmap"] = chunk.heightmap;
		grass.supplementary_texture["size_map"] = chunk.grass_size_map;
		glUseProgram(grass.shader.id);
		opengl_uniform(grass.shader, "chunk_position", chunk.world_position);
		opengl_uniform(grass.shader, "chunk_size", chunk_size);
		opengl_uniform(grass.shader, "timemodpi", std::fmod(timer.t, 2.0f*Pi));
		opengl_uniform(grass.shader, "wind_dir", wind_dir);
		opengl_uniform(grass.shader, "wind_offset", wind_offset);
    	draw(grass, environment, N_instances, false);
	}

	update_fireflies(dt);
	// Enable use of alpha component as color blending for transparent elements
	//  alpha = current_color.alpha
	//  new color = previous_color * alpha + current_color
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	// Disable depth buffer writing
	//  - Transparent elements cannot use depth buffer
	//  - They are supposed to be displayed from farthest to nearest elements but
	//  here it is not necessary since we simply add colors
	glDepthMask(false);

	auto const& camera = camera_control.camera_model;

	// Re-orient the firefly shape to always face the camera direction
	vec3 const cam_right = camera.right();
	vec3 const cam_up = camera.up();
	// Rotation such that the firefly follows the right-vector of the camera, while pointing toward the z-direction
	rotation_transform R = rotation_transform::from_frame_transform({ 1,0,0 }, { 0,0,1 }, cam_right, cam_up);
	firefly.model.rotation = R;
	draw(firefly, environment, N_fireflies, false);

	// Don't forget to re-activate the depth-buffer write
	glDepthMask(true);
	glDisable(GL_BLEND);

	if (gui.display_wireframe) {
		draw_wireframe(terrain, environment);
		// draw_wireframe(tree, environment);
		draw_wireframe(barrel, environment);
	}

}


void scene_structure::display_gui()
{
	ImGui::Checkbox("Frame", &gui.display_frame);
	ImGui::Checkbox("Wireframe", &gui.display_wireframe);
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
	std::cout << "Example of scene to start a project." << std::endl;
	std::cout << "-----------------------------------------------\n" << std::endl;
}