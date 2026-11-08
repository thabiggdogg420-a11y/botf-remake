// Star impostor: a camera-facing glow sized in screen space (never under min_px, never over max_px) so every star
// reads at galaxy zoom and does not flood the screen at system zoom. Colour and spike strength follow the star type.
// When the real sphere is large on screen (system zoom), the white core fades and the glow becomes a corona.
// Forward+ and Compatibility: unshaded, additive, no textures.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, skip_vertex_transform, fog_disabled, shadows_disabled;

uniform vec3 star_color : source_color = vec3(1.0, 0.9, 0.7);
uniform float world_radius = 0.32;
uniform float sphere_radius = 0.045;
uniform float min_px = 9.0;
uniform float max_px = 64.0;
uniform float brightness = 1.0;
uniform float spike = 1.0;
uniform float twinkle_seed = 0.0;

varying float v_sphere;

void vertex() {
	vec4 c = MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0);
	float dist = max(-c.z, 0.001);
	float ppu = VIEWPORT_SIZE.y * PROJECTION_MATRIX[1][1] * 0.5 / dist;
	float sphere_px = sphere_radius * ppu;
	v_sphere = smoothstep(5.0, 16.0, sphere_px);
	float r_px = max(clamp(world_radius * ppu, min_px, max_px), sphere_px * 3.2);
	float r = r_px / ppu;
	VERTEX = c.xyz + vec3(VERTEX.xy * 2.0 * r, 0.0);
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	float d = length(p);
	float tw = 1.0 + 0.05 * sin(TIME * 2.1 + twinkle_seed * 7.0);
	float core = exp(-d * d * 90.0) * 3.2 * (1.0 - v_sphere);
	float halo = (exp(-d * 5.5) * 0.55 + exp(-d * d * 14.0) * 0.45) * (1.0 - 0.6 * v_sphere);
	float spikes = (exp(-abs(p.x) * 70.0) + exp(-abs(p.y) * 70.0)) * (1.0 - smoothstep(0.0, 0.95, d)) * 0.5 * spike * (1.0 - v_sphere);
	float edge = 1.0 - smoothstep(0.75, 1.0, d);
	vec3 col = star_color * (halo + spikes) + mix(vec3(1.0), star_color, 0.25) * core;
	ALBEDO = col * edge * brightness * tw;
}
