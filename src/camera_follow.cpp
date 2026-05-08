#include "camera_follow.hpp"

namespace cgp
{
	void camera_controller_follow::action_mouse_move()
	{
		if (!is_active) return;

		vec2 const& p1 = inputs->mouse.position.current;
		vec2 const& p0 = inputs->mouse.position.previous;
		vec2 const dp = p1 - p0;

		bool const event_valid = !inputs->mouse.on_gui;
		bool const click_left = inputs->mouse.click.left;
		bool const click_right = inputs->mouse.click.right;

		if (event_valid) {
			// Clic gauche : Rotation de la caméra autour de la cible (tonneau)
			if (click_left)     
				camera_model.manipulator_rotate_roll_pitch_yaw(0, dp.y, -dp.x);
			// Clic droit : Rapprocher / Éloigner la caméra
			else if (click_right) 
				camera_model.manipulator_scale_distance_to_center((p1 - p0).y);
		}
	}

	void camera_controller_follow::idle_frame()
	{
		// Rien de spécial ici, le clavier est géré dans le scene.cpp pour le tonneau
	}

	void camera_controller_follow::set_rotation_axis_z()
	{
		camera_model.set_rotation_axis({ 0,0,1 });
	}

	void camera_controller_follow::look_at(vec3 const& eye, vec3 const& center, vec3 const&)
	{
		camera_model.look_at(eye, center);
	}

	// Met à jour dynamiquement le centre de rotation pour suivre l'objet
	void camera_controller_follow::update_target_position(vec3 const& target_position)
	{
		camera_model.center_of_rotation = target_position;
	}

	std::string camera_controller_follow::doc_usage() const
	{
		std::string doc;
		doc += "Camera Controller: Follow\n";
		doc += "  Caméra qui orbite autour d'une cible dynamique.\n";
		doc += "  Mouse left + drag  : Tourner autour de la cible\n";
		doc += "  Mouse right + drag : Zoomer / Dézoomer\n";
		return doc;
	}
}