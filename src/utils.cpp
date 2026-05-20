#include "utils.hpp"

#include "cgp/cgp.hpp"

float get_terrain_height(float x, float y) {
    float height = 0.0f;
    
    // Seulement des grandes collines
    height += 3.0f * cgp::noise_perlin({x * 0.02f, y * 0.02f}, 2);
    
    return height;
}