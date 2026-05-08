#pragma once
#include "cgp/01_base/base.hpp"
#include "cgp/19_camera_controller/camera_controller_generic_base/camera_controller_generic_base.hpp"

namespace cgp
{
	struct camera_controller_follow : camera_controller_generic_base
	{
		camera_orbit_euler camera_model;

		void set_rotation_axis_z();
		void look_at(vec3 const& eye, vec3 const& center, vec3 const& unused = vec3());

		// La fonction clé pour suivre le tonneau
		void update_target_position(vec3 const& target_position);

		void action_mouse_move();
		void idle_frame();

		std::string doc_usage() const;
	};
}