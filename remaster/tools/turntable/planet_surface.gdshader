// L9 planet look for the turntable (reference for the game's planet material). Compatibility-safe: no screen or depth
// textures, standard render modes only. Albedo + roughness map (oceans and ice low: sun glint) + tangent normal.
// Night-side lights show only where the surface faces away from the sun (sun_dir, world space); emit_always keeps lava
// (planet Y) glowing on both renderers.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D rough_tex : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D emit_tex : source_color, hint_default_black, filter_linear_mipmap, repeat_enable;
uniform bool has_normal = false;
uniform bool emit_always = false;
uniform float emit_energy = 1.4;
uniform float rough_scale = 1.0;
uniform vec3 sun_dir = vec3(0.5, 0.3, 0.8);

void fragment() {
	ALBEDO = texture(albedo_tex, UV).rgb;
	ROUGHNESS = clamp(texture(rough_tex, UV).r * rough_scale, 0.06, 1.0);
	if (has_normal) {
		NORMAL_MAP = texture(normal_tex, UV).rgb;
	}
	vec3 wn = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	float ndl = dot(wn, normalize(sun_dir));
	float night = emit_always ? 1.0 : smoothstep(0.04, -0.14, ndl);
	EMISSION = texture(emit_tex, UV).rgb * emit_energy * night;
}
