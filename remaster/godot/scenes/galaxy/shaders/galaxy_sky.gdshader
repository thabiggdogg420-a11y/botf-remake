// Galactic backdrop for the 3D map: two layers of point stars with colour temperature (one cell lookup per layer,
// no neighbour search, so it is cheap enough for WebGL), the dusty band of the galaxy with dark lanes, and a faint
// warm glow toward the galactic core. Forward+ and Compatibility; 4-octave noise.
shader_type sky;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform vec3 band_normal = vec3(0.18, 0.95, 0.25);
uniform vec3 core_dir = vec3(-0.6, -0.35, -0.7);
uniform float band_strength = 0.55;
uniform float star_strength = 1.0;

vec3 star_tint(float t) {
	return mix(mix(vec3(1.0, 0.66, 0.45), vec3(1.0, 0.96, 0.9), smoothstep(0.0, 0.5, t)), vec3(0.68, 0.8, 1.0), smoothstep(0.5, 1.0, t));
}

vec3 stars(vec3 dir, float scale, float density, float size) {
	vec3 p = dir * scale;
	vec3 cell = floor(p);
	float r = g_hash13(cell);
	if (r > density) {
		return vec3(0.0);
	}
	vec3 sp = cell + 0.25 + 0.5 * vec3(g_hash13(cell + 11.0), g_hash13(cell + 23.0), g_hash13(cell + 37.0));
	float d = length(p - sp);
	float b = g_hash13(cell + 5.0);
	float s = exp(-d * d / (size * size)) * (0.25 + 1.6 * b * b * b);
	return star_tint(g_hash13(cell + 3.0)) * s;
}

void sky() {
	vec3 dir = normalize(EYEDIR);
	float bd = dot(dir, normalize(band_normal));
	float band = exp(-bd * bd * 14.0);
	float dust = g_fbm3(dir * 4.0 + vec3(3.0));
	float lanes = smoothstep(0.42, 0.62, g_fbm3(dir * 9.0 + vec3(7.0)));
	float core = pow(max(dot(dir, normalize(core_dir)), 0.0), 6.0);
	vec3 col = vec3(0.004, 0.005, 0.009);
	vec3 glow = mix(vec3(0.16, 0.15, 0.2), vec3(0.42, 0.32, 0.22), core);
	col += glow * band * (0.35 + 0.65 * dust) * (1.0 - 0.75 * lanes * band) * band_strength * 0.35;
	col += vec3(0.5, 0.36, 0.24) * core * band * 0.12 * band_strength;
	// A faint violet and teal veil off the band.
	float veil = g_fbm3(dir * 2.2 + vec3(11.0));
	col += mix(vec3(0.05, 0.02, 0.08), vec3(0.01, 0.05, 0.07), veil) * smoothstep(0.45, 0.8, veil) * 0.6;
	vec3 st = stars(dir, 140.0, 0.22, 0.09) + stars(dir, 320.0, 0.3 + 0.4 * band, 0.12) * 0.7;
	col += st * star_strength * (1.0 - 0.6 * lanes * band);
	COLOR = col;
}
