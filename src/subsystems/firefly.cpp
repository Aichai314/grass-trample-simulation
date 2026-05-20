#include "firefly.hpp"

#include "utils.hpp"

void FireflySwarm::initialize(TerrainSystem const& terrain) {
    N_fireflies = terrain.N_chunks * terrain.N_chunks * 10;
    grid_side = static_cast<int>(terrain.N_chunks * terrain.chunk_size / 5.0f); // On divise le terrain en cellules de 5m x 5m pour la répartition spatiale

    mesh quad_mesh = mesh_primitive_quadrangle({ -0.5f,0,0 }, { 0.5f,0,0 }, { 0.5f,0,1 }, { -0.5f,0,1 });
	firefly.initialize_data_on_gpu(quad_mesh);
	firefly.model.scaling = 0.2f;
	firefly.shader.load(project::path + "shaders/firefly/firefly.vert.glsl", project::path + "shaders/firefly/firefly.frag.glsl");
	fireflies.resize(N_fireflies);
    positions.resize(N_fireflies);
	scales.resize(N_fireflies);
    float spawn_radius = terrain.fog_radius * 0.7f;
	for (int i=0; i < N_fireflies; ++i) {
		fireflies[i].position = { cgp::rand_uniform(-spawn_radius, spawn_radius), cgp::rand_uniform(-spawn_radius, spawn_radius), cgp::rand_uniform(0.5f, 2.0f) };
		fireflies[i].position.z += get_terrain_height(fireflies[i].position.x, fireflies[i].position.y);
		positions[i] = fireflies[i].position; // On remplit le tableau de positions pour le VBO
		fireflies[i].velocity = { cgp::rand_uniform(-1.0f, 1.0f), cgp::rand_uniform(-1.0f, 1.0f), cgp::rand_uniform(-0.5f, 0.5f) };
		fireflies[i].target_velocity = fireflies[i].velocity;
		fireflies[i].time_since_turn = cgp::rand_uniform(0.0f, 1.0f);
		scales[i] = {cgp::rand_uniform(0.4f, 1.2f), 0};
	}
	distribution.reserve(grid_side * grid_side); // On réserve de la place pour des cellules de 5m x 5m
	firefly.initialize_supplementary_data_on_gpu(positions, /*location*/ 4, /*divisor: 1=per instance, 0=per vertex*/ 1);
	firefly.initialize_supplementary_data_on_gpu(scales, /*location*/ 5, /*divisor: 1=per instance, 0=per vertex*/ 1);
}

void FireflySwarm::update(float dt, vec3 const& barrel_pos, TerrainSystem const& terrain) {
    float const planar_speed = 0.5f;
    float const max_turn_angle = Pi / 6.0f; // Max 30 degrés de virage par seconde
	vec3 to_barrel = vec3{barrel_pos.x, barrel_pos.y, 0.0f} - center;
	float dist_to_barrel = norm(to_barrel);
	if (dist_to_barrel > terrain.fog_radius/2.0f) {
		target_velocity = normalize(to_barrel)*(dist_to_barrel-terrain.fog_radius/2.0f + 4.0f)/(terrain.fog_radius/20.0f);
	} else {
		target_velocity = vec3{0.0f, 0.0f, 0.0f};
	}
	velocity = interpolation_linear(10.0f/terrain.fog_radius * dt, velocity, target_velocity);
	center += velocity * dt;

	
	if (herd_behavior) {
		// On vide les vecteurs mais on garde la mémoire allouée
		for (auto& pair : distribution) {
			pair.second.clear(); 
		}
		for (Firefly& f : fireflies) {
			int cell_x = static_cast<int>((f.position.x + terrain.N_chunks*terrain.chunk_size/2.0f) / (5.0f));
			int cell_y = static_cast<int>((f.position.y + terrain.N_chunks*terrain.chunk_size/2.0f) / (5.0f));
			// This prevents the creation on unnecessary cells for out-of-bound fireflies
			// (should never happen since they are recalled before, but just in case)
			cell_x = std::clamp(cell_x, 0, grid_side - 1);
			cell_y = std::clamp(cell_y, 0, grid_side - 1);

			int cell_index = cell_y * grid_side + cell_x;
			distribution[cell_index].push_back(&f);
		}
	}
    
    for(Firefly& f : fireflies) {
        f.time_since_turn += dt;

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
        float ground_h = get_terrain_height(f.position.x + center.x, f.position.y + center.y);
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
		vec3 to_barrel = barrel_pos - (f.position + center);
        float dist_to_barrel = norm(to_barrel);
        float recall_radius = terrain.fog_radius - 7.0f;

        if (dist_to_center > recall_radius) {
            
            to_center = to_center / dist_to_center;
            
            // Ideal direct return velocity towards the center
            vec2 direct_return_vel = to_center * planar_speed * (dist_to_center - recall_radius + 1);

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

        // ==========================================
        // DYNAMIQUE D'ESSAIM (BOIDS de Reynolds)
        // ==========================================
		if (herd_behavior) {
			vec3 separation_steer = {0.0f, 0.0f, 0.0f};
			vec3 alignment_steer  = {0.0f, 0.0f, 0.0f};
			vec3 cohesion_center  = {0.0f, 0.0f, 0.0f};
			int cohesion_count = 0;
			int alignment_count = 0;

			// Les 3 rayons imbriqués
			float const cohesion_radius = 4.0f;
			float const alignment_radius = 2.0f;
			float const separation_radius = 0.3f;
			float const w_separation = 1.0f; // Très fort : vital pour ne pas former un paquet informe
			float const w_alignment  = 0.6f; // Moyen : crée un effet de "courant" fluide
			float const w_cohesion   = 0.3f; // Faible : laisse les lucioles respirer et errer
			float const panic_zone = 3.0f;

			int cell_x = static_cast<int>((f.position.x + terrain.N_chunks*terrain.chunk_size/2.0f) / (5.0f));
			int cell_y = static_cast<int>((f.position.y + terrain.N_chunks*terrain.chunk_size/2.0f) / (5.0f));
			std::vector<int> cells_to_check;
			for (int dx = -1; dx <= 1; ++dx) {
				for (int dy = -1; dy <= 1; ++dy) {
					int nx = cell_x + dx;
					int ny = cell_y + dy;
					if (nx >= 0 && nx < grid_side && ny >= 0 && ny < grid_side) {
						cells_to_check.push_back(ny * grid_side + nx);
					}
				}
			}
			for (int cell_index : cells_to_check) {
				if (distribution.find(cell_index) == distribution.end()) {
					continue;
				}
				for (Firefly* const other : distribution[cell_index]) {
					// Ne pas se comparer avec soi-même !
					if (&f == other) continue; 

					vec3 diff = f.position - other->position;
					float dist = cgp::norm(diff);

					// Si l'autre luciole est dans mon champ de vision
					if (dist < cohesion_radius) {
						cohesion_count++;
						cohesion_center += other->position;
						
						if (dist < alignment_radius) {
							alignment_steer += other->velocity;
							alignment_count++;

							if (dist < separation_radius && dist > 0.001f) {
								separation_steer += (diff / (dist * dist)); 
							}
						}
					}
				}
			}

			float panic = 0.0f;
			if (dist_to_barrel < panic_zone) {
				panic = (panic_zone - dist_to_barrel) / panic_zone;
				panic = std::min(panic, 1.0f);
			}

			if (cohesion_count > 0) {
				// Moyennes
				cohesion_center = cohesion_center / (float)cohesion_count;
				if (alignment_count > 0) alignment_steer = alignment_steer / (float)alignment_count;
				
				// Calcul du vecteur de direction pour la cohésion
				vec3 cohesion_steer = cohesion_center - f.position;

				// Normalisation pour obtenir des forces pures
				if (norm(alignment_steer) > 0.001f)  alignment_steer  = normalize(alignment_steer) * planar_speed;
				if (norm(cohesion_steer) > 0.001f)   cohesion_steer   = normalize(cohesion_steer) * planar_speed;
				if (norm(separation_steer) > 0.001f) separation_steer = normalize(separation_steer) * planar_speed;

				float current_w_alignment = w_alignment * (1.0f - panic);
				float current_w_cohesion  = w_cohesion  * (1.0f - panic);
				float current_w_separation = w_separation + (panic * 2.0f);

				// On applique ces comportements comme des forces sur la vélocité actuelle
				f.velocity += (separation_steer * current_w_separation 
							+ alignment_steer * current_w_alignment 
							+ cohesion_steer * current_w_cohesion) * dt;
			}

			if (dist_to_center > recall_radius) {
				vec2 to_center_dir = normalize(to_center);
				vec3 tangential_dir = vec3{-to_center_dir.y, to_center_dir.x, 0.0f}; // Perpendicular in the XY plane
				float tangential_reduction = std::min(1.0f, (dist_to_center - recall_radius) / 5.0f);
				f.velocity -= tangential_dir * dot(f.velocity, tangential_dir) * tangential_reduction; // Réduction de la composante tangentielle pour éviter les trajectoires en spirale
			}
		}

		// ------------------- Repulsion from barrel ----------------
        if (dist_to_barrel < 1.5f) {
            to_barrel = to_barrel / dist_to_barrel;
            float repulsion_strength = planar_speed / (dist_to_barrel) * 8.0f; // Stronger repulsion when closer
            // We add a velocity change that pushes the firefly away from the barrel
            f.velocity -= to_barrel * repulsion_strength * dt;
        }

        f.position += f.velocity * dt;
    }
	for (int i = 0; i < N_fireflies; ++i) {
		positions[i] = fireflies[i].position + center;
	}
	firefly.update_supplementary_data_on_gpu(positions, 4);
}

void FireflySwarm::draw(cgp::environment_generic_structure const& environment, vec3 const& cam_right, vec3 const& cam_up) {
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
    
    // Rotation such that the firefly follows the right-vector of the camera, while pointing toward the z-direction
	rotation_transform R = rotation_transform::from_frame_transform({ 1,0,0 }, { 0,0,1 }, cam_right, cam_up);
	
    // Re-orient the firefly shape to always face the camera direction
    firefly.model.rotation = R;
	
    cgp::draw(firefly, environment, N_fireflies, false);

	// Don't forget to re-activate the depth-buffer write
	glDepthMask(true);
	glDisable(GL_BLEND);
}