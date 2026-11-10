// Textured hero hull (a local glTF model from blender_hero_ships.py --gltf): Unreal's M_ShipHero.
// BC x tint, tangent normal, ORM (occlusion R, roughness G, metallic B) with the importer's scales, a roughness floor
// against paint sparkle, and the emissive windows / engines x a strength already dimmed for lit coverage.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform sampler2D bc_tex : source_color, hint_default_white, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform bool has_normal = false;
uniform sampler2D orm_tex : hint_default_white, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D emissive_tex : source_color, hint_default_black, filter_linear_mipmap_anisotropic, repeat_enable;
uniform vec3 tint = vec3(1.0);          // linear, as the Unreal Tint parameter
uniform float metallic_scale = 0.35;
uniform float roughness_scale = 1.0;
uniform float roughness_min = 0.32;
uniform float emissive_strength = 1.5;
uniform float ao_light_affect = 0.4;

void fragment() {
	ALBEDO = texture(bc_tex, UV).rgb * tint;
	vec3 orm = texture(orm_tex, UV).rgb;
	AO = orm.r;
	AO_LIGHT_AFFECT = ao_light_affect;
	ROUGHNESS = max(orm.g * roughness_scale, roughness_min);
	METALLIC = orm.b * metallic_scale;
	if (has_normal) {
		NORMAL_MAP = texture(normal_tex, UV).rgb;
	}
	EMISSION = texture(emissive_tex, UV).rgb * emissive_strength;
}
