// Expanding shockwave ring on a flat quad (local XZ plane). Scale the node for the radius. ring_billboard = 1 turns a
// QuadMesh (local XY) to face the camera: the torpedo impact ring (M1 weaponfx), which reads from any angle.
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 ring_color : source_color = vec4(1.0, 0.7, 0.35, 1.0);
uniform float ring_age = 0.0;
uniform float ring_billboard = 0.0;
uniform float ring_intensity = 1.0;

void vertex() {
	if (ring_billboard > 0.5) {
		vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
		MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * scale.x, INV_VIEW_MATRIX[1] * scale.y,
			INV_VIEW_MATRIX[2] * scale.z, MODEL_MATRIX[3]);
		MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
	}
}

float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float r = length(p);
	float a = atan(p.y, p.x);
	float wobble = (1.0 - ring_billboard) * (0.004 * sin(a * 9.0 + ring_age * 5.0) + 0.002 * sin(a * 23.0)); // torpedo ring: clean, as the show
	float ring = exp(-pow((r - 0.8 - wobble) / (mix(0.025 + 0.06 * ring_age, 0.018 + 0.05 * ring_age, ring_billboard)), 2.0));
	float inner = exp(-r * 3.0) * 0.3 * (1.0 - ring_age);
	float fade = pow(1.0 - ring_age, 1.5);
	ALBEDO = ring_color.rgb * (ring * 1.4 + inner) * fade * ring_intensity;
	ALPHA = clamp((ring + inner) * fade, 0.0, 1.0);
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added (ALBEDO x ALPHA under blend_add) with an sRGB-like curve, as in shield.gdshader (M1 battleplay).
	ALBEDO = pow(ALBEDO * ALPHA, vec3(1.0 / 2.2));
	ALPHA = 1.0;
#endif
}
