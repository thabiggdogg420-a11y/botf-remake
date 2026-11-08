// Per-sector fields on the galactic plane (territory per empire, supply range, fog of war). One texel per sector,
// sampled with a B-spline so borders are soft regions with a brighter edge line. Port of field.gdshader to 3D.
//   mode 0: territory / supply (rgb colour, a = strength)    mode 1: fog of war (a = darkness)
// Forward+ and Compatibility: unshaded, 4-octave noise, constant loops.
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform int mode = 0;
uniform sampler2D field_tex : filter_linear, repeat_disable;
uniform float fill = 0.16;
uniform float edge = 0.55;
uniform float edge_width = 0.03;
uniform float noise_amount = 0.06;
uniform float opacity = 1.0;
uniform vec2 cells = vec2(16.0, 12.0);
uniform vec4 fog_color : source_color = vec4(0.008, 0.01, 0.02, 1.0);

vec4 cubic(float v) {
	vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4(x, y, z, w) * (1.0 / 6.0);
}
vec4 smooth_sample(vec2 uv) {
	vec2 inv = 1.0 / cells;
	uv = uv * cells - 0.5;
	vec2 fxy = fract(uv);
	uv -= fxy;
	vec4 xc = cubic(fxy.x);
	vec4 yc = cubic(fxy.y);
	vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;
	vec4 s = vec4(xc.xz + xc.yw, yc.xz + yc.yw);
	vec4 off = (c + vec4(xc.yw, yc.yw) / s) * inv.xxyy;
	vec4 s0 = texture(field_tex, off.xz);
	vec4 s1 = texture(field_tex, off.yz);
	vec4 s2 = texture(field_tex, off.xw);
	vec4 s3 = texture(field_tex, off.yw);
	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);
	return mix(mix(s3, s2, sx), mix(s1, s0, sx), sy);
}

void fragment() {
	// The plane mesh is one sector larger than the grid on each side so borders fade out softly.
	vec2 uv = (UV * (cells + 2.0) - 1.0) / cells;
	vec2 p = uv * cells;
	float n = g_fbm2(p * 1.3 + TIME * 0.05) - 0.5;
	vec4 v = smooth_sample(clamp(uv, vec2(0.0), vec2(1.0)));
	float inside_grid = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
	if (mode == 1) {
		float d = clamp(v.a + n * noise_amount * 2.0, 0.0, 1.0);
		float cloud = g_fbm2(p * 0.8 - TIME * 0.02);
		ALBEDO = fog_color.rgb + vec3(0.012, 0.016, 0.035) * cloud;
		ALPHA = d * opacity * inside_grid;
	} else {
		float a = v.a * inside_grid + n * noise_amount;
		float inside = smoothstep(0.42, 0.52, a);
		// Border line about 1.5 px wide at every zoom (screen derivative), never wider than edge_width.
		float w = clamp(fwidth(a) * 1.5, 0.002, edge_width);
		float line = 1.0 - smoothstep(0.0, w, abs(a - 0.5));
		vec3 col = v.a > 0.001 ? clamp(v.rgb / max(v.a, 0.001), 0.0, 1.0) : vec3(0.0);
		// Soft volume read: the fill thickens toward the region's core.
		float core = smoothstep(0.5, 1.0, v.a);
		// The 3D pass blends in linear space: the fill is kept dark so it tints, never washes.
		ALBEDO = col * mix(0.45, 1.6, line);
		ALPHA = clamp((inside * fill * (1.0 + core * 0.8) + line * edge) * opacity, 0.0, 1.0);
	}
}
