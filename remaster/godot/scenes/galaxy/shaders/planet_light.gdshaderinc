// Shared by the textured planet shaders: one sun per planet, either a point (its star, galaxy view) or a direction
// (battle sun, system orb). Self-lit and unshaded so Forward+ and Compatibility match and no scene light is needed.
uniform bool from_point = false;
uniform vec3 star_pos = vec3(0.0);
uniform vec3 sun_dir = vec3(0.5, 0.3, 0.8);
uniform vec3 sun_color : source_color = vec3(1.0, 0.95, 0.88);
uniform float sun_energy = 1.35;
uniform float ambient = 0.1; // the turntable rig's fill: the night side reads as a dim disc, not a hole

varying vec3 v_world;

vec3 planet_sun(vec3 world_pos) {
	return normalize(from_point ? star_pos - world_pos : sun_dir);
}

float planet_day(float ndl) {
	return clamp((ndl + 0.12) / 1.12, 0.0, 1.0);
}
