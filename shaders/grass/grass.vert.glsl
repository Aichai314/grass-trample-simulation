#version 330 core

// Vertex shader - this code is executed for every vertex of the shape

// Inputs coming from VBOs
layout (location = 0) in vec3 vertex_position; // vertex position in local space (x,y,z)
layout (location = 1) in vec3 vertex_normal;   // vertex normal in local space   (nx,ny,nz)
layout (location = 2) in vec3 vertex_color;    // vertex color      (r,g,b)
layout (location = 3) in vec2 vertex_uv;       // vertex uv-texture (u,v)
layout (location = 4) in vec3 instance_position;  // instance position  (x,y,z)
layout (location = 5) in vec4 yxz_rotation_and_scaling;  // instance position  (x,y,z)

// Output variables sent to the fragment shader
out struct fragment_data
{
    vec3 position; // vertex position in world space
    vec3 normal;   // normal position in world space
    vec3 color;    // vertex color
    vec2 uv;       // vertex uv
} fragment;

// Uniform variables expected to receive from the C++ program
uniform mat4 model; // Model affine transform matrix associated to the current shape
uniform mat4 view;  // View matrix (rigid transform) of the camera
uniform mat4 projection; // Projection (perspective or orthogonal) matrix of the camera
uniform sampler2D heightmap; // Texture containing the heightmap of the terrain (used to displace the grass blades)
uniform sampler2D size_map; // Texture containing the size variation of the grass blades (used to scale the grass blades)
uniform sampler2D windmap; // Texture containing the wind information (used to bend the grass blades)
uniform float time;
uniform vec2 chunk_position; // position of the chunk in the world (used to displace the grass blades)
uniform float chunk_size; // size of the chunk (used to displace the grass blades)

// Fonction de rotation d'un vecteur 'v' autour d'un axe normalisé 'axis' d'un angle 'theta'
vec3 rotate_axis_angle(vec3 v, vec3 axis, float theta) {
    float c = cos(theta);
    float s = sin(theta);
    // Formule de Rodrigues
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

void main()
{
	float top = 0.5; // The maximum height of the grass blade (in local space)

    // Apply scaling and YXZ rotations in the correct order
    float size_variation = texture(size_map, vec2(instance_position.x/chunk_size + 0.5, instance_position.y/chunk_size + 0.5)).r;
    vec3 pos = vertex_position*yxz_rotation_and_scaling.w*size_variation; // Apply scaling to the vertex position
    vec3 n = vertex_normal;
    
    // Rotation around Y
    float cy = cos(yxz_rotation_and_scaling.x);
    float sy = sin(yxz_rotation_and_scaling.x);
    pos = vec3(pos.x * cy + pos.z * sy, pos.y, -pos.x * sy + pos.z * cy);
    n = vec3(n.x * cy + n.z * sy, n.y, -n.x * sy + n.z * cy);
    
    // Rotation around X
    float cx = cos(yxz_rotation_and_scaling.y);
    float sx = sin(yxz_rotation_and_scaling.y);
    pos = vec3(pos.x, pos.y * cx - pos.z * sx, pos.y * sx + pos.z * cx);
    n = vec3(n.x, n.y * cx - n.z * sx, n.y * sx + n.z * cx);
    
    // Rotation around Z
    float cz = cos(yxz_rotation_and_scaling.z);
    float sz = sin(yxz_rotation_and_scaling.z);
    pos = vec3(pos.x * cz - pos.y * sz, pos.x * sz + pos.y * cz, pos.z);
    n = vec3(n.x * cz - n.y * sz, n.x * sz + n.y * cz, n.z);

    vec3 map_data = texture(heightmap, vec2(instance_position.x/chunk_size + 0.5, instance_position.y/chunk_size + 0.5)).rgb;
    float altitude = map_data.r;
    float angle = map_data.g;
    float intensity = map_data.b;

    // Bend the grass blade according to the angle and intensity of the trample
    // Approximate the bending by a simple linear deformation of the blade, where the bending is
    // stronger at the top of the blade and weaker at the bottom (to keep the blade attached to the ground)
    fragment.color = vec3(0.4, 0.5, 0.4) + vec3(0.3, 0.25, 0.3) * (pos.z / top);
    vec2 bend_dir = vec2(cos(angle), sin(angle));
    pos.xy += bend_dir * pos.z * intensity;
    n.xy += bend_dir * intensity;
    pos.z  -= pos.z * intensity * 0.75;
    n.z += intensity;
    // No need to re-normalize the normal vector since the frag shader already does it

    fragment.color += vec3(0.3, 0.25, 0.3) * (pos.z / top); // Make the grass blade darker when trampled

    /* Wind bending */
    // ---------------
    vec2 world_pos = chunk_position + instance_position.xy;

    // 2. Paramétrage de la rafale de vent
    float wind_scale = 250.0; // Une image de bruit couvrira 250 mètres de terrain
    vec2 wind_dir = vec2(cos(time/63.45), sin(time/49.59)); // Direction du vent
    float wind_speed = 4; // Vitesse de déplacement du vent en m/s

    // 3. Calcul des UVs "Monde" qui glissent avec le temps
    // On divise world_pos par wind_scale pour étirer la texture sur plusieurs chunks
    vec2 wind_uv = (world_pos + wind_dir * time * wind_speed) / wind_scale;

    // 4. Lecture de la force du vent (entre 0.0 et 1.0)
    float wind_force = texture(windmap, wind_uv).r;

    // 5. Application
    float bend_amount = (wind_force * wind_force) * 2 * (pos.z / top);

    // On applique la courbure du vent par-dessus la courbure de l'écrasement
    vec3 wind_axis = normalize(cross(vec3(0.0, 0.0, 1.0), vec3(wind_dir, 0.0)));
    pos = rotate_axis_angle(pos, wind_axis, bend_amount);
    n   = rotate_axis_angle(n, wind_axis, bend_amount);

    // random oscillation to add some more life to the grass blades
    float oscillation = sin(time * 3.67 + instance_position.x * 472.26 + instance_position.y * 797.18) * 0.04;
    pos += oscillation * bend_amount * n;

    // The position of the vertex in the world space - Add the offset related to the current instance
    vec4 position = model * vec4(pos + instance_position + vec3(chunk_position, altitude), 1.0);

	// The normal of the vertex in the world space
	mat4 modelNormal = transpose(inverse(model));
	vec4 normal = modelNormal * vec4(n, 0.0);

	// The projected position of the vertex in the normalized device coordinates:
	vec4 position_projected = projection * view * position;

	// Fill the parameters sent to the fragment shader
	fragment.position = position.xyz;
	fragment.normal   = normal.xyz;
	// fragment.color already set above
	fragment.uv = vertex_uv;

	// gl_Position is a built-in variable which is the expected output of the vertex shader
	gl_Position = position_projected; // gl_Position is the projected vertex position (in normalized device coordinates)
}