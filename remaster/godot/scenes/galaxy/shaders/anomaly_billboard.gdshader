// Web (Compatibility) anomaly: a camera-facing cloud quad, the declared fallback for anomaly_volume.gdshader.
// Same parameters; 4-octave noise; no loops beyond the noise.
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform vec4 tint : source_color = vec4(0.55, 0.35, 0.9, 1.0);
uniform float seed = 0.0;
uniform float intensity = 0.8;
uniform float swirl = 0.0;
uniform float crackle = 0.0;
uniform float disc = 0.0;
uniform float core = 0.0;
uniform float opacity = 1.0;

void vertex() {
	// Billboard: keep the node's position and scale, face the camera.
	float s = length(MODEL_MATRIX[0].xyz);
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * s, INV_VIEW_MATRIX[1] * s, INV_VIEW_MATRIX[2] * s, MODEL_MATRIX[3]);
}

void fragment() {
	vec2 c = UV - 0.5;
	float r = length(c);
	if (swirl > 0.0) {
		float ang = atan(c.y, c.x);
		ang += swirl * (0.6 - r) * 6.0 + TIME * swirl * 0.8;
		c = vec2(cos(ang), sin(ang)) * r;
	}
	vec2 p = c * 3.2 + vec2(fract(seed * 0.137) * 40.0, fract(seed * 0.291) * 40.0);
	float n = g_fbm2(p + vec2(TIME * 0.03, -TIME * 0.02));
	float n2 = g_fbm2(p * 2.0 - vec2(TIME * 0.05));
	float mask = smoothstep(0.5, 0.12, r);
	float a = smoothstep(0.30, 0.90, n + 0.25 * n2) * mask * intensity;
	vec3 col = mix(tint.rgb * 0.7, tint.rgb * 1.4 + 0.15, n2);
	if (disc > 0.0) {
		float ring = smoothstep(0.06, 0.16, r) * smoothstep(0.42, 0.2, r);
		a = max(a, ring * disc * 0.9);
		col += tint.rgb * ring * disc * 1.5;
	}
	if (crackle > 0.0) {
		float ridge = abs(g_fbm2(p * 1.6 + vec2(TIME * 0.35, -TIME * 0.2)) - 0.5);
		float w = max(fwidth(ridge) * 1.5, 0.012);
		float bolt = (1.0 - smoothstep(0.0, w * 2.0, ridge)) * crackle * mask * (0.55 + 0.45 * sin(TIME * 9.0 + n2 * 12.0));
		col += vec3(0.8, 0.9, 1.0) * bolt;
		a += bolt * 0.6;
	}
	if (core > 0.0) {
		float hole = 1.0 - smoothstep(0.05 * core, 0.07 * core, r);
		col = mix(col, vec3(0.0), hole);
		a = max(a, hole);
	}
	ALBEDO = col;
	ALPHA = clamp(a * opacity, 0.0, 1.0);
}
