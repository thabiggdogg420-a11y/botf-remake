// Kill fireball lobe (DS9 "Sacrifice of Angels", 1161-1163 s): one camera-facing billboard of a lumpy, volumetric
// orange-yellow fire cloud with a ragged dark smoke rim. A kill layers several of these round the hull (BattleFx
// _kill_fire). Procedural value-noise fbm only (no textures, no HDR, no screen reads): the same in Forward+ and in the
// web build's Compatibility renderer. Plain uniforms per node (BattleFx._fxp).
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, shadows_disabled;

uniform float age = 0.0;      // 0 just lit .. 1 burnt out
uniform float seed = 0.0;
uniform float heat = 1.0;     // 1 = inner lobe (yellow core), 0 = outer lobe (deep orange)
uniform float alpha_gain = 1.0;

varying float v_near;

void vertex() {
	vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * scale.x, INV_VIEW_MATRIX[1] * scale.y,
		INV_VIEW_MATRIX[2] * scale.z, MODEL_MATRIX[3]);
	MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int k = 0; k < 4; k++) {
		v += a * vnoise(p);
		p = p * 2.03 + vec2(1.7, 9.2);
		a *= 0.5;
	}
	return v;
}

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float r = length(p);
	vec2 off = vec2(seed * 13.1, seed * 7.7);
	// Billows: the noise field churns outward as the fire ages.
	vec2 q = p * (1.6 - 0.35 * age) + off;
	float n = fbm(q + vec2(age * 0.9, -age * 0.6));
	float n2 = fbm(q * 2.3 - vec2(age * 1.3, age * 0.4) + 3.1);
	// Lumpy silhouette: the lobe's edge radius wanders with the noise (cauliflower lumps, not a disc).
	float edge = 0.55 + 0.42 * n - 0.12 * age;
	float body = 1.0 - smoothstep(edge - 0.18, edge, r);
	float density = body * (0.55 + 0.75 * n2);
	// Temperature: hot in the middle and early, cooling to deep orange, then brown-black smoke at the rim and late.
	float temp = clamp((1.0 - r / max(edge, 0.05)) * (0.7 + 0.6 * n2) * (1.15 - age) * (0.55 + 0.5 * heat), 0.0, 1.0);
	vec3 smoke = vec3(0.08, 0.05, 0.035);
	vec3 ember = vec3(0.55, 0.16, 0.03);
	vec3 orange = vec3(1.0, 0.45, 0.08);
	vec3 yellow = vec3(1.0, 0.78, 0.32);
	vec3 col = mix(smoke, ember, smoothstep(0.02, 0.25, temp));
	col = mix(col, orange, smoothstep(0.22, 0.5, temp));
	col = mix(col, yellow, smoothstep(0.55, 0.85, temp));
	// Desktop (HDR): the hottest part glows a little above 1 for the bloom; never a white disc.
#if CURRENT_RENDERER != RENDERER_COMPATIBILITY
	col *= 1.0 + 0.8 * smoothstep(0.6, 0.95, temp);
#endif
	float fade_in = smoothstep(0.0, 0.06, age);
	float fade_out = 1.0 - smoothstep(0.55, 1.0, age);
	float near_fade = smoothstep(1.0, 8.0, -VERTEX.z);
	ALBEDO = col;
	ALPHA = clamp(density * 1.3, 0.0, 1.0) * fade_in * fade_out * near_fade * alpha_gain;
}
