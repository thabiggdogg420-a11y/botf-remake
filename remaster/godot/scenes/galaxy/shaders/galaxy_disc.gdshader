// The galactic disc under the charted quadrant: unresolved star clouds with dark dust lanes, brightest toward the
// core side, fading out past the charted edge. Additive, faint, so territory and fog stay readable on top.
// Forward+ and Compatibility.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform vec2 cells = vec2(16.0, 12.0);
uniform vec2 core_dir = vec2(-0.7, -0.5);
uniform float strength = 1.0;

void fragment() {
	vec2 p = (UV - 0.5) * 3.0;          // the plane spans three grids; p in grid units, |p| <= 1.5
	vec2 world = p * cells;
	float r = length(p * vec2(1.0, cells.y / cells.x));
	float fade = 1.0 - smoothstep(0.35, 1.25, r);
	float arm = g_fbm2(world * 0.11 + vec2(2.0, 5.0));
	float clouds = g_fbm2(world * 0.35 + vec2(arm * 3.0));
	float lanes = smoothstep(0.48, 0.66, g_fbm2(world * 0.22 + vec2(9.0, 1.0)));
	float core = clamp(dot(normalize(p + 0.0001), normalize(core_dir)) * 0.5 + 0.5, 0.0, 1.0);
	vec3 warm = vec3(0.32, 0.25, 0.18);
	vec3 cool = vec3(0.12, 0.15, 0.24);
	vec3 col = mix(cool, warm, core * 0.8) * (0.25 + 0.75 * clouds) * smoothstep(0.3, 0.7, arm + 0.15);
	col *= 1.0 - 0.8 * lanes;
	ALBEDO = col * fade * strength * 0.32;
}
