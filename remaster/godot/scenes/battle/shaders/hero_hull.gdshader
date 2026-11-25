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
// Hull-look lane (25 Nov 2026, Simon: "little white specs all over them"). The window maps are a few texels per window, and
// the lit rows were sampled at full sharpness at a grazing angle, so each window landed on one pixel one frame and on none
// the next: a white sparkle. The emission is now sampled `emissive_bias` mips softer (a window reads as a soft warm point,
// not a texel), and capped per channel so it stays warm and below the glow threshold instead of clipping to white.
uniform float emissive_bias = 1.0;
uniform float emissive_cap = 2.4;
uniform float specular_aa = 1.0;        // 0 = off; widens roughness where the normal changes faster than a pixel

// Title fade-in (hull-look lane): 1 = solid. A dither dissolve, not alpha, so it sorts like an opaque and works in every renderer.
uniform float title_fade = 1.0;

void fragment() {
	if (title_fade < 0.999) {
		float d = fract(52.9829189 * fract(dot(FRAGCOORD.xy, vec2(0.06711056, 0.00583715))));
		if (d > title_fade) {
			discard;
		}
	}
	ALBEDO = texture(bc_tex, UV).rgb * tint;
	vec3 orm = texture(orm_tex, UV).rgb;
	AO = orm.r;
	AO_LIGHT_AFFECT = ao_light_affect;
	float rough = max(orm.g * roughness_scale, roughness_min);
	METALLIC = orm.b * metallic_scale;
	float variance = 0.0;
	if (has_normal) {
		vec3 nm = texture(normal_tex, UV).rgb;
		NORMAL_MAP = nm;
		// Toksvig: the mip-averaged normal gets shorter where the map varies faster than the pixel grid, i.e. where it sparkles.
		float len = clamp(length(nm * 2.0 - 1.0), 0.45, 1.0);
		variance += (1.0 - len) / len * 0.35;
	}
	// Geometric specular AA (Kaplanyan): the interpolated normal's own change across the pixel.
	vec3 ndx = dFdx(NORMAL);
	vec3 ndy = dFdy(NORMAL);
	variance += 0.5 * (dot(ndx, ndx) + dot(ndy, ndy));
	ROUGHNESS = sqrt(clamp(rough * rough + specular_aa * min(variance, 0.25), 0.0, 1.0));
	vec3 em = texture(emissive_tex, UV, emissive_bias).rgb * emissive_strength;
	float cap = emissive_cap;
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// The web build has no HDR range: anything over 1.0 clips to white, so the window rows were white dots and the bridge dome a
	// white blob. Capped under 1 and warmed a little, a lit window reads as light, not as a hole in the hull.
	cap = 0.8;
	em *= vec3(1.0, 0.93, 0.8);
#endif
	float em_peak = max(em.r, max(em.g, em.b));
	EMISSION = em * min(1.0, cap / max(em_peak, 1e-4));
}
