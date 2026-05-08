#version 330 core

// Toutes les données envoyées par ton C++
layout (location = 0) in vec3 vertex_position;
layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec3 vertex_color;
layout (location = 3) in vec2 vertex_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D heightmap;

// La fameuse structure exigée par ton Fragment Shader !
out struct fragment_data
{
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
} fragment;

void main()
{
    // 1. On lit l'altitude
    float altitude = texture(heightmap, vertex_uv).r;
    
    // 2. On l'applique pour soulever le point
    vec3 displaced_position = vec3(vertex_position.x, vertex_position.y, altitude);
    vec4 position_world = model * vec4(displaced_position, 1.0);
    
    // 3. On remplit TOUTES les cases de la structure pour le Fragment Shader
    fragment.position = position_world.xyz;
    fragment.normal = vertex_normal; // (Sera recalculée dynamiquement dans le fragment shader)
    fragment.color = vertex_color;
    fragment.uv = vertex_uv;
    
    gl_Position = projection * view * position_world;
}