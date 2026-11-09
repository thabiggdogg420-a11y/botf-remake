// Camera-facing additive flare: soft core plus optional star spikes. Used for torpedo heads, pulse bolts, muzzle
// flashes, impact glare and explosion cores. Scale the node to size it.
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 flare_color : source_color = vec4(1.0, 0.4, 0.1, 1.0);
uniform float flare_intensity = 1.0;
uniform float flare_spikes = 0.0;
uniform float flare_rotation = 0.0;
uniform float flare_stretch = 1.0;

void vertex() {
	// Billboard, keeping the node's scale.
	vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * scale.x, INV_VIEW_MATRIX[1] * scale.y,
		INV_VIEW_MATRIX[2] * scale.z, MODEL_MATRIX[3]);
	MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
}

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float c = cos(flare_rotation);
	float s = sin(flare_rotation);
	p = mat2(vec2(c, -s), vec2(s, c)) * p;
	p.x /= flare_stretch;
	float r = length(p);
	float core = exp(-r * r * 30.0);
	float glow = exp(-r * 4.0) * (1.0 - smoothstep(0.7, 1.0, r));
	float spikes = 0.0;
	if (flare_spikes > 0.0) {
		float a = atan(p.y, p.x);
		spikes = pow(abs(cos(a * 2.0)), 60.0) * exp(-r * 3.0) + pow(abs(cos(a * 2.0 + 0.785)), 90.0) * exp(-r * 5.0) * 0.5;
		spikes *= flare_spikes * (1.0 - smoothstep(0.8, 1.0, r));
	}
	vec3 col = flare_color.rgb * (glow * 1.5 + spikes * 2.0) + vec3(1.0, 0.98, 0.94) * core * 2.5;
	float near_fade = smoothstep(1.0, 8.0, -VERTEX.z); // no flare blinds a camera parked beside a hull
	ALBEDO = col * flare_intensity * near_fade;
	ALPHA = clamp((glow + core) * flare_intensity * near_fade, 0.0, 1.0);
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added (ALBEDO x ALPHA under blend_add) with an sRGB-like curve, as in shield.gdshader (M1 battleplay).
	ALBEDO = pow(ALBEDO * ALPHA, vec3(1.0 / 2.2));
	ALPHA = 1.0;
#endif
}
