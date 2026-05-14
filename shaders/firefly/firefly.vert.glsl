#version 330 core

// Vertex shader - this code is executed for every vertex of the shape

// Inputs coming from VBOs
layout (location = 0) in vec3 vertex_position; // vertex position in local space (x,y,z)
layout (location = 1) in vec3 vertex_normal;   // vertex normal in local space   (nx,ny,nz)
layout (location = 2) in vec3 vertex_color;    // vertex color      (r,g,b)
layout (location = 3) in vec2 vertex_uv;       // vertex uv-texture (u,v)
layout (location = 4) in vec3 instance_position;  // instance position  (x,y,z)
layout (location = 5) in vec2 instance_scale;  // instance scale (y component is unused)

// Output variables sent to the fragment shader
out struct fragment_data
{
    vec3 position; // vertex position in world space
    vec2 uv;       // vertex uv
} fragment;

// Uniform variables expected to receive from the C++ program
uniform mat4 model; // Model affine transform matrix associated to the current shape
uniform mat4 view;  // View matrix (rigid transform) of the camera
uniform mat4 projection; // Projection (perspective or orthogonal) matrix of the camera



void main()
{
	vec4 rotated_vertex = model * vec4(vertex_position, 1.0) * instance_scale.x;
    vec3 world_position = rotated_vertex.xyz + instance_position;
    
    vec4 position_projected = projection * view * vec4(world_position, 1.0);

    // Fill in the output structure
    fragment.position = world_position;
    fragment.uv = vertex_uv;
    
	// gl_Position is a built-in variable which is the expected output of the vertex shader
	gl_Position = position_projected; // gl_Position is the projected vertex position (in normalized device coordinates)
}
