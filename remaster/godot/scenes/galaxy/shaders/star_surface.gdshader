// The star itself at system zoom: a boiling photosphere (granulation from 3D noise) with limb darkening, in HDR so
// the glow picks it up. Forward+ and Compatibility.
shader_type spatial;
render_mode unshaded, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform vec3 star_color : source_color = vec3(1.0, 0.9, 0.7);
uniform float seed = 0.0;

varying vec3 v_local;

void vertex() {
	v_local = normalize(VERTEX);
}

void fragment() {
	vec3 p = v_local * 7.0 + vec3(fract(seed * 0.13) * 30.0);
	float g = g_fbm3(p + vec3(0.0, TIME * 0.12, 0.0));
	float cells = g_noise3(v_local * 26.0 + vec3(TIME * 0.2));
	float mu = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	float limb = mix(0.45, 1.0, pow(mu, 0.45));
	vec3 hot = mix(star_color, vec3(1.0), 0.55);
	vec3 col = mix(star_color * 0.75, hot, smoothstep(0.35, 0.75, g)) * (0.85 + 0.3 * cells);
	ALBEDO = col * limb * 1.45;
}
