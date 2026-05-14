#version 330 core

in struct fragment_data {
    vec3 position; // position in the world space
    vec2 uv;       // current uv-texture on the fragment
} fragment;

uniform mat4 view;       // View matrix (rigid transform) of the camera - to compute the camera position
uniform vec3 fog_color;
uniform float fog_radius;

out vec4 FragColor;

void main() {
    // Compute the position of the center of the camera
	mat3 O = transpose(mat3(view));                   // get the orientation matrix
	vec3 last_col = vec3(view*vec4(0.0, 0.0, 0.0, 1.0)); // get the last column
	vec3 camera_position = -O*last_col;

    vec2 center_uv = fragment.uv - vec2(0.5);
    float dist = length(center_uv);

    float glow = exp(-dist * dist * 20.0);
    glow *= 1.0 - smoothstep(0.3, 0.5, dist);

    // Bright yellow color for the firefly, blended with the fog color based on distance to the camera
    vec3 firefly_color = vec3(1.0, 0.9, 0.2);
    float af = min(length(fragment.position-camera_position)/fog_radius, 1);
	vec3 final_color = (1-af)*firefly_color + af*fog_color;

    FragColor = vec4(final_color, glow);
}