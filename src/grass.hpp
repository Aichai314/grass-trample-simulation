#pragma once

#include "cgp/cgp.hpp"

struct grass_structure {
	float blade_height = 0.7f;
	float base_radius = 0.1f;
	float bending_ratio = 1.0f / 4;

    grass_structure(float height, float radius);
	cgp::mesh create_blade_mesh(int n_seg) const;
};