#include "terrain.hpp"

#include "grass.hpp"

#include "utils.hpp"

// Fonction pour créer une texture de hauteur pour un chunk donné
grid_2D<vec3> create_heightmap(float chunk_world_x, float chunk_world_y, float chunk_size, int resolution = 64) 
{
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

	if (distance < chunk_bounding_radius * 2) {
		return true; // The chunk is so close that it can be visible even if it's outside the camera cone
	}
    
    // Tolerance for the closest chunks
    float apparent_size_tolerance = chunk_bounding_radius / distance;

    if (cos_angle < (cos_35_deg - apparent_size_tolerance)) {
        return false;
    }

    return true;
}

void TerrainSystem::initialize() {
    memory.reserve(10000); // Reserve memory for 10 000 chunks (adjust as needed)

	mesh terrain_mesh = mesh_primitive_grid({ -chunk_size/2,-chunk_size/2,0 }, { chunk_size/2,-chunk_size/2,0 },
		{ chunk_size/2,chunk_size/2,0 }, { -chunk_size/2,chunk_size/2,0 }, 50, 50);
	//deform_terrain(terrain_mesh);
	terrain.initialize_data_on_gpu(terrain_mesh);
	terrain.material.color = vec3{79, 53, 7}/255.0f;
	terrain.shader.load(
		project::path + "shaders/terrain/terrain.vert.glsl", 
		project::path + "shaders/terrain/terrain.frag.glsl"
	);
	terrain.material.phong.specular = 0.05f;

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
			memory[index] = { index, create_heightmap(world_pos.x, world_pos.y, chunk_size),
									 create_grass_size_map(world_pos.x, world_pos.y, chunk_size) };
			Chunk* stored_chunk_ptr = &memory[index];
			active_chunks.push_back(ActiveChunk{stored_chunk_ptr, world_pos, initialize_texture(stored_chunk_ptr->data),
												initialize_texture(stored_chunk_ptr->size_data)});
		}
	}
	
	// Set the material color to green
	grass.material.color = {0.3f, 0.8f, 0.3f};
}

void TerrainSystem::update_chunks(vec3 const& barrel_position) {
    // Position exacte du tonneau
    float bx = barrel_position.x;
    float by = barrel_position.y;

    // Coordonnées du chunk le plus proche du tonneau en index
    int center_x = static_cast<int>(std::round(bx / chunk_size));
    int center_y = static_cast<int>(std::round(by / chunk_size));

	int limit = N_chunks / 2; // Distance maximale du centre pour les chunks actifs (en index)

	int const max_gpu_uploads_per_frame = 2; 
    int current_uploads = 0;

	for (auto it = pending_tasks.begin(); it != pending_tasks.end() && current_uploads < max_gpu_uploads_per_frame; ) {
		if (it->future_data.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			Chunk new_chunk = it->future_data.get(); 
            
            // On sauvegarde en mémoire
            memory[new_chunk.index] = new_chunk;

            // On met à jour l'ActiveChunk et sa position
            it->ac->chunk = &memory[new_chunk.index];
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

        // Si le chunk a été déplacé, il faut recalculer son relief
        if (needs_update) {
			vec2 new_world_pos = vec2(index.x * chunk_size, index.y * chunk_size);
			chunk.is_updating = true;
            
			// Check in already computed chunks
            if (memory.find(index) == memory.end()) {
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
				pending_tasks.push_back({&chunk, new_world_pos, std::move(future_chunk)});
			} else {
				// Si déjà en mémoire, on met à jour directement (cas très rapide, pas besoin d'asynchrone)
				chunk.chunk = &memory[index];
				chunk.world_position = new_world_pos;
				chunk.heightmap.update(chunk.chunk->data);
				chunk.grass_size_map.update(chunk.chunk->size_data);
				chunk.is_updating = false;
			}
        }
    }
}

void TerrainSystem::update_grass_trampling(vec3 const& barrel_pos, float crush_radius, vec3 const& barrel_moving_dir, vec3 const& barrel_right_dir) 
{
    // Le rayon du tonneau converti en nombre de "pixels" sur la texture
    float radius_pixels = (crush_radius / chunk_size) * (resolution - 1);
    
    // L'angle du mouvement (en radians)
    float trample_angle = std::atan2(barrel_moving_dir.y, barrel_moving_dir.x);

    for (ActiveChunk& chunk : active_chunks) {
        bool modified_this_frame = false;

        // Convertir la position 3D globale du tonneau en coordonnées de pixels locaux (px, py) pour ce chunk
        float px = ((barrel_pos.x - chunk.world_position.x) / chunk_size + 0.5f) * (resolution - 1);
        float py = ((barrel_pos.y - chunk.world_position.y) / chunk_size + 0.5f) * (resolution - 1);

        // OPTIMISATION : Si la trace du tonneau (px +/- radius) ne touche 
        // pas du tout cette grille (qui va de 0 à resolution-1), on passe au chunk suivant !
        if (px + radius_pixels < 0 || px - radius_pixels >= resolution ||
            py + radius_pixels < 0 || py - radius_pixels >= resolution) {
            continue; 
        }

        // Définir la Bounding Box à modifier
        int x_min = std::max(0, (int)std::floor(px - radius_pixels));
        int x_max = std::min(resolution - 1, (int)std::ceil(px + radius_pixels));
        int y_min = std::max(0, (int)std::floor(py - radius_pixels));
        int y_max = std::min(resolution - 1, (int)std::ceil(py + radius_pixels));

        for (int y = y_min; y <= y_max; ++y) {
            for (int x = x_min; x <= x_max; ++x) {

                vec2 rel_pos = vec2(x - px, y - py); // Position relative du pixel par rapport au centre du tonneau
				float front = dot(rel_pos, barrel_moving_dir.xy());
				float right = dot(rel_pos, barrel_right_dir.xy());
				float custom_squared_dist = 3.5f*front*front + right*right; // Distance personnalisée qui tient compte de la direction du mouvement
				float radius_squared = radius_pixels*radius_pixels;

                // Si on est bien dans l'ellipse du tonneau
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

        // On renvoie les données à la carte graphique UNIQUEMENT si ce chunk a été touché !
        if (modified_this_frame) {
            chunk.heightmap.update(chunk.chunk->data);
        }
    }
}

void TerrainSystem::update_wind(float t, float dt) {
    // Compute wind offset for grass animation
	wind_dir = {std::cos(t / 63.45f), std::sin(t / 49.59f)};
	wind_offset += wind_dir * wind_speed * dt;
	// Use fmod to wrap the wind offset within the wind scale,
	// creating a seemless looping effect and avoiding precision issues with large offsets
	wind_offset.x = std::fmod(wind_offset.x, wind_scale);
	wind_offset.y = std::fmod(wind_offset.y, wind_scale);
}

void TerrainSystem::draw(cgp::environment_generic_structure const& environment, vec3 const& cam_pos, vec3 const& cam_front, float t, bool draw_wireframe) {
    for (const ActiveChunk& chunk : active_chunks) {
		// Culling: Skip drawing this chunk if it's not visible in the camera's view
		if (!is_chunk_visible(cam_pos, cam_front, chunk.world_position, chunk_size)) {
            continue; 
        }

		terrain.model.translation = {chunk.world_position.x, chunk.world_position.y, 0.0f};

		terrain.supplementary_texture["heightmap"] = chunk.heightmap;
		cgp::draw(terrain, environment);
        if (draw_wireframe) {
            cgp::draw_wireframe(terrain, environment);
        }

		grass.supplementary_texture["heightmap"] = chunk.heightmap;
		grass.supplementary_texture["size_map"] = chunk.grass_size_map;
		glUseProgram(grass.shader.id);
		opengl_uniform(grass.shader, "chunk_position", chunk.world_position);
		opengl_uniform(grass.shader, "chunk_size", chunk_size);
		opengl_uniform(grass.shader, "timemodpi", std::fmod(t, 2.0f*Pi));
		opengl_uniform(grass.shader, "wind_dir", wind_dir);
		opengl_uniform(grass.shader, "wind_offset", wind_offset);
    	cgp::draw(grass, environment, N_instances, false);
	}

}