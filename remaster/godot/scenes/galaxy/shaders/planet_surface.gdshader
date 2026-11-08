// Photoreal planet surface (art wave 2, ported from tools/turntable/planet_surface.gdshader): albedo, roughness (oceans
// and ice low: sun glint), tangent normal (desktop tier only), night lights, lava that glows always (class Y).
// Compatibility-safe: no screen or depth textures, no scene lights; the lighting is computed here from the planet's sun
// (planet_light.gdshaderinc). The cloud shell and atmosphere rim are separate shells (planet_cloud / planet_atmo).
shader_type spatial;
render_mode unshaded, fog_disabled, shadows_disabled, cull_back;
#include "res://scenes/galaxy/shaders/planet_light.gdshaderinc"

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D rough_tex : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D emit_tex : source_color, hint_default_black, filter_linear_mipmap, repeat_enable;
uniform bool has_normal = false;
uniform bool emit_always = false;
uniform float emit_energy = 1.4;
uniform float emit_amount = 1.0;   // 0 for an uninhabited world (no city lights); lava ignores it
uniform float rough_scale = 1.0;

void vertex() {
	v_world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec3 albedo = texture(albedo_tex, UV).rgb;
	float rough = clamp(texture(rough_tex, UV).r * rough_scale, 0.06, 1.0);
	vec3 n = NORMAL;
	if (has_normal) {
		vec3 nm = texture(normal_tex, UV).rgb * 2.0 - 1.0;
		n = normalize(TANGENT * nm.x + BINORMAL * nm.y + NORMAL * nm.z);
	}
	vec3 nw = normalize((INV_VIEW_MATRIX * vec4(n, 0.0)).xyz);
	vec3 gw = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	vec3 l = planet_sun(v_world);
	float ndl = dot(nw, l);
	float day = planet_day(ndl) * smoothstep(-0.12, 0.1, dot(gw, l));
	vec3 col = albedo * day * sun_color * sun_energy + albedo * ambient;
	// Glint: normalised Blinn-Phong with the roughness map (Schlick F0 0.04).
	vec3 vw = normalize((INV_VIEW_MATRIX * vec4(VIEW, 0.0)).xyz);
	vec3 hv = normalize(l + vw);
	float a = rough * rough;
	float p = clamp(2.0 / (a * a + 0.0001) - 2.0, 2.0, 600.0);
	float spec = (p + 2.0) / 25.13 * pow(max(dot(nw, hv), 0.0), p) * 0.04 * max(ndl, 0.0);
	col += sun_color * spec * 3.0 * (1.0 - rough * 0.6);
	float night = emit_always ? 1.0 : smoothstep(0.04, -0.14, dot(gw, l));
	col += texture(emit_tex, UV).rgb * emit_energy * night * (emit_always ? 1.0 : emit_amount);
	ALBEDO = col;
}
