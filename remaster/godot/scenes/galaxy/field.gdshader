shader_type canvas_item;
// Soft per-sector fields (territory tint, supply range, fog of war). The texture holds one
// pixel per sector and is bilinearly sampled; a noise term breaks up the edges so borders read
// as soft, organic regions instead of squares.
//   mode 0: territory / supply (rgb colour, a = strength) with a brighter border line
//   mode 1: fog of war (a = darkness), cloudy noise

uniform int mode = 0;
uniform sampler2D field_tex : filter_linear, repeat_disable;
uniform float fill = 0.16;
uniform float edge = 0.55;
uniform float edge_width = 0.03;
uniform float noise_amount = 0.06;
uniform vec2 cells = vec2(16.0, 12.0);
uniform vec4 fog_color : source_color = vec4(0.035, 0.045, 0.085, 1.0);

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
	float v = 0.0; float a = 0.5;
	for (int i = 0; i < 4; i++) { v += a * noise(p); p *= 2.0; a *= 0.5; }
	return v;
}

// Bicubic B-spline sample for smoother blobs than plain bilinear.
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
	vec2 tex_size = cells;
	vec2 inv = 1.0 / tex_size;
	uv = uv * tex_size - 0.5;
	vec2 fxy = fract(uv);
	uv -= fxy;
	vec4 xc = cubic(fxy.x);
	vec4 yc = cubic(fxy.y);
	vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;
	vec4 s = vec4(xc.xz + xc.yw, yc.xz + yc.yw);
	vec4 off = c + vec4(xc.yw, yc.yw) / s;
	off *= inv.xxyy;
	vec4 s0 = texture(field_tex, off.xz);
	vec4 s1 = texture(field_tex, off.yz);
	vec4 s2 = texture(field_tex, off.xw);
	vec4 s3 = texture(field_tex, off.yw);
	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);
	return mix(mix(s3, s2, sx), mix(s1, s0, sx), sy);
}

void fragment() {
	vec2 p = UV * cells;
	float n = fbm(p * 1.3 + TIME * 0.05) - 0.5;
	vec4 v = smooth_sample(UV);
	if (mode == 1) {
		float d = clamp(v.a + n * noise_amount * 2.0, 0.0, 1.0);
		float cloud = fbm(p * 0.8 - TIME * 0.02);
		float wisps = fbm(p * 2.3 + vec2(TIME * 0.03, 0.0));
		vec3 col = fog_color.rgb + vec3(0.06, 0.08, 0.16) * cloud + vec3(0.05, 0.06, 0.12) * smoothstep(0.55, 0.9, wisps);
		COLOR = vec4(col, d);
	} else {
		float a = v.a + n * noise_amount;
		float inside = smoothstep(0.42, 0.52, a);
		float line = smoothstep(0.5 - edge_width, 0.5, a) * (1.0 - smoothstep(0.5, 0.5 + edge_width, a));
		vec3 col = v.a > 0.001 ? v.rgb / max(v.a, 0.001) : vec3(0.0);
		col = clamp(col, 0.0, 1.0);
		COLOR = vec4(col, inside * fill + line * edge);
	}
}
