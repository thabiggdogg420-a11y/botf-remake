// Plated hero hull (an untextured local model, blender_hero_ships.py bake_plating): Unreal's M_ShipPlated.
// The baked vertex colour "Col" (faction hull colour, underside accent, mottling, AO; linear) x the tiling faction
// plating on UV0 x panel_scale: panel tone R * tone.x + tone.y, seams 1 - seam * G, roughness A (floored), plus its
// normal map. Without the generated plating textures the panels are drawn procedurally at the same scale.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform bool use_vertex_color = true;
uniform vec3 tint = vec3(0.46, 0.48, 0.52);   // linear; the flat hull colour when there is no vertex bake
uniform float color_gain = 1.0;
uniform float panel_scale = 0.35;
uniform vec2 tone = vec2(0.7, 0.65);
uniform float seam = 0.55;
uniform float roughness_min = 0.32;
uniform float metallic = 0.15;
uniform bool has_panels = false;
uniform sampler2D panels_tex : hint_default_white, filter_linear_mipmap_anisotropic, repeat_enable;
uniform bool has_panel_normal = false;
uniform sampler2D panel_normal_tex : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform float panels_per_tile = 8.0;

float h21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

// Stand-in for T_HullPanels: R panel tone, G seam lines, A roughness (one texture tile = panels_per_tile panels).
vec4 procedural_panels(vec2 uv) {
	vec2 q = uv * panels_per_tile * vec2(1.0, 2.0);
	q.x += 0.5 * mod(floor(q.y), 2.0);
	vec2 id = floor(q);
	vec2 f = abs(fract(q) - 0.5);
	float edge = 0.5 - max(f.x, f.y);
	float aa = fwidth(q.x) + fwidth(q.y);
	float line = 1.0 - smoothstep(0.015, 0.015 + aa * 1.5, edge);
	float t = h21(id);
	return vec4(t, line, 0.0, 0.45 + 0.25 * h21(id + 3.7));
}

void fragment() {
	vec2 uv = UV * panel_scale;
	vec4 p = has_panels ? texture(panels_tex, uv) : procedural_panels(uv);
	float shade = (p.r * tone.x + tone.y) * (1.0 - seam * p.g);
	vec3 base = use_vertex_color ? COLOR.rgb : tint;
	ALBEDO = base * shade * color_gain;
	ROUGHNESS = max(p.a, roughness_min);
	METALLIC = metallic;
	if (has_panel_normal) {
		NORMAL_MAP = texture(panel_normal_tex, uv).rgb;
	}
}
