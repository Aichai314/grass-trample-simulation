#include "grass.hpp"

using namespace cgp;

grass_structure::grass_structure(float height, float radius) : 
    blade_height(height), base_radius(radius) 
{}

mesh grass_structure::create_blade_mesh(int n_seg) const {
    mesh m;
    m.position.resize(n_seg*2+1);
    for (int i=0; i < n_seg; ++i) {
        float factor = static_cast<float>(i) / n_seg;
        float squared_factor = factor*factor;
        m.position[2*i] = {
            base_radius*(1.0f - squared_factor),
            blade_height*squared_factor*bending_ratio,
            blade_height*factor
        };
        m.position[2*i+1] = {
            -base_radius*(1.0f - squared_factor),
            blade_height*squared_factor*bending_ratio,
            blade_height*factor
        };
    }
    m.position[2*n_seg] = {0, blade_height*bending_ratio, blade_height};

    for (int i = 1; i < n_seg; ++i) {
        m.connectivity.push_back(uint3{2*i-2, 2*i, 2*i-1});
        m.connectivity.push_back(uint3{2*i+1, 2*i-1, 2*i});
    }
    m.connectivity.push_back(uint3{2*n_seg-1, 2*n_seg-2, 2*n_seg});

    // Need to call fill_empty_field() before returning the mesh
    //  this function fill all empty buffer with default values (ex. normals, colors, etc).
    m.fill_empty_field();

    return m;
}