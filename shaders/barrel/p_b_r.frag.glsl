#version 330 core 

// Fragment shader - this code is executed for every pixel/fragment that belongs to a displayed shape
//
// Compute the color using Phong illumination (ambient, diffuse, specular) 
//  There is 3 possible input colors:
//    - fragment_data.color: the per-vertex color defined in the mesh
//    - material.color: the uniform color (constant for the whole shape)
//    - image_texture: color coming from the texture image
//  The color considered is the product of: fragment_data.color x material.color x image_texture
//  The alpha (/transparent) channel is obtained as the product of: material.alpha x image_texture.a
// 

// Inputs coming from the vertex shader
in struct fragment_data
{
    vec3 position; // position in the world space
    vec3 normal;   // normal in the world space
    vec3 color;    // current color on the fragment
    vec2 uv;       // current uv-texture on the fragment

} fragment;

// Output of the fragment shader - output color
layout(location=0) out vec4 FragColor;


// Uniform values that must be send from the C++ code
// ***************************************************** //

uniform sampler2D image_texture;   // Texture image identifiant
uniform sampler2D normal_map;
uniform sampler2D pbr_map;

uniform mat4 view;       // View matrix (rigid transform) of the camera - to compute the camera position

uniform vec3 light; // position of the light


// Coefficients of phong illumination model
struct phong_structure {
	float ambient;      
	float diffuse;
	float specular;
	float specular_exponent;
};

// Settings for texture display
struct texture_settings_structure {
	bool use_texture;       // Switch the use of texture on/off
	bool texture_inverse_v; // Reverse the texture in the v component (1-v)
	bool two_sided;         // Display a two-sided illuminated surface (doesn't work on Mac)
};

// Material of the mesh (using a Phong model)
struct material_structure
{
	vec3 color;  // Uniform color of the object
	float alpha; // alpha coefficient

	phong_structure phong;                       // Phong coefficients
	texture_settings_structure texture_settings; // Additional settings for the texture
}; 

uniform material_structure material;


void main()
{
	// Compute the position of the center of the camera
	mat3 O = transpose(mat3(view));                   // get the orientation matrix
	vec3 last_col = vec3(view*vec4(0.0, 0.0, 0.0, 1.0)); // get the last column
	vec3 camera_position = -O*last_col;


	// Renormalize normal
	vec3 N = normalize(fragment.normal);

	// Inverse the normal if it is viewed from its back (two-sided surface)
	//  (note: gl_FrontFacing doesn't work on Mac)
	if (material.texture_settings.two_sided && gl_FrontFacing == false) {
		N = -N;
	}

	// Texture
	// *************************************** //

	// Current uv coordinates
	vec2 uv_image = vec2(fragment.uv.x, fragment.uv.y);
	if(material.texture_settings.texture_inverse_v) {
		uv_image.y = 1.0-uv_image.y;
	}

	// Get the current texture color
	vec4 color_image_texture = texture(image_texture, uv_image);
	if(material.texture_settings.use_texture == false) {
		color_image_texture=vec4(1.0,1.0,1.0,1.0);
	}

    vec4 pbr_data = texture(pbr_map, uv_image);
    float roughness = pbr_data.g; // Canal Vert = Rugosité
    float metallic = pbr_data.b;  // Canal Bleu = Métal

    vec3 n_map = texture(normal_map, uv_image).rgb;
    n_map = n_map * 2.0 - 1.0; // Transformation de [0,1] à [-1,1]
	
	// Compute Shading and normal
	// *************************************** //

	// Calcul du repère TBN (Tangent, Bitangent, Normal)
    // Astuce puissante : On calcule la Tangente dynamiquement sans utiliser de C++ !
    vec3 Q1  = dFdx(fragment.position);
    vec3 Q2  = dFdy(fragment.position);
    vec2 st1 = dFdx(uv_image);
    vec2 st2 = dFdy(uv_image);
    
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = normalize(-Q1*st2.s + Q2*st1.s);
    mat3 TBN = mat3(T, B, N);
    
    // Voici la nouvelle normale avec les détails de relief (clous, bois)
    vec3 perturbed_normal = normalize(TBN * n_map);

    // Calcul de l'éclairage inspiré PBR
    vec3 L = normalize(light - fragment.position);
    vec3 V = normalize(camera_position - fragment.position);
    vec3 H = normalize(L + V); // Vecteur médian pour le reflet

    // Base color
    vec3 albedo = fragment.color * material.color * color_image_texture.rgb;

    // Logique PBR : Un métal pur n'a pas de couleur diffuse, il ne fait que refléter
    vec3 diffuse_color = albedo * (1.0 - metallic);
    
    // Logique PBR : Un isolant a un reflet blanc/gris (0.04), un métal colore son propre reflet
    vec3 specular_color = mix(vec3(0.04), albedo, metallic);

    // La rugosité dicte la netteté du reflet (Shininess)
    float shininess = mix(100.0, 2.0, roughness);

    float NdotL = max(dot(perturbed_normal, L), 0.0);
    float NdotH = max(dot(perturbed_normal, H), 0.0);

    // Termes d'éclairage finaux
    vec3 diffuse_term = diffuse_color * NdotL;
    vec3 specular_term = specular_color * pow(NdotH, shininess) * NdotL;
    vec3 ambient_term = material.phong.ambient * albedo; // On garde un peu de lumière d'ambiance

    vec3 final_color = ambient_term + diffuse_term + specular_term;
    
    FragColor = vec4(final_color, material.alpha * color_image_texture.a);
}